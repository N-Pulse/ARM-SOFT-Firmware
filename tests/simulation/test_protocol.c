#include "sim_protocol.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned accepted;
static bool frame(uint8_t type,const uint8_t *p,size_t n,uint32_t ms,uint32_t epoch,void *ctx) {
    (void)ms;(void)epoch;(void)ctx; assert(type==SIM_HEARTBEAT && p && n==16U); ++accepted; return true;
}
static void feed(sim_parser_t *p,const uint8_t *f,size_t n,uint32_t ms) {
    for(size_t i=0;i<n;++i) SimParser_Feed(p,f[i],ms,1U,frame,NULL);
}
int main(void) {
    struct { uint32_t before; sim_parser_t parser; uint32_t after; } guard={.before=0x12345678U,.after=0x87654321U};
    sim_parser_t *p=&guard.parser; uint8_t payload[144]={1U},wire[160];
    SimParser_Init(p); size_t n=Sim_Encode(SIM_HEARTBEAT,payload,16U,wire,sizeof(wire)); assert(n==23U);
    feed(p,wire,n,UINT32_MAX-5U); assert(accepted==1U);
    feed(p,wire,10U,UINT32_MAX-2U); feed(p,wire+10U,n-10U,3U); assert(accepted==2U);
    SimParser_Init(p); feed(p,wire,10U,0U); feed(p,wire+10U,n-10U,16U); assert(accepted==2U && p->timeouts==1U);
    SimParser_Init(p);
    for(unsigned i=0;i<n;++i) SimParser_Feed(p,wire[i],i*10U,1U,frame,NULL);
    assert(accepted==2U && p->timeouts>0U); /* interbyte <15, total >50 */
    SimParser_Init(p); feed(p,wire,10U,1U);
    for(size_t i=10U;i<n;++i) SimParser_Feed(p,wire[i],2U,2U,frame,NULL);
    assert(accepted==2U); /* epoch changed midway */
    for(unsigned length=0U;length<256U;++length) {
        SimParser_Init(p); uint8_t header[5]={0xA5U,0x5AU,2U,SIM_HEARTBEAT,(uint8_t)length};
        feed(p,header,sizeof(header),0U);
        for(unsigned j=0;j<length+2U;++j) { uint8_t zero=0U; feed(p,&zero,1U,0U); }
        feed(p,wire,n,1U); assert(p->used<=SIM_FRAME_MAX);
        assert(guard.before==0x12345678U && guard.after==0x87654321U);
    }
    uint32_t random=1U; SimParser_Init(p);
    for(unsigned i=0;i<100000U;++i) {
        random=random*1664525U+1013904223U;
        SimParser_Feed(p,(uint8_t)(random>>24U),i/100U,1U,frame,NULL);
        assert(p->used<=SIM_FRAME_MAX && guard.before==0x12345678U && guard.after==0x87654321U);
    }
    for(unsigned length=0;length<=145U;++length) for(unsigned cap=0;cap<=152U;++cap) {
        memset(wire,0xCC,sizeof(wire));
        size_t size=Sim_Encode(SIM_HEARTBEAT,payload,length,wire,cap);
        assert((size!=0U)==(length>=16U && length<=144U && cap>=length+7U));
        assert(wire[cap]==0xCCU);
    }
    assert(Sim_SequenceNewer(0U,UINT32_MAX)); assert(Sim_SequenceNewer(1U,UINT32_MAX));
    assert(!Sim_SequenceNewer(UINT32_MAX,0U)); assert(!Sim_SequenceNewer(1U,1U));
    assert(!Sim_SequenceNewer(0x80000000U,0U));
    puts("PASS production SIM parser: all lengths, capacity matrix, 100000 noisy bytes, epoch, interbyte/total deadlines, HAL time and sequence wraps");
    return 0;
}
