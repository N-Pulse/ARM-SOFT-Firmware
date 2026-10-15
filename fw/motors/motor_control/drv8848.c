#ifndef MOTOR_BACKEND_SIM
#include "drv8848.h"

#include "main.h"
#include "stm32g4xx_hal.h"
#include "motor_lock.h"
#include "hw_test_config.h"
#if N_PULSE_HW_SINGLE_MOTOR_TEST
#include "hw_commission.h"
#endif
#include "motor_outputs.h"
#include "motor_safety.h"

#include <math.h>

extern HRTIM_HandleTypeDef hhrtim1;
extern TIM_HandleTypeDef htim15;
extern TIM_HandleTypeDef htim16;
extern TIM_HandleTypeDef htim17;

typedef struct {
    uint32_t hrtim_a;
    uint32_t hrtim_b;
    uint32_t hrtim_timer;
    uint32_t hrtim_unit_a;
    uint32_t hrtim_unit_b;
    TIM_HandleTypeDef *tim_a;
    uint32_t tim_channel_a;
    TIM_HandleTypeDef *tim_b;
    uint32_t tim_channel_b;
} drv_channel_t;

/* Driver inputs retain the existing channel order (board wiring still to verify). HRTIM outputs are
 * intentionally not treated as complementary pairs: A and B are independent
 * DRV8848 inputs. */
static drv_channel_t s_channels[MOTOR_COUNT] = {
    [MOTOR_INDEX]   = { HRTIM_OUTPUT_TA2, HRTIM_OUTPUT_TA1, HRTIM_TIMERINDEX_TIMER_A, HRTIM_COMPAREUNIT_2, HRTIM_COMPAREUNIT_1, NULL, 0U, NULL, 0U },
    [MOTOR_MIDDLE]  = { HRTIM_OUTPUT_TB2, HRTIM_OUTPUT_TB1, HRTIM_TIMERINDEX_TIMER_B, HRTIM_COMPAREUNIT_2, HRTIM_COMPAREUNIT_1, NULL, 0U, NULL, 0U },
    [MOTOR_RING]    = { HRTIM_OUTPUT_TF1, HRTIM_OUTPUT_TF2, HRTIM_TIMERINDEX_TIMER_F, HRTIM_COMPAREUNIT_1, HRTIM_COMPAREUNIT_2, NULL, 0U, NULL, 0U },
    [MOTOR_LITTLE]  = { HRTIM_OUTPUT_TE2, HRTIM_OUTPUT_TE1, HRTIM_TIMERINDEX_TIMER_E, HRTIM_COMPAREUNIT_2, HRTIM_COMPAREUNIT_1, NULL, 0U, NULL, 0U },
    [MOTOR_THUMB]   = { HRTIM_OUTPUT_TC1, HRTIM_OUTPUT_TC2, HRTIM_TIMERINDEX_TIMER_C, HRTIM_COMPAREUNIT_1, HRTIM_COMPAREUNIT_2, NULL, 0U, NULL, 0U },
    [MOTOR_PALM]    = { HRTIM_OUTPUT_TD2, HRTIM_OUTPUT_TD1, HRTIM_TIMERINDEX_TIMER_D, HRTIM_COMPAREUNIT_2, HRTIM_COMPAREUNIT_1, NULL, 0U, NULL, 0U },
    [MOTOR_WRIST_X] = { 0U, 0U, 0U, 0U, 0U, &htim15, TIM_CHANNEL_1, &htim15, TIM_CHANNEL_2 },
    [MOTOR_WRIST_Y] = { 0U, 0U, 0U, 0U, 0U, &htim16, TIM_CHANNEL_1, &htim17, TIM_CHANNEL_1 }
};

static int8_t s_direction[MOTOR_COUNT];
static bool s_armed;
/* One owner spans every HAL start/stop, including interrupted arm cleanup. */
static bool s_arm_in_progress;

static const uint32_t s_hrtim_outputs =
    HRTIM_OUTPUT_TA1 | HRTIM_OUTPUT_TA2 |
    HRTIM_OUTPUT_TB1 | HRTIM_OUTPUT_TB2 |
    HRTIM_OUTPUT_TC1 | HRTIM_OUTPUT_TC2 |
    HRTIM_OUTPUT_TD1 | HRTIM_OUTPUT_TD2 |
    HRTIM_OUTPUT_TE1 | HRTIM_OUTPUT_TE2 |
    HRTIM_OUTPUT_TF1 | HRTIM_OUTPUT_TF2;

