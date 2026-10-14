#if N_PULSE_OLD_HW && !defined(MOTOR_BACKEND_SIM)
#include "motor_l298n.h"
#include "main.h"
#include "old_hw.h"
#include "board_link.h"
#include "motor_lock.h"
#include "motor_safety.h"
#include "encoder_bsp.h"
#include "hw_commission.h"
#include "hw_test_config.h"
#include "l298n_config.h"
#include "fatal_stop.h"
#include <math.h>
#include <string.h>

static bool initialized, armed, transitioning, register_fault;
static uint32_t armed_ms;
static int8_t direction[MOTOR_COUNT], last_direction[MOTOR_COUNT];
static uint32_t off_ms[MOTOR_COUNT];
static float applied[MOTOR_COUNT];
static uint32_t expected_ticks[MOTOR_COUNT];
static l298n_output_result_t result[MOTOR_COUNT];

static unsigned pin_shift(uint16_t pin)
{
    unsigned n = 0;
    while ((1U << n) != pin) ++n;
    return n;
}
static uint32_t pin_mode(GPIO_TypeDef *port, uint16_t pin)
{ return (port->MODER >> (2U * pin_shift(pin))) & 3U; }
static void gpio_low(GPIO_TypeDef *port, uint16_t pin)
{
    port->BSRR = (uint32_t)pin << 16;
    __DSB();
    port->OTYPER &= ~pin;
    unsigned shift = 2U * pin_shift(pin);
    port->MODER = (port->MODER & ~(3U << shift)) | (1U << shift);
}
static volatile uint32_t *compare(const old_hw_motor_pins_t *m)
{
    switch (m->channel) {
    case TIM_CHANNEL_1: return &m->timer->CCR1;
    case TIM_CHANNEL_2: return &m->timer->CCR2;
    case TIM_CHANNEL_3: return &m->timer->CCR3;
    case TIM_CHANNEL_4: return &m->timer->CCR4;
    default: return NULL;
    }
}
static bool timer_ready(TIM_TypeDef *t)
{
    if (t == TIM1 && !(RCC->APB2ENR & RCC_APB2ENR_TIM1EN)) return false;
    if (t == TIM4 && !(RCC->APB1ENR1 & RCC_APB1ENR1_TIM4EN)) return false;
    return t->PSC == 0U && t->ARR == 8499U &&
           !(t->CR1 & (TIM_CR1_DIR | TIM_CR1_CMS | TIM_CR1_UDIS)) &&
           (t != TIM1 || !(t->BDTR & (TIM_BDTR_AOE | TIM_BDTR_LOCK)));
}
static bool local(motor_id_t id)
{ return (uint32_t)id < MOTOR_COUNT && BoardLink_IsLocalMotor(id); }
static void direction_inputs(const old_hw_motor_pins_t *m, int sign)
{
    uint32_t in1 = sign>0 ? m->in1_pin : (uint32_t)m->in1_pin<<16;
    uint32_t in2 = sign<0 ? m->in2_pin : (uint32_t)m->in2_pin<<16;
    if (m->in1_port == m->in2_port) m->in1_port->BSRR = in1 | in2;
    else { m->in1_port->BSRR=in1; m->in2_port->BSRR=in2; }
    __DSB();
}
static void inactive_locked(motor_id_t id)
{
    if (!local(id)) return;
    const old_hw_motor_pins_t *m = &g_old_hw_motor_pins[id];
    gpio_low(m->enable_port,m->enable_pin); /* Disconnect AF before touching direction. */
    m->timer->CCER &= ~(1U << m->channel);
    *compare(m) = 0;
    direction_inputs(m,0);
    if (direction[id]) off_ms[id] = HAL_GetTick();
    direction[id] = 0; applied[id] = 0; expected_ticks[id] = 0;
    if (!(m->timer->CCER & 0x1111U)) {
        m->timer->CR1 &= ~TIM_CR1_CEN;
        if (m->timer == TIM1) m->timer->BDTR &= ~TIM_BDTR_MOE;
    }
    __DSB();
}
static void fail_locked(void)
{
    register_fault = true;
    MotorSafety_RequestStopCauseLocked(MOTOR_STOP_DRIVER,MOTOR_COUNT);
    /* Even if the same STOP cause was already latched, force the registers off. */
    Motor_L298N_InhibitLocked();
}
static bool consistent_locked(void)
{
    uint32_t tim1 = 0, tim4 = 0;
    for (unsigned i = 0; i < MOTOR_COUNT; ++i) {
        if (!local((motor_id_t)i)) continue;
        const old_hw_motor_pins_t *m = &g_old_hw_motor_pins[i];
        if (!timer_ready(m->timer)) return false;
        if ((m->enable_port->OTYPER & m->enable_pin) ||
            (m->in1_port->OTYPER & m->in1_pin) || (m->in2_port->OTYPER & m->in2_pin)) return false;
        if (direction[i]) {
            if (m->timer == TIM1) tim1 |= 1U << m->channel;
            else tim4 |= 1U << m->channel;
            unsigned shift = 4U * (pin_shift(m->enable_pin) % 8U);
            unsigned af = m->timer == TIM1 ? 6U : 2U;
            if (pin_mode(m->enable_port,m->enable_pin) != 2U ||
                ((m->enable_port->AFR[pin_shift(m->enable_pin)/8U] >> shift) & 15U) != af ||
                *compare(m) == 0U || *compare(m) != expected_ticks[i] || *compare(m) > 8500U) return false;
            uint32_t ccmr=m->channel<TIM_CHANNEL_3 ? m->timer->CCMR1 : m->timer->CCMR2;
            unsigned mode_shift=(m->channel==TIM_CHANNEL_2 || m->channel==TIM_CHANNEL_4) ? 8U : 0U;
            if ((ccmr & ((TIM_CCMR1_OC1M | TIM_CCMR1_OC1PE | 3U)<<mode_shift)) !=
                (TIM_OCMODE_PWM1<<mode_shift)) return false;
        } else if (pin_mode(m->enable_port,m->enable_pin) != 1U ||
                   (m->enable_port->ODR & m->enable_pin) || *compare(m)) return false;
        bool forward = direction[i] > 0, reverse = direction[i] < 0;
        if (pin_mode(m->in1_port,m->in1_pin) != 1U || pin_mode(m->in2_port,m->in2_pin) != 1U ||
            !!(m->in1_port->ODR & m->in1_pin) != forward ||
            !!(m->in2_port->ODR & m->in2_pin) != reverse) return false;
    }
    if (RCC->APB2ENR & RCC_APB2ENR_TIM1EN) {
        if (TIM1->CCER != tim1 || !!(TIM1->CR1 & TIM_CR1_CEN) != !!tim1 ||
            !!(TIM1->BDTR & TIM_BDTR_MOE) != !!tim1) return false;
    }
    return TIM4->CCER == tim4 && !!(TIM4->CR1 & TIM_CR1_CEN) == !!tim4;
}
void Motor_L298N_Init(void)
{
    uint32_t key = Motor_Lock();
    if (initialized) {
        /* Reinitialization is never permission to retain a live output. */
        MotorSafety_RequestStopCauseLocked(MOTOR_STOP_MANUAL,MOTOR_COUNT);
        Motor_L298N_InhibitLocked();
        Motor_Unlock(key); return;
    }
    OldHW_HoldOutputsLow();
    memset(direction,0,sizeof direction); memset(last_direction,0,sizeof last_direction);
    for (unsigned i = 0; i < MOTOR_COUNT; ++i) off_ms[i] = HAL_GetTick();
    initialized = OldHW_OutputsDisabled() && consistent_locked();
    if (!initialized) register_fault = true;
    Motor_Unlock(key);
    if (!initialized) Error_Handler();
}
bool Motor_L298N_FaultActive(void)
{
    uint32_t key = Motor_Lock();
    bool fault = !initialized || register_fault || g_n_pulse_fatal_cause != 0U;
    if (initialized && !consistent_locked()) fault = true;
    if (armed && (uint32_t)(HAL_GetTick()-armed_ms) >= N_PULSE_L298N_MAX_ARM_MS) fault = true;
    /* No nFAULT/current sensor exists in this mapping. This is a software/register status. */
    Motor_Unlock(key); return fault;
}
bool Motor_L298N_Arm(void)
{
#if !N_PULSE_OLD_HW_MOTION_OPT_IN
    ++g_old_hw_rejected_commands;
    return false;
#else
    /* PB14/PB15 acquisition can be monitored now; its worst IRQ latency still
     * needs qualification under real target load before driving THUMB. */
    if (N_PULSE_HW_TEST_MOTOR == MOTOR_THUMB && !N_PULSE_THUMB_EXTI_MOTION_QUALIFIED) return false;
    if (__get_IPSR() || __get_PRIMASK() || !HWCommission_Ready() ||
        !HWCommission_OutputPreflightReady()) return false;
    uint32_t key = Motor_Lock();
    if (armed || transitioning || !MotorSafety_CanArmLocked() || Motor_L298N_FaultActive()) {
        Motor_Unlock(key); return false;
    }
    uint32_t epoch = MotorSafety_EpochLocked();
    transitioning = true;
    Motor_L298N_InhibitLocked();
    Motor_Unlock(key);
    /* No blocking HAL start or delayed command exists. STOP may run here and
     * invalidate the epoch; ownership remains held until final validation. */
    bool consistent = OldHW_OutputsDisabled();
    key = Motor_Lock();
    bool ok = consistent && epoch == MotorSafety_EpochLocked() &&
        MotorSafety_CanArmLocked() && HWCommission_Ready() &&
        HWCommission_OutputPreflightReady() && !Motor_L298N_FaultActive() && consistent_locked();
    if (ok) { armed = true; armed_ms = HAL_GetTick(); }
    else Motor_L298N_InhibitLocked();
    transitioning = false;
    Motor_Unlock(key);
    return ok;
#endif
}
bool Motor_L298N_IsArmed(void)
{ uint32_t key=Motor_Lock(); bool value=armed; Motor_Unlock(key); return value; }
bool Motor_L298N_IsTransitioning(void) { return transitioning; }
void Motor_L298N_InhibitLocked(void)
{
    armed = false;
    for (unsigned i=0;i<MOTOR_COUNT;++i) {
        if (direction[i]) off_ms[i]=HAL_GetTick();
        direction[i]=0; applied[i]=0; expected_ticks[i]=0; result[i]=L298N_INHIBITED;
    }
    OldHW_HoldOutputsLow();
}
bool Motor_L298N_CleanupStoppedInternal(void)
{
    uint32_t key=Motor_Lock();
    if (armed || transitioning) { Motor_Unlock(key); return false; }
    transitioning=true;
    OldHW_HoldOutputsLow();
    bool ok=OldHW_OutputsDisabled();
    if (!ok) fail_locked();
    transitioning=false;
    Motor_Unlock(key); return ok;
}
void Motor_L298N_Disarm(void)
{
    MotorSafety_RequestStopCause(MOTOR_STOP_MANUAL,MOTOR_COUNT);
    if (!__get_PRIMASK() && !__get_IPSR()) Motor_ServiceStop();
}
#if N_PULSE_OLD_HW_MOTION_OPT_IN
static bool program_locked(motor_id_t id, float output)
{
    const old_hw_motor_pins_t *m=&g_old_hw_motor_pins[id];
    int sign=output>0 ? 1 : -1;
    if (direction[id] && direction[id]!=sign) inactive_locked(id);
    if (sign!=last_direction[id] &&
        (uint32_t)(HAL_GetTick()-off_ms[id])<N_PULSE_L298N_REVERSE_BLANK_MS) {
        result[id]=L298N_BLANKED; return consistent_locked(); /* No staged output. */
    }
    inactive_locked(id);
    uint32_t ticks=(uint32_t)(fabsf(output)*85.0f); /* ARR+1=8500; 100%=8500, zero never AF. */
    if (!ticks) { result[id]=L298N_ZERO; return consistent_locked(); }
    TIM_TypeDef *t=m->timer;
    t->CR1 &= ~TIM_CR1_CEN;
    volatile uint32_t *ccmr=m->channel<TIM_CHANNEL_3 ? &t->CCMR1 : &t->CCMR2;
    unsigned mode_shift=(m->channel==TIM_CHANNEL_2 || m->channel==TIM_CHANNEL_4) ? 8U : 0U;
    *ccmr=(*ccmr & ~((TIM_CCMR1_OC1M | TIM_CCMR1_OC1PE) << mode_shift)) |
           (TIM_OCMODE_PWM1 << mode_shift); /* Immediate CCR while ENA remains GPIO low. */
    *compare(m)=ticks;
    expected_ticks[id]=ticks;
    t->EGR=TIM_EGR_UG; t->CNT=0;
    direction_inputs(m,sign);
    unsigned pin=pin_shift(m->enable_pin), shift=4U*(pin%8U);
    m->enable_port->AFR[pin/8U]=(m->enable_port->AFR[pin/8U] & ~(15U<<shift)) |
                               ((t==TIM1 ? 6U : 2U)<<shift);
    if (t==TIM1) t->BDTR |= TIM_BDTR_MOE;
    t->CCER |= 1U<<m->channel;
    t->CR1 |= TIM_CR1_CEN;
    __DSB();
    m->enable_port->MODER=(m->enable_port->MODER & ~(3U<<(2U*pin))) | (2U<<(2U*pin));
    direction[id]=(int8_t)sign; last_direction[id]=(int8_t)sign;
    applied[id]=(float)sign*(float)ticks/85.0f;
    result[id]=L298N_APPLIED;
    return consistent_locked();
}
#endif
l298n_output_result_t Motor_L298N_ApplySigned(motor_id_t id,float output)
{
    if (!local(id)) { ++g_old_hw_rejected_commands; return L298N_INHIBITED; }
    uint32_t key=Motor_Lock();
    if (!isfinite(output)) {
        MotorSafety_RequestStopCauseLocked(MOTOR_STOP_PID,id);
        Motor_L298N_InhibitLocked(); result[id]=L298N_FAULT;
    } else if (Motor_L298N_FaultActive()) { fail_locked(); result[id]=L298N_FAULT; }
    else if (output==0.0f) {
        inactive_locked(id); result[id]=L298N_ZERO;
        if (!consistent_locked()) { fail_locked(); result[id]=L298N_FAULT; }
    }
    else {
#if N_PULSE_OLD_HW_MOTION_OPT_IN
        if (!armed || transitioning || !HWTest_MotorAllowed(id) || !Motor_OutputAllowedLocked(id) ||
            (id == MOTOR_THUMB && !N_PULSE_THUMB_EXTI_MOTION_QUALIFIED)) {
            inactive_locked(id); result[id]=L298N_INHIBITED; ++g_old_hw_rejected_commands;
        } else if (!Encoder_BSP_IsCalibrated(id) || !Motor_HasFreshPositionFeedback(id) || !HWCommission_Ready()) {
            MotorSafety_RequestStopCauseLocked(MOTOR_STOP_FEEDBACK,id);
            Motor_L298N_InhibitLocked(); result[id]=L298N_FAULT;
        } else {
            float ceiling=HWCommission_PwmCeiling();
            if (ceiling>100.0f) ceiling=100.0f;
            if (!isfinite(ceiling) || ceiling<=0.0f) { fail_locked(); result[id]=L298N_FAULT; }
            else {
                if (output>ceiling) output=ceiling;
                if (output<-ceiling) output=-ceiling;
                if (!program_locked(id,output)) { fail_locked(); result[id]=L298N_FAULT; }
            }
        }
#else
        inactive_locked(id); result[id]=L298N_INHIBITED; ++g_old_hw_rejected_commands;
#endif
    }
    l298n_output_result_t value=result[id];
    Motor_Unlock(key); return value;
}
void Motor_L298N_SetRaw(motor_id_t id,float output) { (void)Motor_L298N_ApplySigned(id,output); }
void Motor_L298N_SetSignedRaw(motor_id_t id,int sign,uint8_t speed)
{ Motor_L298N_SetRaw(id,sign>0 ? (float)speed : sign<0 ? -(float)speed : 0.0f); }
void Motor_L298N_Stop(motor_id_t id)
{
    uint32_t key=Motor_Lock();
    if(local(id)) { inactive_locked(id); result[id]=L298N_ZERO; }
    if(initialized && !consistent_locked()) fail_locked();
    Motor_Unlock(key);
}
void Motor_L298N_Brake(motor_id_t id) { Motor_L298N_Stop(id); } /* Coast only, never an active brake. */
void Motor_L298N_StopAll(void)
{
    uint32_t key=Motor_Lock();
    for(unsigned i=0;i<MOTOR_COUNT;++i) if(local((motor_id_t)i)) inactive_locked((motor_id_t)i);
    if(initialized && !consistent_locked()) fail_locked();
    Motor_Unlock(key);
}
int Motor_L298N_DrivingDir(motor_id_t id)
{ uint32_t key=Motor_Lock(); int value=local(id) ? direction[id] : 0; Motor_Unlock(key); return value; }
float Motor_L298N_AppliedPWM(motor_id_t id)
{ uint32_t key=Motor_Lock(); float value=local(id) ? applied[id] : 0; Motor_Unlock(key); return value; }
#endif
