/* Reuse only the existing I/O fixture; call actual driver and pair writers. */
#define OUTPUTS_TEST_DSB
#define main previous_safety_suite
#include "../safety/test_safety.c"
#undef main
#include "motor_outputs.h"
#include "motor_lock.h"

static unsigned resets_seen;
static bool check_pair;
static uint32_t test_timer;
static bool check_tim_pair;
static unsigned first_tim, second_tim, second_channel, tim_barriers;
static void apply_commands(void)
{
    HRTIM_Common_TypeDef *c = &hrtim_registers.sCommonRegs;
    if (c->OENR) { c->ODSR &= ~c->OENR; c->OENR=0; }
    if (c->ODISR) { c->ODSR |= c->ODISR; c->ODISR=0; }
}
void __DSB(void)
{
    if (check_tim_pair) {
        assert(!(timers[first_tim].CR1 & TIM_CR1_CEN));
        assert(!(timers[second_tim].CR1 & TIM_CR1_CEN));
        if ((tim_barriers++ % 2U) == 0U) {
            assert((timers[first_tim].CCMR1 & TIM_CCMR1_OC1M) == TIM_OCMODE_FORCED_INACTIVE);
            assert(((timers[second_tim].CCMR1 >> (second_channel*8)) & TIM_CCMR1_OC1M) == TIM_OCMODE_FORCED_INACTIVE);
        }
    }
    apply_commands();
    for (unsigned i=0;i<6;++i) {
        HRTIM_Timerx_TypeDef *t=&hrtim_registers.sTimerxRegs[i];
        if (t->RSTx1R & HRTIM_RST1R_SRT) {
            if (check_pair && i == test_timer) {
                assert((hrtim_registers.sMasterRegs.MCR & (HRTIM_MCR_TACEN << i)) == 0);
                assert((hrtim_registers.sCommonRegs.ODSR & (3U << (2*i))) == (3U << (2*i)));
                ++resets_seen;
            }
            t->RSTx1R &= ~HRTIM_RST1R_SRT;
        }
        if (t->RSTx2R & HRTIM_RST2R_SRT) t->RSTx2R &= ~HRTIM_RST2R_SRT;
        /* The model observes software triggers, not the analog waveform. */
        t->SETx1R &= ~HRTIM_SET1R_SST;
        t->SETx2R &= ~HRTIM_SET2R_SST;
    }
}
static void expect(motor_id_t id, float a, float b)
{
    apply_commands();
    /* Expected mapping independently copied from the existing MCU channel map. */
    const unsigned timer[MOTOR_COUNT]={[MOTOR_THUMB]=2,[MOTOR_INDEX]=0,[MOTOR_MIDDLE]=1,
        [MOTOR_RING]=5,[MOTOR_LITTLE]=4,[MOTOR_PALM]=3};
    const unsigned a_unit[MOTOR_COUNT]={[MOTOR_THUMB]=0,[MOTOR_INDEX]=1,[MOTOR_MIDDLE]=1,
        [MOTOR_RING]=0,[MOTOR_LITTLE]=1,[MOTOR_PALM]=1};
    if (id != MOTOR_WRIST_X && id != MOTOR_WRIST_Y) {
        unsigned i=timer[id]; HRTIM_Timerx_TypeDef *t=&hrtim_registers.sTimerxRegs[i];
        float values[2]; values[a_unit[id]]=a; values[1-a_unit[id]]=b;
        for(unsigned u=0;u<2;++u) {
            float duty=values[u]; bool active=duty*7999.0f/100.0f >= 3.0f;
            uint32_t set=u ? t->SETx2R : t->SETx1R;
            uint32_t rst=u ? t->RSTx2R : t->RSTx1R;
            assert(set == (active ? HRTIM_SET1R_PER : 0U));
            assert(rst == (active && duty<100 ? (u ? HRTIM_RST2R_CMP2 : HRTIM_RST1R_CMP1) : 0));
            uint32_t cmp=hhrtim1.compare[i][u];
            if (!active) assert(cmp==3U);
            else if (duty>=100) assert(cmp==8000U);
            else assert(cmp>=3U && cmp<=7996U);
        }
        assert(!(hrtim_registers.sCommonRegs.ODSR & (3U << (2*i))));
        assert(hrtim_registers.sMasterRegs.MCR & (HRTIM_MCR_TACEN << i));
    } else {
        unsigned first=id==MOTOR_WRIST_X ? 0 : 1;
        unsigned second=id==MOTOR_WRIST_X ? 0 : 2;
        unsigned ch=id==MOTOR_WRIST_X ? 1 : 0;
        uint32_t mode_a=timers[first].CCMR1 & TIM_CCMR1_OC1M;
        uint32_t mode_b=(timers[second].CCMR1 >> (ch*8)) & TIM_CCMR1_OC1M;
        assert(mode_a==(a>=100 ? TIM_OCMODE_FORCED_ACTIVE : a<=0 ? TIM_OCMODE_FORCED_INACTIVE : TIM_OCMODE_PWM1));
        assert(mode_b==(b>=100 ? TIM_OCMODE_FORCED_ACTIVE : b<=0 ? TIM_OCMODE_FORCED_INACTIVE : TIM_OCMODE_PWM1));
        assert(timers[first].compare[0] == (uint32_t)(a*80.0f));
        assert(timers[second].compare[ch] == (uint32_t)(b*80.0f));
        assert(timers[first].CR1 & TIM_CR1_CEN);
    }
}
int main(void)
{
    for(unsigned id=0;id<MOTOR_COUNT;++id) {
        reset(); arm(); enable((motor_id_t)id);
        const float values[]={0.0f,0.001f,0.025f,0.037f,0.038f,0.1f,50.0f,99.99f,100.0f,150.0f};
        const bool hrtim_active[]={false,false,false,false,true,true,true,true,true,true};
        const bool tim_active[]={false,false,true,true,true,true,true,true,true,true};
        for(unsigned i=0;i<sizeof(values)/sizeof(values[0]);++i) {
            float v=values[i], limited=v>100 ? 100 : v;
            Motor_DRV8848_SetRaw((motor_id_t)id,v); expect((motor_id_t)id,limited,0);
            bool active=(id==MOTOR_WRIST_X || id==MOTOR_WRIST_Y) ? tim_active[i] : hrtim_active[i];
            assert(Motor_DRV8848_DrivingDir((motor_id_t)id)==(active ? 1 : 0));
            Motor_DRV8848_SetRaw((motor_id_t)id,-v); expect((motor_id_t)id,0,limited);
            assert(Motor_DRV8848_DrivingDir((motor_id_t)id)==(active ? -1 : 0));
        }
        Motor_DRV8848_Brake((motor_id_t)id); expect((motor_id_t)id,100,100);
        Motor_DRV8848_Stop((motor_id_t)id); expect((motor_id_t)id,0,0);
        assert(!primask);
    }
    puts("PASS all 8 production mappings: zero, tiny, extremes, signed reversal, brake, stop");

    reset(); arm(); enable(MOTOR_INDEX); check_pair=true; test_timer=0;
    resets_seen=0;
    uint32_t other=hrtim_registers.sMasterRegs.MCR & ~(HRTIM_MCR_TACEN << test_timer);
    Motor_DRV8848_SetRaw(MOTOR_INDEX,40);
    apply_commands();
    Motor_DRV8848_SetRaw(MOTOR_INDEX,-40);
    expect(MOTOR_INDEX,0,40);
    assert(resets_seen==2);
    assert((hrtim_registers.sMasterRegs.MCR & ~(HRTIM_MCR_TACEN << test_timer)) == other);
    check_pair=false;
    puts("PASS both HRTIM inputs held idle and counter stopped during each prepared pair");

    for (unsigned id=MOTOR_WRIST_X;id<=MOTOR_WRIST_Y;++id) {
        reset(); arm(); enable((motor_id_t)id);
        first_tim=id==MOTOR_WRIST_X ? 0 : 1;
        second_tim=id==MOTOR_WRIST_X ? 0 : 2;
        second_channel=id==MOTOR_WRIST_X ? 1 : 0;
        tim_barriers=0; check_tim_pair=true;
        Motor_DRV8848_SetRaw((motor_id_t)id,40);
        Motor_DRV8848_SetRaw((motor_id_t)id,-40);
        check_tim_pair=false;
        assert(tim_barriers==4); expect((motor_id_t)id,0,40);
    }
    puts("PASS TIM15 pair and TIM16/17 pair both forced inactive before signed changes");

    reset(); uint32_t key=Motor_Lock();
    assert(MotorOutputs_HRTIMPair(&hhrtim1,0,0,0,false));
    assert(MotorOutputs_TIMPair(&htim15,TIM_CHANNEL_1,&htim15,TIM_CHANNEL_2,0,0));
    assert(!hrtim_registers.sMasterRegs.MCR && !hrtim_registers.sCommonRegs.OENR);
    assert(!timers[0].CR1 && !timers[0].CCER && !timers[0].BDTR);
    hrtim_registers.sTimerxRegs[0].TIMxCR=0;
    assert(!MotorOutputs_HRTIMPair(&hhrtim1,0,50,0,true));
    assert(!hrtim_registers.sCommonRegs.OENR);
    assert(!MotorOutputs_TIMPair(&htim16,TIM_CHANNEL_1,&htim17,TIM_CHANNEL_1,NAN,0));
    assert((timers[1].CCMR1 & TIM_CCMR1_OC1M)==TIM_OCMODE_FORCED_INACTIVE);
    assert((timers[2].CCMR1 & TIM_CCMR1_OC1M)==TIM_OCMODE_FORCED_INACTIVE);
    Motor_Unlock(key);
    puts("PASS stopped/disabled stays stopped/disabled; unsupported HRTIM config inhibited");
    return 0;
}