static const uint32_t s_hrtim_timers =
    HRTIM_TIMERID_TIMER_A | HRTIM_TIMERID_TIMER_B |
    HRTIM_TIMERID_TIMER_C | HRTIM_TIMERID_TIMER_D |
    HRTIM_TIMERID_TIMER_E | HRTIM_TIMERID_TIMER_F;

static void set_sleep(GPIO_PinState state)
{
    HAL_GPIO_WritePin(GPIOG, GPIO_PIN_0 | GPIO_PIN_2 | GPIO_PIN_4, state);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, state);
}

bool Motor_DRV8848_FaultActive(void)
{
    return HAL_GPIO_ReadPin(GPIOG, GPIO_PIN_1) == GPIO_PIN_RESET ||
           HAL_GPIO_ReadPin(GPIOG, GPIO_PIN_3) == GPIO_PIN_RESET ||
           HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_12) == GPIO_PIN_RESET ||
           HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_10) == GPIO_PIN_RESET;
}

static bool set_compare(motor_id_t id, float duty_a, float duty_b)
{
    const drv_channel_t *channel = &s_channels[id];
    bool ok;
    if (channel->tim_a != NULL) {
        ok = MotorOutputs_TIMPair(channel->tim_a, channel->tim_channel_a,
                                  channel->tim_b, channel->tim_channel_b, duty_a, duty_b);
    } else {
        bool a_is_one = channel->hrtim_unit_a == HRTIM_COMPAREUNIT_1;
        ok = MotorOutputs_HRTIMPair(&hhrtim1, channel->hrtim_timer,
                a_is_one ? duty_a : duty_b, a_is_one ? duty_b : duty_a, s_armed);
    }
    if (!ok) Error_Handler(); /* Unsupported peripheral configuration: fatal cut. */
    if (channel->tim_a != NULL)
        return __HAL_TIM_GET_COMPARE(channel->tim_a, channel->tim_channel_a) != 0U ||
               __HAL_TIM_GET_COMPARE(channel->tim_b, channel->tim_channel_b) != 0U;
    const HRTIM_Timerx_TypeDef *t = &hhrtim1.Instance->sTimerxRegs[channel->hrtim_timer];
    return (t->SETx1R & HRTIM_SET1R_PER) != 0U || (t->SETx2R & HRTIM_SET2R_PER) != 0U;
}

void Motor_DRV8848_Init(void)
{
    s_armed = false;
    s_arm_in_progress = false;
    for (uint32_t i = 0U; i < MOTOR_COUNT; ++i)
    {
        s_direction[i] = 0;
        set_compare((motor_id_t)i, 0.0f, 0.0f);
    }
    set_sleep(GPIO_PIN_RESET);
}

static bool cleanup_hal(void)
{
    /* Transition owner only. Execute ALL cleanup steps, even after errors. */
    bool ok = HAL_HRTIM_WaveformOutputStop(&hhrtim1, s_hrtim_outputs) == HAL_OK;
    ok = (HAL_HRTIM_WaveformCountStop(&hhrtim1, s_hrtim_timers) == HAL_OK) && ok;
    ok = (HAL_TIM_PWM_Stop(&htim15, TIM_CHANNEL_1) == HAL_OK) && ok;
    ok = (HAL_TIM_PWM_Stop(&htim15, TIM_CHANNEL_2) == HAL_OK) && ok;
    ok = (HAL_TIM_PWM_Stop(&htim16, TIM_CHANNEL_1) == HAL_OK) && ok;
    ok = (HAL_TIM_PWM_Stop(&htim17, TIM_CHANNEL_1) == HAL_OK) && ok;
    return ok;
}

static bool arm_current(uint32_t epoch)
{
    uint32_t key = Motor_Lock();
    bool current = epoch == MotorSafety_EpochLocked() &&
                   MotorSafety_CanArmLocked() && !Motor_DRV8848_FaultActive();
    Motor_Unlock(key);
    return current;
}

