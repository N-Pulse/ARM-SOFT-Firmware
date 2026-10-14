/* Actual OLD_HW startup + BSP + controller/PID/guard/STOP/commissioning.
 * Only peripheral/clock/ADC boundaries are modeled. Never opens hardware. */
#define OLD_HW_INTEGRATION
#include "test_old_hw.c"
#include "motor_l298n.h"
#include "hw_commission.h"
#include "comms_session.h"
#include "motor_pid.h"
#include "motor_guard.h"
#include "adc_dma_bsp.h"
#include "thumb_encoder.h"
#include "motor_estimation.h"
/* Include production driver to exercise its private register primitive at 100%
 * as well as its public safety gates. No production test hooks are introduced. */
#include "../../fw/motors/motor_l298n/motor_l298n.c"

static motor_id_t selected=(motor_id_t)N_PULSE_HW_TEST_MOTOR;
static unsigned request;
static void *stop_entry, *stop_exit;
void __cyg_profile_func_enter(void *,void *) __attribute__((no_instrument_function));
void __cyg_profile_func_exit(void *,void *) __attribute__((no_instrument_function));
void __cyg_profile_func_enter(void *fn,void *caller)
{ (void)caller; if(fn==stop_entry) { stop_entry=NULL; assert(!masked); Motor_StopAll(); } }
void __cyg_profile_func_exit(void *fn,void *caller)
{ (void)caller; if(fn==stop_exit) { stop_exit=NULL; assert(!masked); Motor_StopAll(); } }
bool ADC_DMA_BSP_Init(void) { return true; } /* Acquisition intentionally inactive. */
bool ADC_DMA_BSP_GetSample(motor_id_t id,adc_current_sample_t *out)
{ (void)id; *out=(adc_current_sample_t){.voltage_v=NAN,.current_a=NAN,.state=ADC_CURRENT_INVALID}; return false; }
static uint32_t command(hw_command_t c)
{
    c.motor_id=selected; c.request_id=++request; c.epoch=MotorSafety_EpochLocked();
    return HWCommission_Execute(&c,now,c.epoch);
}
static void hello(void)
{
    bool started; rx_result_t msg={.type=RX_TYPE_HELLO,.proto_version=1,.data.device_id=1};
    assert(CommsSession_OnMessage(&msg,now,MotorSafety_EpochLocked(),&started));
}
static void advance(unsigned ms)
{ now+=ms; Motor_Update(); MotorGuard_Poll(); }
static void stopped(void)
{
    assert(!Motor_L298N_IsArmed() && OldHW_OutputsDisabled());
    for(unsigned i=0;i<MOTOR_COUNT;++i) assert(!Motor_IsEnabled((motor_id_t)i) && !Motor_L298N_DrivingDir((motor_id_t)i));
}
static void configure(void)
{
    hello();
    assert(command((hw_command_t){.operation=1,.minimum_rad=0,.maximum_rad=.3f,
        .maximum_step_rad=.05f,.pwm_percent=3,.reference_rate_rad_s=.1f})==HW_OK);
    assert(command((hw_command_t){.operation=2,.cpr=1000,.quadrature=4,.gear_ratio=1,.direction=1})==HW_OK);
    assert(command((hw_command_t){.operation=3})==HW_OK);
    advance(10); advance(10);
    assert(command((hw_command_t){.operation=5})==HW_REJECTED); /* No preflight acknowledgement. */
    assert(command((hw_command_t){.operation=11})==HW_REJECTED);
    assert(command((hw_command_t){.operation=11,.stationary_reference_verified=true})==HW_OK);
}
static void enable(void)
{
    assert(command((hw_command_t){.operation=5})==HW_OK);
    assert(OldHW_OutputsDisabled()); /* Arm is only readiness, never a nonzero command. */
    encoder_calibration_t forbidden={1000,4,1,1,0};
    assert(!Encoder_BSP_SetCalibration(selected,&forbidden) && !Encoder_BSP_Reset(selected));
    assert(command((hw_command_t){.operation=6})==HW_OK);
    assert(OldHW_OutputsDisabled());
    assert(command((hw_command_t){.operation=7,.target_rad=.02f,.pwm_percent=3})==HW_OK);
    advance(10); advance(10); /* First 0.01% PID step is below one 20-kHz timer tick. */
    assert(Motor_L298N_AppliedPWM(selected)>0 && Motor_L298N_AppliedPWM(selected)<=3);
}
static void thumb_edge(uint8_t ab,uint8_t pending)
{
    GPIOB->IDR=((uint32_t)(ab&2U)<<13) | ((uint32_t)(ab&1U)<<15);
    EXTI->PR1=((uint32_t)(pending&2U)<<13) | ((uint32_t)(pending&1U)<<15) | GPIO_PIN_13;
    ThumbEncoder_IRQHandler();
    assert(EXTI->PR1 != GPIO_PIN_13); /* Decoder W1C command never clears line 13. */
    assert(!(EXTI->PR1 & GPIO_PIN_13));
    EXTI->PR1=GPIO_PIN_13; /* Model W1C semantics, preserving shared pending bit. */
}
static void valid_thumb_irq(void) { thumb_edge(1,1); }
int main(int argc,char **argv)
{
    const char *test=argc>1 ? argv[1] : "pid";
    slave=selected!=MOTOR_LITTLE;
    fail_exti=!strcmp(test,"thumb-init-fault");
    if(setjmp(fatal)) {
        assert(fail_exti && OldHW_OutputsDisabled());
        puts("PASS THUMB EXTI configuration readback failure aborts startup, outputs disabled; exit=0");
        return 0;
    }
    if(!strcmp(test,"wrap")) now=UINT32_MAX-25U;
    BoardLink_Init(); OldHW_HoldOutputsLow(); OldHW_ClockConfig(); OldHW_Init();
    Motor_InitAll(); CommsSession_Init(true); stopped();
    if(!strcmp(test,"registers")) {
        now+=3;
        for(unsigned i=0;i<MOTOR_COUNT;++i) if(BoardLink_IsLocalMotor((motor_id_t)i)) {
            uint32_t key=Motor_Lock();
            assert(program_locked((motor_id_t)i,100));
            const old_hw_motor_pins_t *m=&g_old_hw_motor_pins[i];
            assert(*compare(m)==8500 && direction[i]==1);
            inactive_locked((motor_id_t)i);
            assert(OldHW_OutputsDisabled());
            assert(program_locked((motor_id_t)i,.00001f) && OldHW_OutputsDisabled());
            Motor_Unlock(key);
            now+=3;
            key=Motor_Lock(); assert(program_locked((motor_id_t)i,-100));
            assert(*compare(m)==8500 && direction[i]==-1);
            inactive_locked((motor_id_t)i); Motor_Unlock(key); now+=3;
        }
        puts("PASS all local TIM1/TIM4 channels, GPIO direction, AF, CCR=ARR+1 and no complementary outputs; exit=0");
        fatal_checks(); return 0;
    }
    configure();
    if(!strcmp(test,"thumb-unqualified")) {
        assert(selected==MOTOR_THUMB && !N_PULSE_THUMB_EXTI_MOTION_QUALIFIED);
        assert(command((hw_command_t){.operation=5})==HW_REJECTED); stopped();
    } else if(!strcmp(test,"arm-stop")) {
        stop_entry=(void *)OldHW_OutputsDisabled;
        assert(command((hw_command_t){.operation=5})==HW_REJECTED);
        assert(!Motor_L298N_IsTransitioning()); stopped();
    } else if(!strcmp(test,"deadline")) {
        assert(command((hw_command_t){.operation=5})==HW_OK);
        for(unsigned i=0;i<501;++i) { if(i%50==0) hello(); advance(10); }
        assert(MotorSafety_GetStopCauses() & MOTOR_STOP_DRIVER); stopped();
    } else if(!strcmp(test,"thumb-irq-fault")) {
        enable(); HAL_NVIC_DisableIRQ(EXTI15_10_IRQn); MotorSafety_Poll();
        stopped(); assert(!Encoder_BSP_Reset(selected));
        encoder_snapshot_t sample; assert(!Encoder_BSP_GetSnapshot(selected,&sample));
        assert(isnan(sample.position_rad));
    } else if(!strcmp(test,"thumb")) {
        assert(selected==MOTOR_THUMB && Encoder_BSP_IsLocal(selected));
        int64_t before=Encoder_BSP_GetRawCount(selected);
        deferred_irq=valid_thumb_irq;
        assert(Encoder_BSP_GetRawCount(selected)==before);
        assert(Encoder_BSP_GetRawCount(selected)==before+1); /* Delivered only after PRIMASK restoration. */
        assert(Encoder_BSP_Reset(selected)); advance(10);
        enable(); thumb_edge(2,3); /* Coalesced/lost ordering: immediate STOP from IRQ. */
        stopped(); assert(!Encoder_BSP_IsCalibrated(selected));
        encoder_snapshot_t sample; assert(!Encoder_BSP_GetSnapshot(selected,&sample));
        assert(isnan(sample.position_rad) && g_thumb_encoder_invalid_transitions==1);
        Motor_ServiceStop(); assert(command((hw_command_t){.operation=4})==HW_FAULT);
        assert(command((hw_command_t){.operation=3})==HW_OK); advance(10);
        assert(command((hw_command_t){.operation=4})==HW_OK);
        stopped(); assert(!HWCommission_OutputPreflightReady());
    } else {
        enable();
        if(!strcmp(test,"reverse") || !strcmp(test,"wrap")) {
            assert(Motor_L298N_ApplySigned(selected,1000)==L298N_APPLIED);
            assert(fabsf(Motor_L298N_AppliedPWM(selected)-3)<.001);
            assert(Motor_L298N_ApplySigned(selected,-1000)==L298N_BLANKED);
            assert(OldHW_OutputsDisabled()); now+=2;
            assert(Motor_L298N_ApplySigned(selected,-1000)==L298N_APPLIED);
            assert(Motor_L298N_AppliedPWM(selected)<0);
            assert(Motor_L298N_ApplySigned(selected,0)==L298N_ZERO && OldHW_OutputsDisabled());
            Motor_StopAll(); stopped();
        } else if(!strcmp(test,"pid-stop")) {
            stop_exit=(void *)MotorPID_Update; advance(10); assert(!stop_exit); stopped();
        } else if(!strcmp(test,"concurrent")) {
            uint32_t epoch=MotorSafety_EpochLocked(); deferred_irq=Motor_StopAll;
            (void)Motor_L298N_ApplySigned(selected,3); stopped();
            assert(epoch!=MotorSafety_EpochLocked());
            assert(!Motor_SetTargetAtEpoch(selected,.02f,3,epoch));
            assert(Motor_L298N_ApplySigned(selected,3)!=L298N_APPLIED);
        } else if(!strcmp(test,"stale")) {
            now+=101; assert(!Motor_HasFreshPositionFeedback(selected)); stopped();
        } else if(!strcmp(test,"encoder-fault")) {
            TIM_TypeDef *t=selected==MOTOR_LITTLE ? TIM3 : selected==MOTOR_INDEX ? TIM2 : TIM8;
            t->CR1 &= ~TIM_CR1_CEN; MotorSafety_Poll(); stopped();
            assert(MotorSafety_GetStopCauses() & MOTOR_STOP_FEEDBACK);
        } else if(!strcmp(test,"register-fault")) {
            g_old_hw_motor_pins[selected].timer->ARR=0; MotorSafety_Poll(); stopped();
            assert(MotorSafety_GetStopCauses() & MOTOR_STOP_DRIVER);
            assert(command((hw_command_t){.operation=4})==HW_FAULT);
        } else if(!strcmp(test,"stall")) {
            for(unsigned i=0;i<25;++i) advance(10);
            assert(MotorSafety_GetStopCauses() & MOTOR_STOP_GUARD); stopped();
        } else if(!strcmp(test,"nan")) {
            assert(Motor_L298N_ApplySigned(selected,NAN)==L298N_FAULT); stopped();
            assert(Motor_L298N_ApplySigned(selected,INFINITY)==L298N_FAULT); stopped();
        } else if(!strcmp(test,"reinit")) {
            uint32_t epoch=MotorSafety_EpochLocked(); Motor_L298N_Init();
            assert(epoch!=MotorSafety_EpochLocked()); stopped();
        } else {
            for(unsigned i=0;i<MOTOR_COUNT;++i) if(i!=(unsigned)selected) {
                assert(!Motor_Enable((motor_id_t)i));
                assert(Motor_L298N_ApplySigned((motor_id_t)i,100)!=L298N_APPLIED);
            }
            assert(!Encoder_BSP_Reset(selected)); Motor_L298N_Disarm(); stopped();
            advance(10); assert(command((hw_command_t){.operation=4})==HW_OK);
            assert(!HWCommission_OutputPreflightReady());
            assert(command((hw_command_t){.operation=5})==HW_REJECTED);
            assert(command((hw_command_t){.operation=11,.stationary_reference_verified=true})==HW_OK);
            assert(command((hw_command_t){.operation=5})==HW_OK); assert(OldHW_OutputsDisabled());
            assert(command((hw_command_t){.operation=6})==HW_OK); advance(10);
            assert(OldHW_OutputsDisabled()); /* Old target/PWM never resumes. */
            Motor_StopAll(); stopped();
        }
    }
    assert(!masked);
    printf("PASS OLD_HW real pipeline motor=%u case=%s exit=0\n",(unsigned)selected,test);
    return 0;
}
