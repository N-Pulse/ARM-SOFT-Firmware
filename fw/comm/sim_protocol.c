#include "sim_protocol.h"
#include <string.h>
#include <float.h>
#if FLT_RADIX != 2 || FLT_MANT_DIG != 24 || FLT_MAX_EXP != 128
#error SIM v2 requires IEEE binary32
#endif
uint16_t Sim_Read16(const uint8_t *p) { return (uint16_t)p[0] | ((uint16_t)p[1]<<8U); }
uint32_t Sim_Read32(const uint8_t *p) { return (uint32_t)Sim_Read16(p) | ((uint32_t)Sim_Read16(p+2)<<16U); }
uint64_t Sim_Read64(const uint8_t *p) { return (uint64_t)Sim_Read32(p) | ((uint64_t)Sim_Read32(p+4)<<32U); }
float Sim_ReadFloat(const uint8_t *p) { uint32_t bits=Sim_Read32(p); float f; memcpy(&f,&bits,4U); return f; }
void Sim_Write16(uint8_t *p,uint16_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8U); }
void Sim_Write32(uint8_t *p,uint32_t v) { Sim_Write16(p,(uint16_t)v); Sim_Write16(p+2,(uint16_t)(v>>16U)); }
void Sim_Write64(uint8_t *p,uint64_t v) { Sim_Write32(p,(uint32_t)v); Sim_Write32(p+4,(uint32_t)(v>>32U)); }
void Sim_WriteFloat(uint8_t *p,float f) { uint32_t bits; memcpy(&bits,&f,4U); Sim_Write32(p,bits); }
bool Sim_SequenceNewer(uint32_t a,uint32_t b) { uint32_t d=a-b; return d!=0U && d<0x80000000U; }
uint16_t Sim_Crc16(const uint8_t *p,size_t n)
{
    uint16_t crc=0xFFFFU;
    for (size_t i=0U;i<n;++i) {
        crc^=(uint16_t)p[i]<<8U;
        for(unsigned j=0U;j<8U;++j) crc=crc&0x8000U ? (uint16_t)((crc<<1U)^0x1021U) : (uint16_t)(crc<<1U);
    }
    return crc;
}
size_t Sim_Encode(uint8_t type,const uint8_t *payload,size_t n,uint8_t *out,size_t capacity)
{
    if (!out || n<SIM_COMMON_SIZE || n>SIM_PAYLOAD_MAX || !payload || capacity<n+7U) return 0U;
    out[0]=0xA5U; out[1]=0x5AU; out[2]=SIM_VERSION; out[3]=type; out[4]=(uint8_t)n;
    memcpy(out+5U,payload,n); Sim_Write16(out+5U+n,Sim_Crc16(out+2U,n+3U)); return n+7U;
}
void SimParser_Init(sim_parser_t *p) { memset(p,0,sizeof(*p)); }
void SimParser_Reset(sim_parser_t *p) { p->used=0U; }
void SimParser_Poll(sim_parser_t *p,uint32_t now)
{
    if(p->used && ((uint32_t)(now-p->last_ms)>SIM_INTERBYTE_TIMEOUT_MS ||
                   (uint32_t)(now-p->first_ms)>SIM_FRAME_TIMEOUT_MS)) { ++p->timeouts; p->used=0U; }
}
static void consume(sim_parser_t *p,size_t n)
{
    p->used-=n; memmove(p->bytes,p->bytes+n,p->used);
    memmove(p->byte_ms,p->byte_ms+n,p->used*sizeof(p->byte_ms[0]));
    if(p->used) p->first_ms=p->byte_ms[0];
}
void SimParser_Feed(sim_parser_t *p,uint8_t byte,uint32_t ms,uint32_t epoch,sim_frame_fn callback,void *context)
{
    SimParser_Poll(p,ms);
    if(p->used && p->epoch!=epoch) p->used=0U;
    if(!p->used) { p->epoch=epoch; p->first_ms=ms; }
    p->last_ms=ms;
    if(p->used>=sizeof(p->bytes)) p->used=0U;
    p->byte_ms[p->used]=ms; p->bytes[p->used++]=byte;
    while(p->used) {
        if(p->bytes[0]!=0xA5U) { consume(p,1U); continue; }
        if(p->used<2U) break;
        if(p->bytes[1]!=0x5AU) { consume(p,1U); continue; }
        if(p->used<5U) break;
        size_t n=p->bytes[4];
        if(p->bytes[2]!=SIM_VERSION || n<SIM_COMMON_SIZE || n>SIM_PAYLOAD_MAX) {
            ++p->invalid; consume(p,1U); continue;
        }
        if(p->used<n+7U) break;
        if(Sim_Read16(p->bytes+5U+n)!=Sim_Crc16(p->bytes+2U,n+3U)) {
            ++p->invalid; consume(p,1U); continue;
        }
        ++p->frames;
        /* Valid framing is consumed even if session/application rejects it. */
        callback(p->bytes[3],p->bytes+5U,n,p->last_ms,p->epoch,context);
        consume(p,n+7U);
    }
}
