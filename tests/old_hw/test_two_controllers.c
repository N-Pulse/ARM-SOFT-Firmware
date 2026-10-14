#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif
typedef struct {
    void (*Init)(uint32_t,uint64_t);
    void (*Step)(uint32_t),(*Feed)(const uint8_t *,uint32_t),(*Stop)(void),(*ThumbFault)(void),(*Feedback)(void);
    uint32_t (*Take)(uint8_t *),(*Recover)(void),(*Authorize)(void),(*Enable)(void),(*Ready)(void),(*Epoch)(void),
        (*Faults)(void),(*Disabled)(void),(*LocalTarget)(void),(*RemoteValid)(uint32_t),(*Target)(uint32_t,int32_t);
    float (*PWM)(void);
} board_api_t;
static board_api_t a,b;
static void *symbol(void *handle,const char *name)
{
#ifdef _WIN32
    void *p=(void *)GetProcAddress((HMODULE)handle,name);
#else
    void *p=dlsym(handle,name);
#endif
    assert(p); return p;
}
static board_api_t load(const char *path,uint32_t clock,uint64_t nonce)
{
#ifdef _WIN32
    void *handle=(void *)LoadLibraryA(path);
#else
    void *handle=dlopen(path,RTLD_NOW|RTLD_LOCAL);
#endif
    assert(handle); board_api_t api;
#define LOAD(name) *(void **)(&api.name)=symbol(handle,"B3_"#name)
    LOAD(Init); LOAD(Step); LOAD(Feed); LOAD(Stop); LOAD(ThumbFault); LOAD(Feedback); LOAD(Take); LOAD(Recover);
    LOAD(Authorize); LOAD(Enable); LOAD(Ready); LOAD(Epoch); LOAD(Faults); LOAD(Disabled); LOAD(LocalTarget);
    LOAD(RemoteValid); LOAD(Target); LOAD(PWM);
#undef LOAD
    api.Init(clock,nonce); return api;
}
static void pump(unsigned ms,bool ab,bool ba)
{
    for(unsigned t=0;t<ms;t+=10) {
        a.Step(10); b.Step(10);
        for(unsigned i=0;i<12;++i) {
            uint8_t p[86]; unsigned x=a.Take(p); if(ab && x) b.Feed(p,x);
            unsigned y=b.Take(p); if(ba && y) a.Feed(p,y);
            if(!x && !y) break;
        }
    }
}
int main(int argc,char **argv)
{
    assert(argc==4); bool thumb=argv[3][0]=='1'; uint32_t remote=thumb?0:1;
    a=load(argv[1],400,111222333); b=load(argv[2],9200,444555666);
    pump(200,true,true); assert(a.Disabled() && b.Disabled() && a.Epoch()!=b.Epoch());
    assert(a.Recover()==0 && b.Recover()==0); pump(100,true,true);
    assert(a.Authorize() && b.Authorize()); pump(60,true,true);
    assert(a.Ready() && b.Ready()); assert(a.Enable()==0 && b.Enable()==0);
    assert(a.PWM()==0 && b.PWM()==0);
    assert(a.LocalTarget()==0 && a.Target(remote,20)); pump(30,true,true);
    assert(a.PWM()>0 && a.PWM()<=3 && b.PWM()>0 && b.PWM()<=3);
    b.Feedback(); pump(10,true,true); assert(a.RemoteValid(remote));
    if(thumb) {
        b.ThumbFault(); assert(b.Disabled()); pump(20,true,true);
        assert(a.Disabled() && a.Faults() && b.Faults());
    } else {
        b.Stop(); assert(b.Disabled()); pump(20,true,true);
        assert(a.Disabled() && a.Faults() && b.Faults() && !a.Target(1,20));
        assert(a.Recover()==0 && b.Recover()==0); pump(100,true,true);
        assert(!a.Ready() && !b.Ready()); assert(a.Authorize() && b.Authorize()); pump(60,true,true);
        assert(a.Enable()==0 && b.Enable()==0); assert(a.PWM()==0 && b.PWM()==0);
        assert(a.LocalTarget()==0 && a.Target(1,20)); pump(30,true,true); assert(a.PWM()>0 && b.PWM()>0);
        pump(180,false,true); assert(a.Disabled() && b.Disabled());
    }
    printf("PASS two independent real OLD_HW controllers: %s; exit=0\n",thumb?"THUMB IRQ fault during traffic":
        "local/remote PID, feedback, STOP, recovery, new targets, one-way loss");
    return 0;
}
