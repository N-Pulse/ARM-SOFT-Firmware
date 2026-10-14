#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif
typedef struct {
    void *handle;
    void (*Init)(uint32_t,uint32_t,uint64_t),(*Step)(uint32_t,uint32_t),(*Feed)(const uint8_t *,uint32_t);
    void (*Stop)(void),(*EncoderFault)(void);
    uint32_t (*Take)(uint8_t *),(*Operator)(uint32_t),(*Command)(uint32_t),(*Configure)(void),
             (*Target)(void),(*Fresh)(void),(*Faults)(void),(*Disabled)(void),(*Epoch)(void),(*Role)(void),(*RemoteRaw)(uint32_t);
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
    LOAD(Operator); LOAD(Command); LOAD(Configure); LOAD(Target); LOAD(Fresh); LOAD(Faults); LOAD(Disabled);
    LOAD(Epoch); LOAD(Role); LOAD(RemoteRaw); LOAD(PWM);
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
int main(int argc,char **argv)
{
    assert(argc==4); const char *test=argv[3];
    a=load(argv[1],1,400,111222333);
    if(!strcmp(test,"late-start")) {
        for(unsigned i=0;i<300;++i) { uint8_t bytes[86]; a.Step(2,1); a.Take(bytes); }
        assert(!a.Fresh() && a.Disabled() && a.Operator(13)!=0);
    }
    b=load(argv[2],2,9200,444555666); assert(a.Role()==1 && b.Role()==2); healthy();
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
