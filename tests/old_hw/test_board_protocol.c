#include "board_protocol.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    board_protocol_t link;
    uint32_t now,epoch,faults,targets,stops;
    uint8_t motor;
    bool feedback_valid,enabled;
} board_t;
static board_t a,b;
static void stop_hook(void *p)
{
    board_t *s=p; ++s->stops; s->enabled=false;
    if(!s->faults) { s->faults=128; ++s->epoch; }
}
static uint8_t target_hook(void *p,uint8_t id,int32_t target,uint8_t speed,uint32_t epoch)
{
    board_t *s=p;
    assert(BL_Local(s->link.role,id) && epoch==s->epoch && target>=0 && speed<=100);
    if(!s->feedback_valid || !s->enabled) return BL_FORBIDDEN;
    ++s->targets; s->motor=id; return BL_ACCEPTED;
}
static void init_board(board_t *s,uint8_t role,uint64_t session,uint32_t now)
{
    memset(s,0,sizeof(*s)); s->now=now; s->epoch=role==BL_MASTER?17:83;
    s->feedback_valid=true;
    assert(BL_Init(&s->link,role,session,now,(bl_hooks_t){stop_hook,target_hook,s}));
    BL_SetLocalState(&s->link,s->epoch,s->faults,s->now);
}
static void init(void) { init_board(&a,BL_MASTER,0x1234567811121314ULL,400); init_board(&b,BL_SLAVE,0x9988776655443322ULL,9200); }
static void feed(board_t *to,const uint8_t *p,size_t n)
{ for(size_t i=0;i<n;++i) BL_Feed(&to->link,p[i],to->now,to->now); }
static size_t take(board_t *s,uint8_t *p)
{
    BL_SetLocalState(&s->link,s->epoch,s->faults,s->now);
    return BL_Take(&s->link,s->now,p);
}
static void tick(unsigned elapsed,bool ab,bool ba)
{
    a.now+=elapsed; b.now+=elapsed;
    BL_SetLocalState(&a.link,a.epoch,a.faults,a.now); BL_SetLocalState(&b.link,b.epoch,b.faults,b.now);
    BL_Poll(&a.link,a.now); BL_Poll(&b.link,b.now);
    for(unsigned i=0;i<12;++i) {
        uint8_t p[BL_WIRE_MAX]; size_t x=take(&a,p); if(ab && x) feed(&b,p,x);
        size_t y=take(&b,p); if(ba && y) feed(&a,p,y);
        if(!x && !y) break;
    }
}
static void run(unsigned ms) { for(unsigned i=0;i<ms;i+=5) tick(5,true,true); }
static void recover(void)
{
    a.faults=b.faults=0; ++a.epoch; ++b.epoch;
    run(200);
    assert(!a.faults && !b.faults && BL_Fresh(&a.link,a.now) && BL_Fresh(&b.link,b.now));
    assert(BL_Authorize(&a.link,a.now) && BL_Authorize(&b.link,b.now)); run(60);
    assert(BL_Ready(&a.link,a.now) && BL_Ready(&b.link,b.now)); a.enabled=b.enabled=true;
}
static void ready(void)
{
    init(); run(200);
    assert(a.epoch!=b.epoch && a.now!=b.now && a.link.session!=b.link.session);
    assert(BL_Fresh(&a.link,a.now) && BL_Fresh(&b.link,b.now));
    assert(!BL_Ready(&a.link,a.now) && !a.enabled && !b.enabled);
    recover();
}
static size_t inject_frame(board_t *from,board_t *to,uint8_t type,uint8_t *payload,size_t n,uint8_t *out)
{
    return BL_Encode(type,from->link.role,from->link.session,to->link.session,++from->link.tx_seq,
                     from->epoch,to->epoch,payload,n,out);
}
int main(int argc,char **argv)
{
    const char *test=argc>1?argv[1]:"boot"; ready();
    if(!strcmp(test,"boot")) {
        assert(a.targets==0 && b.targets==0);
        for(uint8_t id=0;id<8;++id) assert(BL_Local(BL_MASTER,id)!=BL_Local(BL_SLAVE,id));
        assert(!BL_Local(BL_MASTER,8) && !BL_Local(0,0));
    } else if(!strcmp(test,"hello-not-healthy")) {
        init(); uint8_t p[86]; size_t n=inject_frame(&b,&a,BL_HELLO,NULL,0,p); feed(&a,p,n);
        assert(a.link.established && !BL_Fresh(&a.link,a.now) && !BL_Authorize(&a.link,a.now) && !a.enabled);
    } else if(!strcmp(test,"targets")) {
        for(uint8_t id=0;id<8;++id) {
            bool remote=!BL_Local(BL_MASTER,id);
            assert(BL_Target(&a.link,id,100,3,a.epoch,a.now)==remote);
            run(15);
            if(remote) assert(b.motor==id && a.link.accepted_status[id]==BL_ACCEPTED && a.link.accepted_id[id]);
        }
        assert(b.targets==5 && a.targets==0);
        assert(!BL_Target(&b.link,4,100,3,b.epoch,b.now));
    } else if(!strcmp(test,"partial")) {
        uint8_t p[BL_WIRE_MAX]; bl_feedback_t f={.id=1,.flags=BL_VALID|BL_CALIBRATED,.age_ms=3,.position_mrad=121};
        assert(BL_OfferFeedback(&b.link,&f,b.now)); size_t n=take(&b,p); assert(n);
        for(size_t i=0;i<n-1;++i) { BL_Feed(&a.link,p[i],a.now,a.now); assert(!a.link.have_feedback[1]); }
        BL_Feed(&a.link,p[n-1],a.now,a.now); assert(a.link.have_feedback[1]);
    } else if(!strcmp(test,"corrupt")) {
        uint8_t p[BL_WIRE_MAX],payload[9]={0}; size_t n=inject_frame(&b,&a,BL_HEARTBEAT,payload,9,p);
        p[n/2]^=0x40; feed(&a,p,n); assert(a.link.malformed && a.targets==0);
        uint8_t noise[]={255,3,1,0}; feed(&a,noise,sizeof(noise)); assert(a.link.malformed>=2);
        run(50); assert(BL_Fresh(&a.link,a.now));
    } else if(!strcmp(test,"duplicate")) {
        assert(BL_Target(&a.link,1,100,3,a.epoch,a.now));
        uint8_t p[BL_WIRE_MAX]; size_t n=take(&a,p); assert(n); feed(&b,p,n); feed(&b,p,n);
        assert(b.targets==1 && b.link.duplicates==1); run(15); assert(a.link.accepted_id[1]);
    } else if(!strcmp(test,"order")) {
        uint8_t p[BL_WIRE_MAX],q[BL_WIRE_MAX],payload[40]={1,3,0,0};
        size_t n=inject_frame(&b,&a,BL_FEEDBACK,payload,40,p);
        BL_Put32(payload+8,200); size_t m=inject_frame(&b,&a,BL_FEEDBACK,payload,40,q);
        feed(&a,q,m); feed(&a,p,n); assert(a.link.feedback[1].position_mrad==200 && a.link.duplicates);
    } else if(!strcmp(test,"reset")) {
        uint64_t session=b.link.session; init_board(&b,BL_SLAVE,session+300,b.now); run(200);
        assert(!BL_Ready(&a.link,a.now) && !a.enabled && !b.enabled && a.faults && b.faults);
        recover(); assert(!a.targets && !b.targets);
    } else if(!strcmp(test,"disconnect") || !strcmp(test,"oneway")) {
        uint32_t start_a=a.now,start_b=b.now;
        for(unsigned i=0;i<36;++i) tick(5,false,!strcmp(test,"oneway"));
        assert(a.faults && b.faults && !a.enabled && !b.enabled);
        assert(a.now-start_a==180 && b.now-start_b==180);
        run(200); assert(!BL_Ready(&a.link,a.now) && !BL_Ready(&b.link,b.now));
    } else if(!strcmp(test,"stop-master") || !strcmp(test,"stop-slave")) {
        board_t *s=!strcmp(test,"stop-master")?&a:&b;
        stop_hook(s); run(20); assert(a.faults && b.faults && !a.enabled && !b.enabled);
    } else if(!strcmp(test,"inflight")) {
        uint8_t p[BL_WIRE_MAX]; assert(BL_Target(&a.link,1,100,3,a.epoch,a.now));
        size_t n=take(&a,p); assert(n); stop_hook(&b);
        BL_SetLocalState(&b.link,b.epoch,b.faults,b.now); feed(&b,p,n); assert(!b.targets); run(20); assert(a.faults);
    } else if(!strcmp(test,"replay")) {
        uint8_t p[BL_WIRE_MAX]; assert(BL_Target(&a.link,1,100,3,a.epoch,a.now)); size_t n=take(&a,p);
        BL_Fail(&a.link,a.now); run(200); recover(); feed(&b,p,n); assert(!b.targets && b.link.rejected);
    } else if(!strcmp(test,"wrap-time")) {
        a.now=UINT32_MAX-20; b.now=UINT32_MAX-90;
        a.link.rx_ms=a.link.ack_ms=a.now; a.link.beat_ms=a.now;
        b.link.rx_ms=b.link.ack_ms=b.now; b.link.beat_ms=b.now;
        run(100); assert(BL_Ready(&a.link,a.now) && BL_Ready(&b.link,b.now));
    } else if(!strcmp(test,"wrap-sequence")) {
        uint64_t before=a.link.session; a.link.tx_seq=UINT32_MAX; uint8_t p[BL_WIRE_MAX];
        assert(!take(&a,p)); assert(a.link.session==before+1 && !a.enabled); run(200); assert(!BL_Ready(&a.link,a.now));
    } else if(!strcmp(test,"wrap-epoch")) {
        a.link.epoch=UINT32_MAX; a.epoch=0; uint64_t before=a.link.session;
        BL_SetLocalState(&a.link,a.epoch,a.faults,a.now); assert(a.link.session==before+1 && !a.enabled);
    } else if(!strcmp(test,"wrap-session")) {
        a.link.session=UINT64_MAX; BL_Fail(&a.link,a.now); uint8_t p[BL_WIRE_MAX]; assert(a.link.permanent_fault && !take(&a,p));
    } else if(!strcmp(test,"saturation")) {
        for(unsigned i=0;i<BL_QUEUE;++i) assert(BL_Target(&a.link,1,100,3,a.epoch,a.now));
        assert(!BL_Target(&a.link,1,100,3,a.epoch,a.now)); assert(!a.enabled && a.faults); run(20); assert(b.faults);
    } else if(!strcmp(test,"ack-timeout")) {
        assert(BL_Target(&a.link,1,100,3,a.epoch,a.now));
        /* Drop just the command, retain valid bidirectional heartbeats. */
        uint8_t p[BL_WIRE_MAX]; assert(take(&a,p)); run(105); assert(a.faults && !a.enabled);
    } else if(!strcmp(test,"feedback")) {
        bl_feedback_t f={.id=1,.flags=BL_VALID|BL_CALIBRATED|BL_RAW_VALID,.age_ms=4,.position_mrad=-120,.velocity_mrad_s=700,.raw_count=-991};
        assert(BL_OfferFeedback(&b.link,&f,b.now)); run(10); bl_feedback_t out;
        assert(BL_GetFeedback(&a.link,1,a.now,&out) && out.position_mrad==-120 && out.raw_count==-991 && (out.flags&BL_VALID) && (out.flags&BL_RAW_VALID));
        assert(out.age_ms>=4 && out.age_ms<=20); run(105);
        assert(BL_GetFeedback(&a.link,1,a.now,&out) && !(out.flags&(BL_VALID|BL_RAW_VALID)));
        assert(!BL_GetFeedback(&b.link,1,b.now,&out));
    } else if(!strcmp(test,"invalid-feedback")) {
        b.feedback_valid=false; assert(BL_Target(&a.link,1,100,3,a.epoch,a.now)); run(20);
        assert(!b.targets && a.faults && b.faults);
    } else if(!strcmp(test,"thumb-fault")) {
        /* Same immediate local stop used by invalid quadrature in production. */
        stop_hook(&b); for(unsigned i=0;i<80;++i) { uint8_t noise=0x33; BL_Feed(&a.link,noise,a.now,a.now); }
        run(30); assert(!a.enabled && !b.enabled && a.faults && b.faults);
    } else if(!strcmp(test,"forbidden")) {
        uint8_t payload[10]={0},p[BL_WIRE_MAX]; BL_Put32(payload,10); payload[4]=4; payload[5]=3;
        size_t n=inject_frame(&a,&b,BL_TARGET,payload,10,p); feed(&b,p,n); assert(!b.targets && b.faults);
    } else if(!strcmp(test,"version-role")) {
        uint8_t p[BL_WIRE_MAX]; size_t n=BL_Encode(BL_HELLO,BL_MASTER,999,a.link.session,100,1,a.epoch,NULL,0,p);
        feed(&a,p,n); assert(a.faults && !a.enabled && a.link.rejected);
    } else if(!strcmp(test,"version") || !strcmp(test,"old-version")) {
        uint8_t raw[36]={!strcmp(test,"old-version")?1:BL_VERSION+1,BL_HELLO,BL_SLAVE,0},p[86];
        BL_Put32(raw+4,(uint32_t)b.link.session); BL_Put32(raw+8,(uint32_t)(b.link.session>>32));
        BL_Put32(raw+12,(uint32_t)a.link.session); BL_Put32(raw+16,(uint32_t)(a.link.session>>32));
        BL_Put32(raw+20,UINT32_MAX-1); BL_Put32(raw+24,b.epoch); BL_Put32(raw+28,a.epoch);
        uint32_t check=UINT32_MAX;
        for(unsigned i=0;i<32;++i) { check^=raw[i]; for(unsigned j=0;j<8;++j) check=(check>>1)^((0U-(check&1U))&0xEDB88320U); }
        BL_Put32(raw+32,~check); size_t write=1,code_at=0; unsigned code=1;
        for(unsigned i=0;i<36;++i) if(raw[i]) { p[write++]=raw[i]; ++code; }
            else { p[code_at]=(uint8_t)code; code_at=write++; code=1; }
        p[code_at]=(uint8_t)code; p[write++]=0;
        feed(&a,p,write); assert(a.link.rejected && a.faults && !a.enabled);
    } else if(!strcmp(test,"deadline")) {
        a.link.rx_ms=a.link.ack_ms=a.now; b.link.rx_ms=b.link.ack_ms=b.now;
        a.now+=150; b.now+=150; BL_Poll(&a.link,a.now); BL_Poll(&b.link,b.now);
        assert(!a.faults && !b.faults); ++a.now; ++b.now;
        BL_Poll(&a.link,a.now); BL_Poll(&b.link,b.now); assert(a.faults && b.faults);
    } else if(!strcmp(test,"critical-full")) {
        for(unsigned i=0;i<BL_QUEUE;++i) assert(BL_Target(&a.link,1,100,3,a.epoch,a.now));
        stop_hook(&a); uint8_t p[86]; size_t n=take(&a,p); assert(n && !a.link.count);
        feed(&b,p,n); assert(b.faults && !b.targets);
    } else if(!strcmp(test,"invalid-id")) {
        uint8_t p[86],payload[10]={1,0,0,0,255,3,1,0,0,0};
        size_t n=inject_frame(&a,&b,BL_TARGET,payload,10,p); feed(&b,p,n); assert(b.faults && !b.targets);
    } else if(!strcmp(test,"unexpected-hello")) {
        uint8_t p[86]; size_t n=inject_frame(&b,&a,BL_HELLO,NULL,0,p);
        feed(&a,p,n); assert(a.faults && !a.enabled && !BL_Ready(&a.link,a.now));
    } else if(!strcmp(test,"malformed-stop")) {
        uint8_t p[86],payload[1]={0}; size_t n=inject_frame(&b,&a,BL_STOP,payload,1,p);
        feed(&a,p,n); assert(a.faults && !a.enabled && a.link.rejected);
    } else if(!strcmp(test,"truncated")) {
        uint8_t p[BL_WIRE_MAX]; size_t n=BL_Encode(BL_HELLO,BL_SLAVE,b.link.session,a.link.session,100,b.epoch,a.epoch,NULL,0,p);
        feed(&a,p,n/2); a.now+=BL_FRAME_MS+1; BL_Poll(&a.link,a.now); assert(!a.link.encoded_length && a.link.malformed);
    } else if(!strcmp(test,"high-load")) {
        for(unsigned i=0;i<1000;++i) { assert(BL_Target(&a.link,(uint8_t)(i%4),100,3,a.epoch,a.now)); run(5); }
        assert(b.targets==1000 && !a.faults && !b.faults);
    } else if(!strncmp(test,"application",11)) {
        uint8_t bytes[BL_WIRE_MAX],payload[]={2,10,0,0,0,7,1};
        size_t n=inject_frame(&a,&b,BL_APPLICATION,payload,sizeof payload,bytes);
        if(!strcmp(test,"application-stale")) {
            n=BL_Encode(BL_APPLICATION,BL_MASTER,a.link.session,b.link.session,++a.link.tx_seq,
                        a.epoch,b.epoch-1,payload,sizeof payload,bytes);
            feed(&b,bytes,n); assert(!b.link.have_application && b.faults);
        } else if(!strcmp(test,"application-deadline")) {
            assert(BL_OfferApplication(&a.link,payload,sizeof payload,a.now));
            a.now+=BL_COMMAND_QUEUE_MS+1;
            take(&a,bytes); take(&a,bytes); assert(a.faults);
        } else {
            feed(&b,bytes,n); assert(b.link.have_application);
            if(!strcmp(test,"application-duplicate")) {
                feed(&b,bytes,n); assert(b.link.duplicates && !b.faults);
            } else if(!strcmp(test,"application-overflow")) {
                n=inject_frame(&a,&b,BL_APPLICATION,payload,sizeof payload,bytes);
                feed(&b,bytes,n); assert(b.faults && !b.link.have_application);
            } else if(!strcmp(test,"application-stop")) {
                stop_hook(&b); BL_SetLocalState(&b.link,b.epoch,b.faults,b.now);
                assert(!b.link.have_application);
            }
            if(!b.faults) {
                bl_message_t message;
                assert(BL_TakeApplication(&b.link,&message) && !BL_TakeApplication(&b.link,&message));
                assert(message.epoch==b.epoch && message.peer==a.link.session);
                assert(message.length==sizeof payload && !memcmp(message.data,payload,sizeof payload));
            }
        }
    } else if(!strcmp(test,"baud-budget")) {
        struct wire { uint8_t bytes[BL_WIRE_MAX]; size_t n; unsigned left; } ab={0},ba={0};
        for(unsigned ms=0;ms<1000;ms+=2) {
            a.now+=2; b.now+=2;
            BL_SetLocalState(&a.link,a.epoch,a.faults,a.now); BL_SetLocalState(&b.link,b.epoch,b.faults,b.now);
            BL_Poll(&a.link,a.now); BL_Poll(&b.link,b.now);
            if(ms%100==0) for(uint8_t id=0;id<8;++id) if(!BL_Local(BL_MASTER,id)) assert(BL_Target(&a.link,id,100,3,a.epoch,a.now));
            if(ms%10==0) {
                uint8_t id=(uint8_t)((ms/10)%8); bl_feedback_t f={.id=id,.flags=3};
                if(BL_Local(BL_MASTER,id)) BL_OfferFeedback(&a.link,&f,a.now);
                else BL_OfferFeedback(&b.link,&f,b.now);
            }
            if(ab.n && ab.left<=2) { feed(&b,ab.bytes,ab.n); ab.n=0; } else if(ab.n) ab.left-=2;
            if(ba.n && ba.left<=2) { feed(&a,ba.bytes,ba.n); ba.n=0; } else if(ba.n) ba.left-=2;
            if(!ab.n) { ab.n=take(&a,ab.bytes); ab.left=(unsigned)((ab.n*10000U+115199U)/115200U); }
            if(!ba.n) { ba.n=take(&b,ba.bytes); ba.left=(unsigned)((ba.n*10000U+115199U)/115200U); }
            assert(!a.faults && !b.faults);
        }
        assert(b.targets==50 && BL_Ready(&a.link,a.now) && BL_Ready(&b.link,b.now));
    } else { fprintf(stderr,"unknown test %s\n",test); return 2; }
    printf("PASS dual-board %s: independent sessions/epochs/clocks; exit=0\n",test); return 0;
}
