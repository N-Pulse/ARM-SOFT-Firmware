#include "board_protocol.h"
#include <limits.h>
#include <string.h>

void BL_Put32(uint8_t *p, uint32_t v)
{ for (unsigned i=0;i<4;++i) p[i]=(uint8_t)(v>>(8U*i)); }
uint32_t BL_Get32(const uint8_t *p)
{ return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static void put64(uint8_t *p,uint64_t v) { BL_Put32(p,(uint32_t)v); BL_Put32(p+4,(uint32_t)(v>>32)); }
static uint64_t get64(const uint8_t *p) { return BL_Get32(p)|(uint64_t)BL_Get32(p+4)<<32; }
static uint32_t crc(const uint8_t *p,size_t n)
{
    static const uint32_t nibble[16]={0x00000000U,0x1DB71064U,0x3B6E20C8U,0x26D930ACU,
        0x76DC4190U,0x6B6B51F4U,0x4DB26158U,0x5005713CU,0xEDB88320U,0xF00F9344U,
        0xD6D6A3E8U,0xCB61B38CU,0x9B64C2B0U,0x86D3D2D4U,0xA00AE278U,0xBDBDF21CU};
    uint32_t c=UINT32_MAX;
    /* Two bounded lookup steps per byte keep IRQ-masked parser work short.
     * Worst-case latency still requires measurement on the RET6. */
    for(size_t i=0;i<n;++i) { c^=p[i]; c=(c>>4)^nibble[c&15U]; c=(c>>4)^nibble[c&15U]; }
    return ~c;
}
size_t BL_Encode(uint8_t type,uint8_t role,uint64_t source,uint64_t destination,
                 uint32_t sequence,uint32_t epoch,uint32_t peer_epoch,
                 const uint8_t *payload,size_t length,uint8_t out[BL_WIRE_MAX])
{
    if(length>BL_PAYLOAD || (length && !payload) || !out) return 0;
    uint8_t raw[84]; raw[0]=BL_VERSION; raw[1]=type; raw[2]=role; raw[3]=(uint8_t)length;
    put64(raw+4,source); put64(raw+12,destination); BL_Put32(raw+20,sequence);
    BL_Put32(raw+24,epoch); BL_Put32(raw+28,peer_epoch);
    if(length) memcpy(raw+32,payload,length);
    BL_Put32(raw+32+length,crc(raw,32+length));
    size_t code_at=0,write=1; uint8_t code=1;
    for(size_t i=0;i<36+length;++i) {
        if(raw[i]) { out[write++]=raw[i]; ++code; }
        else { out[code_at]=code; code_at=write++; code=1; }
    }
    out[code_at]=code; out[write++]=0; return write;
}
bool BL_Local(uint8_t role,uint8_t motor)
{
    if(motor>=8 || (role!=BL_MASTER && role!=BL_SLAVE)) return false;
    bool master=motor==4 || motor==5 || motor==6;
    return role==BL_MASTER ? master : !master;
}
static void discard(board_protocol_t *s)
{
    s->authorized=s->peer_authorized=false; s->count=s->head=0;
    memset(s->pending,0,sizeof(s->pending));
    memset(s->have_feedback,0,sizeof(s->have_feedback));
}
static bool authority(const board_protocol_t *s)
{
    if(s->authorized || s->peer_authorized) return true;
    for(unsigned i=0;i<BL_QUEUE;++i) if(s->pending[i].used) return true;
    return false;
}
static void stop(board_protocol_t *s)
{
    bool notify=authority(s);
    discard(s); s->stop_pending|=notify; ++s->stops;
    if(s->hooks.stop) s->hooks.stop(s->hooks.context);
}
bool BL_Init(board_protocol_t *s,uint8_t role,uint64_t session,uint32_t now,bl_hooks_t hooks)
{
    memset(s,0,sizeof(*s)); s->role=role; s->session=session; s->hooks=hooks;
    s->initialized=session && (role==BL_MASTER || role==BL_SLAVE);
    s->hello_pending=s->initialized; s->beat_ms=now-BL_HEARTBEAT_MS;
    return s->initialized;
}
void BL_SetLocalState(board_protocol_t *s,uint32_t epoch,uint32_t faults,uint32_t now)
{
    if(epoch!=s->epoch || faults!=s->faults) {
        bool wrap=epoch<s->epoch;
        discard(s);
        if(faults) s->stop_pending=true;
        s->epoch=epoch; s->faults=faults;
        if(wrap) BL_Fail(s,now);
    }
}
void BL_Fail(board_protocol_t *s,uint32_t now)
{
    stop(s);
    if(!s->initialized || s->permanent_fault) return;
    if(!s->dropped) {
        ++s->restarts;
        if(s->session==UINT64_MAX) { s->permanent_fault=true; return; }
        ++s->session; /* Never reuse an old channel session after loss. */
    }
    s->dropped=true; s->established=s->have_heartbeat=false; s->peer=0; s->have_peer_epoch=false;
    s->rx_seq=s->tx_seq=s->peer_ack=s->command_id=s->command_rx_id=0; s->encoded_length=0;
    s->hello_pending=true; s->beat_ms=now;
}
bool BL_Fresh(const board_protocol_t *s,uint32_t now)
{
    return s->initialized && !s->permanent_fault && s->established && s->have_heartbeat &&
        (uint32_t)(now-s->rx_ms)<=BL_TIMEOUT_MS && (uint32_t)(now-s->ack_ms)<=BL_TIMEOUT_MS;
}
bool BL_Ready(const board_protocol_t *s,uint32_t now)
{ return BL_Fresh(s,now) && s->authorized && s->peer_authorized && !s->faults && !s->peer_faults; }
bool BL_Authorize(board_protocol_t *s,uint32_t now)
{
    if(!BL_Fresh(s,now) || s->faults || s->peer_faults || !s->have_peer_epoch) return false;
    s->authorized=true; s->beat_ms=now-BL_HEARTBEAT_MS; return true;
}
void BL_Poll(board_protocol_t *s,uint32_t now)
{
    if(s->encoded_length && (uint32_t)(now-s->frame_ms)>BL_FRAME_MS) {
        s->encoded_length=0; ++s->malformed;
    }
    if(s->established && ((uint32_t)(now-s->rx_ms)>BL_TIMEOUT_MS ||
                         (uint32_t)(now-s->ack_ms)>BL_TIMEOUT_MS)) { BL_Fail(s,now); return; }
    for(unsigned i=0;i<BL_QUEUE;++i) if(s->pending[i].used &&
        (uint32_t)(now-s->pending[i].ms)>BL_COMMAND_MS) { BL_Fail(s,now); return; }
}
static bool offer(board_protocol_t *s,uint8_t type,const uint8_t *p,uint8_t n,uint32_t now)
{
    if(s->count==BL_QUEUE) { ++s->overflows; BL_Fail(s,now); return false; }
    bl_message_t *m=&s->queue[(s->head+s->count)%BL_QUEUE];
    *m=(bl_message_t){.type=type,.length=n,.epoch=s->epoch,.peer_epoch=s->peer_epoch,
                      .offered_ms=now,.peer=s->peer};
    memcpy(m->data,p,n); ++s->count; return true;
}
bool BL_Target(board_protocol_t *s,uint8_t id,int32_t mrad,uint8_t speed,uint32_t epoch,uint32_t now)
{
    if(s->role!=BL_MASTER || BL_Local(s->role,id) || id>=8 || !speed || speed>100 ||
       mrad<0 || mrad>1571 || epoch!=s->epoch || !BL_Ready(s,now)) { ++s->rejected; return false; }
    unsigned slot=BL_QUEUE;
    for(unsigned i=0;i<BL_QUEUE;++i) if(!s->pending[i].used) { slot=i; break; }
    if(slot==BL_QUEUE || s->command_id==UINT32_MAX) { BL_Fail(s,now); return false; }
    uint8_t p[10]; BL_Put32(p,++s->command_id); p[4]=id; p[5]=speed; BL_Put32(p+6,(uint32_t)mrad);
    if(!offer(s,BL_TARGET,p,sizeof(p),now)) return false;
    s->pending[slot]=(bl_pending_t){s->command_id,now,id,true}; return true;
}
bool BL_OfferFeedback(board_protocol_t *s,const bl_feedback_t *f,uint32_t now)
{
    if(!f || !BL_Local(s->role,f->id) || !BL_Fresh(s,now)) return false;
    /* Telemetry may be skipped before filling the command/ACK queue. */
    if(s->count>=BL_QUEUE-2) return false;
    uint8_t p[36]={f->id,f->flags,0,0}; BL_Put32(p+4,f->age_ms);
    BL_Put32(p+8,(uint32_t)f->position_mrad); BL_Put32(p+12,(uint32_t)f->velocity_mrad_s);
    BL_Put32(p+16,(uint32_t)f->target_mrad); BL_Put32(p+20,(uint32_t)f->pwm_milli);
    put64(p+24,(uint64_t)f->raw_count); BL_Put32(p+32,f->faults);
    return offer(s,BL_FEEDBACK,p,sizeof(p),now);
}
bool BL_GetFeedback(const board_protocol_t *s,uint8_t id,uint32_t now,bl_feedback_t *out)
{
    if(!out || id>=8 || BL_Local(s->role,id)) return false;
    *out=(bl_feedback_t){.id=id,.age_ms=UINT32_MAX};
    if(!s->have_feedback[id]) return false;
    *out=s->feedback[id]; uint32_t elapsed=now-s->feedback_ms[id];
    out->age_ms=out->age_ms>UINT32_MAX-elapsed ? UINT32_MAX : out->age_ms+elapsed;
    if(!BL_Fresh(s,now) || out->age_ms>BL_FEEDBACK_MS) out->flags&=~(BL_VALID|BL_RAW_VALID);
    if(!(out->flags&BL_CALIBRATED)) out->flags&=~BL_VALID;
    return true;
}
static void receive(board_protocol_t *s,const uint8_t *p,size_t n,uint32_t ms,uint32_t now)
{
    if(n<36 || p[3]>BL_PAYLOAD || n!=36U+p[3] || crc(p,n-4)!=BL_Get32(p+n-4)) { ++s->malformed; return; }
    uint8_t type=p[1],len=p[3]; uint64_t source=get64(p+4),destination=get64(p+12);
    uint32_t seq=BL_Get32(p+20),epoch=BL_Get32(p+24),mine=BL_Get32(p+28);
    if(p[0]!=BL_VERSION || p[2]==s->role || !BL_Local(p[2],p[2]==BL_MASTER?4:0) || !source || !seq) {
        ++s->rejected; BL_Fail(s,now); return;
    }
    if((uint32_t)(now-ms)>BL_FRAME_MS) { ++s->rejected; return; }
    if(type==BL_HELLO && len==0) {
        if(destination && destination!=s->session) { ++s->rejected; return; }
        if(s->established && authority(s)) stop(s); /* Unexpected link initialization during coordinated control. */
        if(source!=s->peer) {
            stop(s); s->peer=source; s->rx_seq=0; s->command_rx_id=0;
            s->established=s->have_heartbeat=false; s->have_peer_epoch=false;
            s->hello_pending=true;
        }
        if(destination==s->session && !s->established) {
            s->established=true; s->dropped=false; s->rx_ms=ms; s->ack_ms=now;
            s->hello_pending=true;
        }
    } else if(!s->established || source!=s->peer || destination!=s->session) { ++s->rejected; return; }
    /* STOP only removes authority; even a repeated STOP must be safe. */
    if(type==BL_STOP && len==4) { stop(s); s->peer_faults=BL_Get32(p+32); }
    if(seq<=s->rx_seq) { ++s->duplicates; return; }
    s->rx_seq=seq;
    if(!s->have_peer_epoch || epoch!=s->peer_epoch) {
        if(s->have_peer_epoch) {
            if(authority(s)) stop(s);
            else discard(s); /* A disarmed peer's explicit recovery is not a new fault. */
        }
        s->peer_epoch=epoch; s->have_peer_epoch=true;
    }
    const uint8_t *d=p+32;
    if(type==BL_HELLO && !len) return;
    if(type==BL_HEARTBEAT && len==9) {
        uint32_t ack=BL_Get32(d);
        if(ack>s->tx_seq || d[8]>1) { ++s->rejected; BL_Fail(s,now); return; }
        s->rx_ms=ms;
        if(ack>s->peer_ack) { s->peer_ack=ack; s->ack_ms=ms; }
        uint32_t faults=BL_Get32(d+4);
        if(faults && !s->peer_faults) stop(s);
        s->peer_faults=faults;
        s->peer_authorized=d[8] && mine==s->epoch && !faults;
        s->have_heartbeat=true; /* HELLO alone never proves the remote safety state. */
    } else if(type==BL_TARGET && len==10) {
        uint8_t id=d[4],status=BL_FORBIDDEN; uint32_t command=BL_Get32(d);
        int32_t target=(int32_t)BL_Get32(d+6);
        if(!command || command<=s->command_rx_id || id>=8 || !d[5] || d[5]>100 || target<0 || target>1571) status=BL_INVALID;
        else if(s->role==BL_SLAVE && BL_Local(s->role,id) && mine==s->epoch &&
                BL_Ready(s,now) && s->hooks.target)
            { s->command_rx_id=command; status=s->hooks.target(s->hooks.context,id,target,d[5],s->epoch); }
        else ++s->rejected;
        uint8_t reply[6]; BL_Put32(reply,command); reply[4]=id; reply[5]=status;
        if(status!=BL_ACCEPTED) stop(s);
        offer(s,BL_ACK,reply,sizeof(reply),now);
    } else if(type==BL_ACK && len==6) {
        bool matched=false;
        for(unsigned i=0;i<BL_QUEUE;++i) if(s->pending[i].used && s->pending[i].id==BL_Get32(d) &&
            s->pending[i].motor==d[4] && mine==s->epoch) {
            matched=true; s->pending[i].used=false; s->accepted_id[d[4]]=BL_Get32(d); s->accepted_status[d[4]]=d[5];
            if(d[5]!=BL_ACCEPTED) stop(s);
            break;
        }
        if(!matched) ++s->rejected;
    } else if(type==BL_FEEDBACK && len==36) {
        uint8_t id=d[0];
        if(id>=8 || mine!=s->epoch || BL_Local(s->role,id) || !BL_Local(p[2],id) || d[2] || d[3] || (d[1]&~31U)) { ++s->rejected; return; }
        bl_feedback_t *f=&s->feedback[id];
        *f=(bl_feedback_t){.id=id,.flags=d[1],.age_ms=BL_Get32(d+4),.position_mrad=(int32_t)BL_Get32(d+8),
             .velocity_mrad_s=(int32_t)BL_Get32(d+12),.target_mrad=(int32_t)BL_Get32(d+16),
             .pwm_milli=(int32_t)BL_Get32(d+20),.raw_count=(int64_t)get64(d+24),.faults=BL_Get32(d+32)};
        s->feedback_ms[id]=ms; s->have_feedback[id]=true;
        if(f->faults && !s->peer_faults) stop(s);
        s->peer_faults=f->faults;
    } else if(type!=BL_STOP || len!=4) { ++s->rejected; BL_Fail(s,now); }
}
void BL_Feed(board_protocol_t *s,uint8_t byte,uint32_t ms,uint32_t now)
{
    if(!s->initialized || s->permanent_fault) return;
    if(s->encoded_length && (uint32_t)(ms-s->frame_ms)>BL_FRAME_MS) { s->encoded_length=0; ++s->malformed; }
    if(byte) {
        if(!s->encoded_length) s->frame_ms=ms;
        if(s->encoded_length<BL_WIRE_MAX-1) s->encoded[s->encoded_length++]=byte;
        else { s->encoded_length=0; ++s->malformed; BL_Fail(s,now); }
        return;
    }
    if(!s->encoded_length) return;
    uint8_t raw[84]; size_t read=0,write=0,n=s->encoded_length; s->encoded_length=0;
    while(read<n) {
        unsigned code=s->encoded[read++];
        if(!code || read+code-1>n || write+code-1>sizeof(raw)) { ++s->malformed; return; }
        for(unsigned i=1;i<code;++i) raw[write++]=s->encoded[read++];
        if(read<n && code!=255) { if(write==sizeof(raw)) { ++s->malformed; return; } raw[write++]=0; }
    }
    receive(s,raw,write,s->frame_ms,now);
}
size_t BL_Take(board_protocol_t *s,uint32_t now,uint8_t out[BL_WIRE_MAX])
{
    BL_Poll(s,now);
    if(!s->initialized || s->permanent_fault) return 0;
    if(s->tx_seq==UINT32_MAX) { BL_Fail(s,now); return 0; }
    uint8_t type,p[BL_PAYLOAD],length=0;
    uint32_t epoch=s->epoch,peer_epoch=s->peer_epoch; uint64_t peer=s->peer;
    if(s->hello_pending) { s->hello_pending=false; type=BL_HELLO; }
    else if(s->stop_pending && s->established) {
        s->stop_pending=false; type=BL_STOP; length=4; BL_Put32(p,s->faults);
    } else if((uint32_t)(now-s->beat_ms)>=BL_HEARTBEAT_MS) {
        s->beat_ms=now;
        if(!s->established) type=BL_HELLO;
        else { type=BL_HEARTBEAT; length=9; BL_Put32(p,s->rx_seq); BL_Put32(p+4,s->faults); p[8]=s->authorized; }
    } else if(s->count) {
        bl_message_t m=s->queue[s->head]; s->head=(s->head+1)%BL_QUEUE; --s->count;
        if(m.epoch!=s->epoch || m.peer_epoch!=s->peer_epoch || m.peer!=s->peer || !BL_Fresh(s,now)) return 0;
        if(m.type==BL_TARGET && (!BL_Ready(s,now) || (uint32_t)(now-m.offered_ms)>BL_COMMAND_QUEUE_MS)) { BL_Fail(s,now); return 0; }
        type=m.type; length=m.length; memcpy(p,m.data,length); epoch=m.epoch; peer_epoch=m.peer_epoch; peer=m.peer;
        if(type==BL_FEEDBACK) { uint32_t age=BL_Get32(p+4),elapsed=now-m.offered_ms; BL_Put32(p+4,age>UINT32_MAX-elapsed?UINT32_MAX:age+elapsed); }
    } else return 0;
    return BL_Encode(type,s->role,s->session,peer,++s->tx_seq,epoch,peer_epoch,p,length,out);
}