bool Motor_DRV8848_Arm(void)
{
#if N_PULSE_HW_SINGLE_MOTOR_TEST
    if (!HWCommission_Ready()) return false;
#endif
    uint32_t key = Motor_Lock();
    if (s_armed || s_arm_in_progress || !MotorSafety_CanArmLocked() || Motor_DRV8848_FaultActive()) {
        Motor_Unlock(key);
        return false;
    }
    uint32_t epoch = MotorSafety_EpochLocked();
    s_arm_in_progress = true;
    for (uint32_t i = 0U; i < MOTOR_COUNT; ++i) set_compare((motor_id_t)i, 0.0f, 0.0f);
    Motor_Unlock(key);

    /* HAL calls may wait: keep ownership, never PRIMASK, across each call.
     * STOP can inhibit at any point; the epoch forbids subsequent SLEEP high. */
#define ARM_STEP(call) do { \
    if (!arm_current(epoch)) goto cancelled; \
    if ((call) != HAL_OK) { \
        MotorSafety_RequestStopCause(MOTOR_STOP_DRIVER, MOTOR_COUNT); \
        goto cancelled; \
    } \
} while (0)
    ARM_STEP(HAL_HRTIM_WaveformCountStart(&hhrtim1, s_hrtim_timers));
    ARM_STEP(HAL_HRTIM_WaveformOutputStart(&hhrtim1, s_hrtim_outputs));
    ARM_STEP(HAL_TIM_PWM_Start(&htim15, TIM_CHANNEL_1));
    ARM_STEP(HAL_TIM_PWM_Start(&htim15, TIM_CHANNEL_2));
    ARM_STEP(HAL_TIM_PWM_Start(&htim16, TIM_CHANNEL_1));
    ARM_STEP(HAL_TIM_PWM_Start(&htim17, TIM_CHANNEL_1));
#undef ARM_STEP
    key = Motor_Lock();
    bool allowed = epoch == MotorSafety_EpochLocked() && MotorSafety_CanArmLocked() &&
                   !Motor_DRV8848_FaultActive();
    if (allowed) set_sleep(GPIO_PIN_SET);
    Motor_Unlock(key);
    if (!allowed) goto cancelled;
    HAL_Delay(1U);
    key = Motor_Lock();
    allowed = epoch == MotorSafety_EpochLocked() && MotorSafety_CanArmLocked() &&
              !Motor_DRV8848_FaultActive();
    if (allowed) {
        s_armed = true;
        s_arm_in_progress = false;
    }
    Motor_Unlock(key);
    if (allowed) return true;
cancelled:
    /* A newly asserted physical fault also invalidates all controller state. */
    if (Motor_DRV8848_FaultActive()) MotorSafety_RequestStopCause(MOTOR_STOP_DRIVER, MOTOR_COUNT);
    key = Motor_Lock();
    Motor_DRV8848_InhibitLocked();
    Motor_Unlock(key);
    if (!cleanup_hal()) MotorSafety_RequestStopCause(MOTOR_STOP_DRIVER, MOTOR_COUNT);
    /* Pending stop work checks/retries cleanup before recovery. */
    key = Motor_Lock();
    s_arm_in_progress = false;
    Motor_Unlock(key);
    return false;
}

void Motor_DRV8848_InhibitLocked(void)
{
    s_armed = false;
    set_sleep(GPIO_PIN_RESET);
    Motor_DRV8848_StopAll();
}

bool Motor_DRV8848_CleanupStoppedInternal(void)
{
    uint32_t key = Motor_Lock();
    if (s_arm_in_progress || s_armed) {
        Motor_Unlock(key);
        return false;
    }
    s_arm_in_progress = true;
    Motor_Unlock(key);
    bool ok = cleanup_hal();
    key = Motor_Lock();
    if (!ok) MotorSafety_RequestStopCauseLocked(MOTOR_STOP_DRIVER, MOTOR_COUNT);
    s_arm_in_progress = false;
    Motor_Unlock(key);
    return ok;
}

void Motor_DRV8848_Disarm(void)
{
    /* Public safe disarm is the global interlock, never just HAL cleanup. */
    MotorSafety_RequestStopCause(MOTOR_STOP_MANUAL, MOTOR_COUNT);
    if (!__get_PRIMASK() && !__get_IPSR()) Motor_ServiceStop();
}

