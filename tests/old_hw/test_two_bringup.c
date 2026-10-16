#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif
typedef struct {
    void *handle;
    void (*Init)(uint32_t,uint32_t,uint64_t),(*Step)(uint32_t,uint32_t),(*Feed)(const uint8_t *,uint32_t);
    void (*Stop)(void),(*EncoderFault)(void),(*ThumbFault)(void);
    void (*AdvanceEncoder)(int32_t);
    uint32_t (*Take)(uint8_t *),(*Operator)(uint32_t),(*Command)(uint32_t),(*Configure)(void),
             (*Target)(void),(*Fresh)(void),(*Faults)(void),(*Disabled)(void),(*Epoch)(void),(*Role)(void),(*RemoteRaw)(uint32_t),
             (*Route)(uint32_t),(*Reply)(void),(*Exclusive)(void),(*Select)(uint32_t),(*Selected)(void),
             (*Configured)(void),(*EnabledCount)(void),(*NominalEncoder)(void);
    float (*PWM)(void);
} api_t;
static api_t a,b;
static api_t load(const char *path,uint32_t role,uint32_t clock,uint64_t nonce)
{
    api_t v;
#ifdef _WIN32
    v.handle=(void *)LoadLibraryA(path);
#define LOAD(name) *(void **)(&v.name)=(void *)GetProcAddress((HMODULE)v.handle,"B4_"#name); assert(v.name)
#else
    v.handle=dlopen(path,RTLD_NOW|RTLD_LOCAL);
#define LOAD(name) *(void **)(&v.name)=dlsym(v.handle,"B4_"#name); assert(v.name)
#endif
    assert(v.handle); LOAD(Init); LOAD(Step); LOAD(Feed); LOAD(Stop); LOAD(EncoderFault); LOAD(Take);
    LOAD(Operator); LOAD(Command); LOAD(Configure); LOAD(Target); LOAD(Fresh); LOAD(Faults); LOAD(Disabled); LOAD(ThumbFault);
    LOAD(Epoch); LOAD(Role); LOAD(RemoteRaw); LOAD(PWM);
    LOAD(Route); LOAD(Reply); LOAD(Exclusive);
    LOAD(Select); LOAD(Selected); LOAD(Configured); LOAD(EnabledCount);
    LOAD(NominalEncoder);
    LOAD(AdvanceEncoder);
#undef LOAD
    v.Init(role,clock,nonce); return v;
}
static void unload(api_t *v)
{
#ifdef _WIN32
    assert(FreeLibrary((HMODULE)v->handle));
#else
    assert(dlclose(v->handle)==0);
#endif
}
static void pump(unsigned ms,bool ab,bool ba,bool host_a,bool host_b)
{
    for(unsigned t=0;t<ms;t+=2) {
        a.Step(2,host_a); b.Step(2,host_b);
        uint8_t bytes[86]; unsigned n=a.Take(bytes); if(ab && n) b.Feed(bytes,n);
        n=b.Take(bytes); if(ba && n) a.Feed(bytes,n);
    }
}
static void healthy(void)
{
    pump(220,true,true,true,true); assert(a.Fresh() && b.Fresh() && a.Disabled() && b.Disabled());
    assert(a.Faults() && b.Faults()); /* HELLO never clears an independent latch. */
    assert(a.Operator(13)==0 && b.Operator(13)==0); pump(220,true,true,true,true);
    assert(!a.Faults() && !b.Faults() && a.Disabled() && b.Disabled());
}
static void arm(api_t *local)
{
    assert(local->Configure()==0); pump(30,true,true,true,true);
    assert(local->Command(11)==0 && local->Command(5)==0 && local->Command(6)==0);
    assert(local->PWM()==0); /* Enabling does not resume an old target. */
}
static uint32_t routed(uint32_t op)
{
    a.Route(op); pump(80,true,true,true,false);
    assert(a.Reply()!=UINT32_MAX); return a.Reply();
}
int main(int argc,char **argv)
{
    assert(argc==4); const char *test=argv[3];
    a=load(argv[1],1,400,111222333);
    if(!strcmp(test,"late-start")) {
        for(unsigned i=0;i<300;++i) { uint8_t bytes[86]; a.Step(2,1); a.Take(bytes); }
        assert(!a.Fresh() && a.Disabled() && a.Operator(13)!=0);
    }
    b=load(argv[2],2,9200,444555666); assert(a.Role()==1 && b.Role()==2);
    if(!strncmp(test,"timed-",6)) {
        pump(220,true,true,true,false);
        assert(a.Select(0)==UINT32_MAX); pump(180,true,true,true,false); assert(a.Reply()==0);
        b.ThumbFault(); /* Actual invalid Gray transition: position unavailable, not fabricated. */
        assert(routed(13)==0 && routed(1)==0);
        if(!strcmp(test,"timed-assume")) assert(routed(105)==0);
        assert(routed(11)==0 && routed(5)==0 && routed(6)==0);
        assert(routed(!strcmp(test,"timed-assume")?103:!strcmp(test,"timed-cap")?104:7)==0);
        assert(fabsf(b.PWM())>24.9f && a.Exclusive() && b.Exclusive());
        if(!strcmp(test,"timed-stop")) {
            a.Stop(); pump(40,true,true,true,false); assert(a.Disabled() && b.Disabled());
        } else if(!strcmp(test,"timed-loss")) {
            pump(200,false,true,true,false); assert(a.Disabled() && b.Disabled());
        } else if(!strcmp(test,"timed-cap")) {
            pump(2100,true,true,true,false); assert(a.Disabled() && b.Disabled());
        } else {
            if(!strcmp(test,"timed-assume")) assert(b.PWM()<-24.9f);
            else assert(b.PWM()>24.9f);
            assert(routed(105)==6); /* Refuse estimated-origin changes during motion. */
            pump(1100,true,true,true,false);
            assert(a.Disabled() && b.Disabled() && b.PWM()==0 && !(b.Faults()&(2U|4U)));
            if(!strcmp(test,"timed-move")) {
                /* A second PC session reselects the same finger. Estimated position survives. */
                assert(a.Select(0)==UINT32_MAX); pump(180,true,true,true,false); assert(a.Reply()==0);
                assert(routed(13)==0 && routed(1)==0 && routed(11)==0 && routed(5)==0 && routed(6)==0 && routed(103)==0);
                assert(b.PWM()<-24.9f);
                pump(1100,true,true,true,false); assert(a.Disabled() && b.Disabled() && b.PWM()==0);
            }
        }
        printf("PASS %s: invalid encoder bypass isolated to timed diagnostics, signed 25%%, estimated reference persists, local timer/STOP/link cutoff\n",test);
        unload(&a); unload(&b); return 0;
    }
    if(!strncmp(test,"diagnostic",10)) {
        bool checks=!strncmp(test,"diagnostic-check",16);
        pump(220,true,true,true,false);
        assert(a.Select(0)==UINT32_MAX); pump(180,true,true,true,false); assert(a.Reply()==0);
        assert(routed(13)==0 && routed(1)==0);
        if(checks) assert(routed(5)!=0 && a.Disabled() && b.Disabled()); /* Preflight restored. */
        assert(routed(11)==0 && routed(5)==0 && routed(6)==0);
        if(checks) { assert(routed(7)==5 && b.PWM()==0); assert(routed(102)==0); } /* Step restored. */
        else assert(routed(7)==0); /* A single full 30-degree target is accepted. */
        assert(b.PWM()>24.9f && b.PWM()<=25.0f && a.PWM()==0 && a.Exclusive() && b.Exclusive());
        if(!strcmp(test,"diagnostic-timeout")) {
            pump(400,true,true,true,false);
            assert(!b.Disabled() && b.PWM()>24.9f && !(b.Faults()&2U)); /* GUARD bypass, no movement. */
            pump(1700,true,true,true,false); assert(a.Disabled() && b.Disabled() && !(b.Faults()&2U));
        } else if(!strcmp(test,"diagnostic-stop")) {
            a.Stop(); pump(40,true,true,true,false); assert(a.Disabled() && b.Disabled());
        } else if(!strcmp(test,"diagnostic-loss")) {
            pump(200,false,true,true,false); assert(a.Disabled() && b.Disabled());
        } else if(!strcmp(test,"diagnostic-check-guard")) {
            pump(220,true,true,true,false); assert(a.Disabled() && b.Disabled() && (b.Faults()&2U));
        } else if(!strcmp(test,"diagnostic-check-travel")) {
            b.AdvanceEncoder(450); pump(40,true,true,true,false);
            assert(a.Disabled() && b.Disabled() && (b.Faults()&4U));
        } else {
            b.AdvanceEncoder(244); pump(40,true,true,true,false);
            assert(a.Disabled() && b.Disabled() && b.PWM()==0 && !(b.Faults()&(2U|4U)));
            assert(routed(13)==0 && routed(11)==0 && routed(5)==0 && routed(6)==0 && routed(103)==0);
            assert(b.PWM()<-24.9f && b.PWM()>=-25.0f);
            b.AdvanceEncoder(-244); pump(40,true,true,true,false);
            assert(a.Disabled() && b.Disabled() && b.PWM()==0);
        }
        printf("PASS %s: direct 25 percent, full 30 degrees, goal/2s/STOP/link cutoff, independently restored checks\n",test);
        unload(&a); unload(&b); return 0;
    }
    if(!strncmp(test,"selectable",10)) {
        bool nominal=!strcmp(test,"selectable-nominal") || !strcmp(test,"selectable-settle");
        if(nominal) assert(a.NominalEncoder() && b.NominalEncoder());
        pump(220,true,true,true,false);
        assert(a.Selected()==8 && b.Selected()==8 && a.Route(5)==4);
        const uint32_t ids[]={4,1,2,3,4,1,0}; bool configured[5]={false};
        for(unsigned i=0;i<sizeof(ids)/sizeof(ids[0]);++i) {
            uint32_t id=ids[i];
            assert(a.Select(id)==UINT32_MAX && a.Disabled());
            pump(180,true,true,true,false);
            assert(a.Reply()==0 && a.Selected()==id && b.Selected()==id);
            assert(a.Disabled() && b.Disabled() && a.Exclusive() && b.Exclusive());
            assert(routed(13)==0); api_t *owner=id==4?&a:&b;
            assert((owner->Configured()!=0)==configured[id]);
            assert(routed(1)==0);
            if(!nominal) assert(routed(2)==0 && routed(3)==0);
            configured[id]=true;
            assert(routed(11)==0);
            assert(routed(5)==0 && routed(6)==0 && routed(7)==0);
            assert(owner->PWM()>0 && a.EnabledCount()+b.EnabledCount()==1 && a.Exclusive() && b.Exclusive());
            assert(owner->PWM()<=3.0f && owner->PWM()>=2.9f); /* Requested ceiling, not 0.2% from legacy Kp. */
            if(id==1 && !strcmp(test,"selectable-stall")) {
                pump(220,true,true,true,false);
                assert(a.Disabled() && b.Disabled() && (b.Faults()&2U)); break;
            }
            if(id==1 && !strcmp(test,"selectable-settle")) {
                b.AdvanceEncoder(6); pump(60,true,true,true,false);
                b.AdvanceEncoder(3); pump(300,true,true,true,false);
                assert(!a.Faults() && !b.Faults() && b.EnabledCount()==1 && b.PWM()==0);
                a.Stop(); pump(40,true,true,true,false);
                assert(a.Disabled() && b.Disabled()); break;
            }
            if(id==0) {
                assert(owner->PWM()<=5.0f);
                if(!strcmp(test,"selectable-thumb-irq")) {
                    b.EncoderFault(); pump(40,true,true,true,false);
                    assert(a.Disabled() && b.Disabled() && (b.Faults()&4U));
                } else if(!strcmp(test,"selectable-thumb-invalid")) {
                    b.ThumbFault(); assert(b.Disabled()); pump(40,true,true,true,false);
                    assert(a.Disabled() && b.Disabled() && (b.Faults()&4U));
                }
            }
            if(!strcmp(test,"selectable-loss") && id==1) {
                assert(a.Select(4)==UINT32_MAX); pump(300,false,true,true,false);
                assert(a.Reply()==3 && a.Disabled() && b.Disabled()); break;
            }
            if(!strcmp(test,"selectable-reset") && id==1) {
                assert(a.Select(4)==UINT32_MAX);
                unload(&b); b=load(argv[2],2,40,777888999); pump(180,true,true,true,false);
                assert(a.Reply()==3 && a.Disabled() && b.Disabled() && b.Selected()==8); break;
            }
        }
        printf("PASS %s: one common image, runtime switches across boards, independent limits/encoder settings, one enabled motor, explicit rearm, bounded THUMB bench output\n",test);
        unload(&a); unload(&b); return 0;
    }
    if(!strncmp(test,"routed-",7)) {
        /* Only A receives host traffic. All setup/replies use the actual USART3 adapter. */
        pump(220,true,true,true,false); assert(routed(13)==0);
        pump(100,true,true,true,false); assert(!a.Faults() && !b.Faults());
        assert(routed(1)==0 && routed(2)==0 && routed(3)==0);
        pump(2100,true,true,true,false); /* Slave host lease must be forwarded continuously. */
        assert(!a.Faults() && !b.Faults());
        assert(routed(11)==0);
        if(!strcmp(test,"routed-thumb")) {
            assert(routed(5)!=0 && a.Disabled() && b.Disabled());
        } else {
            assert(routed(5)==0 && routed(6)==0);
            if(!strcmp(test,"routed-wrong-motor")) {
                assert(routed(100)==4);
            } else if(!strcmp(test,"routed-stale")) {
                assert(routed(101)==3);
            } else if(!strcmp(test,"routed-ack-timeout")) {
                assert(a.Route(7)==UINT32_MAX);
                pump(300,true,false,true,false);
                assert(a.Reply()==3 && a.Disabled() && b.Disabled());
            } else if(!strcmp(test,"routed-inflight-stop")) {
                assert(a.Route(7)==UINT32_MAX); a.Stop();
                pump(100,true,true,true,false);
                assert(a.Reply()==3 && a.Disabled() && b.Disabled() && a.PWM()==0 && b.PWM()==0);
            } else {
                assert(routed(7)==0);
                assert(a.PWM()>0 || b.PWM()>0);
                if(!strcmp(test,"routed-host-loss")) pump(2100,true,true,false,false);
                else if(!strcmp(test,"routed-oneway")) pump(300,false,true,true,false);
                else if(!strcmp(test,"routed-reset")) {
                    unload(&b); b=load(argv[2],2,40,777888999); pump(180,true,true,true,false);
                } else { a.Stop(); pump(40,true,true,true,false); }
                assert(a.Disabled() && b.Disabled());
            }
            assert(a.Exclusive() && b.Exclusive());
        }
        printf("PASS %s: master-only host, routed calibration/limits/preflight/arm/enable/target, single finger and STOP\n",test);
        unload(&a); unload(&b); return 0;
    }
    healthy();
    assert(a.RemoteRaw(1) && b.RemoteRaw(4)); /* Raw acquisition without calibration, both roles. */
    if(!strcmp(test,"default") || !strcmp(test,"late-start")) {
        assert(a.Command(5)!=0 && b.Command(5)!=0 && a.Disabled() && b.Disabled());
        assert(a.Operator(12)==0 && b.Operator(12)==0); pump(60,true,true,true,true);
        assert(a.Disabled() && b.Disabled());
    } else if(!strcmp(test,"independent-stop")) {
        a.Stop(); pump(30,true,true,true,true); assert(a.Faults()&1U);
        assert(b.Operator(13)==0); pump(100,true,true,true,true);
        assert(a.Faults()&1U); /* Recovery on B cannot clear A's MANUAL stop. */
        assert(a.Operator(13)==0); pump(160,true,true,true,true);
        assert(a.Disabled() && b.Disabled() && !a.Faults() && !b.Faults());
    } else {
        api_t *local=!strcmp(test,"little")?&a:&b; arm(local);
        if(!strcmp(test,"host-loss")) {
            pump(1990,true,true,true,false); assert(!b.Disabled());
            pump(40,true,true,true,false); assert(a.Disabled() && b.Disabled() && (b.Faults()&128U));
        } else {
            assert(local->Target()==0); pump(30,true,true,true,true); assert(local->PWM()>0 && local->PWM()<=3);
            if(!strcmp(test,"reset")) {
                unload(&b); b=load(argv[2],2,40,777888999); pump(180,true,true,true,true);
                assert(a.Disabled() && b.Disabled() && a.Faults() && b.Faults());
                assert(b.Command(5)!=0); /* No implicit recovery/rearm after reboot. */
            } else if(!strcmp(test,"encoder")) {
                local->EncoderFault(); pump(30,true,true,true,true);
                assert(a.Disabled() && b.Disabled() && (local->Faults()&4U));
                assert(!a.RemoteRaw(1));
            } else if(!strcmp(test,"overflow")) {
                uint8_t noise[256]; memset(noise,1,sizeof(noise)); local->Feed(noise,sizeof(noise));
                assert(local->Disabled()); pump(40,true,true,true,true); assert(a.Disabled() && b.Disabled());
            } else if(!strcmp(test,"oneway")) {
                pump(180,false,true,true,true); assert(a.Disabled() && b.Disabled());
            } else {
                local->Stop(); pump(30,true,true,true,true); assert(a.Disabled() && b.Disabled());
                assert(a.Operator(13)==0 && b.Operator(13)==0); pump(180,true,true,true,true);
                assert(local->Command(5)!=0); /* New preflight and enable still required. */
                arm(local); assert(local->Target()==0); pump(30,true,true,true,true); assert(local->PWM()>0);
                local->Stop(); pump(20,true,true,true,true); assert(a.Disabled() && b.Disabled());
            }
        }
    }
    printf("PASS actual dual-board bringup %s, independent globals/clocks, real USART3 + control + recovery gates; exit=0\n",test);
    return 0;
}