bool Motor_DRV8848_IsArmed(void)
{
    uint32_t key = Motor_Lock();
    bool armed = s_armed;
    Motor_Unlock(key);
    return armed;
}

bool Motor_DRV8848_IsTransitioning(void) { return s_arm_in_progress; }

void Motor_DRV8848_SetRaw(motor_id_t id, float output_percent)
{
    if ((uint32_t)id >= MOTOR_COUNT) return;
#if N_PULSE_HW_SINGLE_MOTOR_TEST
    if (!HWTest_MotorAllowed(id)) { Motor_DRV8848_Stop(id); return; }
#endif
    if (!isfinite(output_percent)) {
        MotorSafety_RequestStopCause(MOTOR_STOP_PID, id);
        return;
    }
    if (output_percent > 100.0f) output_percent = 100.0f;
    if (output_percent < -100.0f) output_percent = -100.0f;
#if N_PULSE_HW_SINGLE_MOTOR_TEST
    float ceiling=HWCommission_PwmCeiling();
    if (output_percent > ceiling) output_percent = ceiling;
    if (output_percent < -ceiling) output_percent = -ceiling;
#endif
    uint32_t key = Motor_Lock();
    if (Motor_DRV8848_FaultActive())
    {
        MotorSafety_RequestStopCauseLocked(MOTOR_STOP_DRIVER, MOTOR_COUNT);
        Motor_Unlock(key);
        return;
    }
    if (!s_armed || !Motor_OutputAllowedLocked(id)) {
        Motor_Unlock(key);
        return;
    }

    /* Pair writer holds both inputs inactive while preparing the new state.
     * This specifies MCU levels; electrical switching still requires measurement. */
    if (output_percent > 0.0f)
    {
        bool driving = set_compare(id, output_percent, 0.0f);
        s_direction[id] = driving ? 1 : 0;
    }
    else if (output_percent < 0.0f)
    {
        bool driving = set_compare(id, 0.0f, -output_percent);
        s_direction[id] = driving ? -1 : 0;
    }
    else
    {
        set_compare(id, 0.0f, 0.0f);
        s_direction[id] = 0;
    }
    Motor_Unlock(key);
}

void Motor_DRV8848_SetSignedRaw(motor_id_t id, int direction, uint8_t speed_percent)
{
    float output = (float)speed_percent;
    if (direction < 0) output = -output;
    if (direction == 0) output = 0.0f;
    Motor_DRV8848_SetRaw(id, output);
}

void Motor_DRV8848_Stop(motor_id_t id)
{
    if ((uint32_t)id >= MOTOR_COUNT) return;
    uint32_t key = Motor_Lock();
    set_compare(id, 0.0f, 0.0f);
    s_direction[id] = 0;
    Motor_Unlock(key);
}

void Motor_DRV8848_Brake(motor_id_t id)
{
    if ((uint32_t)id >= MOTOR_COUNT) return;
#if N_PULSE_HW_SINGLE_MOTOR_TEST
    Motor_DRV8848_Stop(id); /* Commissioning never applies a 100% brake. */
    return;
#endif
    uint32_t key = Motor_Lock();
    if (Motor_DRV8848_FaultActive())
        MotorSafety_RequestStopCauseLocked(MOTOR_STOP_DRIVER, MOTOR_COUNT);
    else if (s_armed && Motor_OutputAllowedLocked(id)) {
        set_compare(id, 100.0f, 100.0f);
        s_direction[id] = 0;
    }
    Motor_Unlock(key);
}

void Motor_DRV8848_StopAll(void)
{
    for (uint32_t i = 0U; i < MOTOR_COUNT; ++i)
    {
        Motor_DRV8848_Stop((motor_id_t)i);
    }
}

int Motor_DRV8848_DrivingDir(motor_id_t id)
{
    if ((uint32_t)id >= MOTOR_COUNT) return 0;
    uint32_t key = Motor_Lock();
    int direction = s_direction[id];
    Motor_Unlock(key);
    return direction;
}

#endif /* Physical driver is unavailable in SIM, including direct Arm calls. */
