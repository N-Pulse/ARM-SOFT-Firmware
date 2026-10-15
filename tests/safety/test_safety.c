#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "main.h"
#include "motor_control.h"
#include "motor_backend.h"
#include "motor_safety.h"
#include "motor_safety_config.h"
#include "motor_guard.h"
#include "encoder_bsp.h"
#include "drv8848.h"
#include "adc_dma_bsp.h"

GPIO_TypeDef mock_gpio[3];
static TIM_TypeDef timers[3];
TIM_HandleTypeDef htim15 = { &timers[0] };
TIM_HandleTypeDef htim16 = { &timers[1] };
TIM_HandleTypeDef htim17 = { &timers[2] };
static HRTIM_TypeDef hrtim_registers;
HRTIM_HandleTypeDef hhrtim1;
#ifndef OUTPUTS_TEST_DSB
void __DSB(void) { }
#endif
static uint32_t clock_ms, primask, positive_writes, starts, cleanups;
static bool setup, hardware_fault;
static bool stop_in_arm_delay;
static unsigned stop_at_start, fail_at_start, probe_cleanup_at, fail_cleanup_at;
static bool cleanup_probe_without_stop;
static void start_injection(void) {
    if (starts == stop_at_start) { Motor_StopAll(); Motor_ServiceStop(); }
}
static void cleanup_injection(void) {
    if (cleanups == probe_cleanup_at) {
        assert(!Motor_DRV8848_Arm());
        assert(!Motor_RecoverFromFault());
        if (!cleanup_probe_without_stop) Motor_DRV8848_Disarm();
    }
}
static bool guard_injection;
static motor_id_t last_raw_id;
static bool calibrated[MOTOR_COUNT], local[MOTOR_COUNT];
static int64_t raw[MOTOR_COUNT];
static float positions[MOTOR_COUNT];
static int injection;
static unsigned scenarios;

#ifndef TEST_COMMS_FIXTURE
uint32_t __get_IPSR(void) { return 0U; }
#endif
uint32_t __get_PRIMASK(void) { return primask; }
void __disable_irq(void) { primask = 1U; }
#ifdef TEST_COMMS_FIXTURE
static void comms_restore_irq(void) __attribute__((no_instrument_function));
static void comms_instrument_exit(void *) __attribute__((no_instrument_function));
#endif
void __set_PRIMASK(uint32_t previous) {
    primask = previous;
#ifdef TEST_COMMS_FIXTURE
    if (!previous) comms_restore_irq();
#endif
}
uint32_t HAL_GetTick(void) { return clock_ms; }
void HAL_Delay(uint32_t delay)
{
    assert(!primask); clock_ms += delay;
    if (stop_in_arm_delay) { stop_in_arm_delay = false; Motor_StopAll(); }
}
void Error_Handler(void) { abort(); }
void mock_compare_write(uint32_t *reg, uint32_t value)
{
    assert(setup || primask);
    bool parked_hrtim = (uintptr_t)reg >= (uintptr_t)&hhrtim1.compare[0][0] &&
                        (uintptr_t)reg <= (uintptr_t)&hhrtim1.compare[5][1] && value == 3U;
    if (value && !parked_hrtim) ++positive_writes;
    *reg = value;
}
void HAL_GPIO_WritePin(GPIO_TypeDef *gpio, uint32_t pins, GPIO_PinState state)
{
    if (state == GPIO_PIN_SET) { assert(!MotorSafety_IsFaultActive()); gpio->pins |= pins; }
    else gpio->pins &= ~pins;
}
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *gpio, uint32_t pin)
{
    (void)gpio; (void)pin;
    return hardware_fault ? GPIO_PIN_RESET : GPIO_PIN_SET;
}
HAL_StatusTypeDef HAL_HRTIM_WaveformCountStart(HRTIM_HandleTypeDef *h, uint32_t bits)
{ assert(!primask); ++starts; start_injection(); h->Instance->sMasterRegs.MCR |= bits << 17; return starts == fail_at_start ? HAL_ERROR : HAL_OK; }
HAL_StatusTypeDef HAL_HRTIM_WaveformOutputStart(HRTIM_HandleTypeDef *h, uint32_t bits)
{ assert(!primask); ++starts; start_injection(); h->Instance->sCommonRegs.ODSR &= ~bits; return starts == fail_at_start ? HAL_ERROR : HAL_OK; }
HAL_StatusTypeDef HAL_TIM_PWM_Start(TIM_HandleTypeDef *h, uint32_t ch)
{ assert(!primask); ++starts; start_injection(); h->Instance->CR1 |= TIM_CR1_CEN; h->Instance->CCER |= 1U << (ch * 4U); h->Instance->BDTR |= TIM_BDTR_MOE; return starts == fail_at_start ? HAL_ERROR : HAL_OK; }
HAL_StatusTypeDef HAL_HRTIM_WaveformCountStop(HRTIM_HandleTypeDef *h, uint32_t bits)
{ assert(!primask); ++cleanups; cleanup_injection(); h->Instance->sMasterRegs.MCR &= ~(bits << 17); return cleanups == fail_cleanup_at ? HAL_ERROR : HAL_OK; }
HAL_StatusTypeDef HAL_HRTIM_WaveformOutputStop(HRTIM_HandleTypeDef *h, uint32_t bits)
{ assert(!primask); ++cleanups; cleanup_injection(); h->Instance->sCommonRegs.ODSR |= bits; return cleanups == fail_cleanup_at ? HAL_ERROR : HAL_OK; }
HAL_StatusTypeDef HAL_TIM_PWM_Stop(TIM_HandleTypeDef *h, uint32_t ch)
{ assert(!primask); ++cleanups; cleanup_injection(); h->Instance->CCER &= ~(1U << (ch * 4U)); h->Instance->CR1 &= ~TIM_CR1_CEN; h->Instance->BDTR &= ~TIM_BDTR_MOE; return cleanups == fail_cleanup_at ? HAL_ERROR : HAL_OK; }

#ifndef TEST_REAL_BSP
bool Encoder_BSP_Init(void) { return true; }
bool Encoder_BSP_IsCalibrated(motor_id_t id) { return calibrated[id]; }
bool Encoder_BSP_IsLocal(motor_id_t id) { return local[id]; }
int64_t Encoder_BSP_GetRawCount(motor_id_t id) { last_raw_id = id; return raw[id]; }
float Encoder_BSP_GetPosition(motor_id_t id) { return positions[id]; }
float Encoder_BSP_GetVelocity(motor_id_t id) { (void)id; return 0.0f; }
bool Encoder_BSP_GetSnapshot(motor_id_t id, encoder_snapshot_t *out) {
    *out = (encoder_snapshot_t){raw[id], positions[id], 0.0f, calibrated[id]};
    return out->valid;
}
#endif
bool ADC_DMA_BSP_Init(void) { return true; }
bool ADC_DMA_BSP_GetSample(motor_id_t id,adc_current_sample_t *out) {
    (void)id;
    *out=(adc_current_sample_t){.current_a=NAN,.voltage_v=NAN,.age_ms=UINT32_MAX};
    return true;
}
#ifndef TEST_HW_BACKEND
/* I/O fixture only: asynchronous delivery can be withheld independently of
 * time/guard motion. All validation and stop logic are production functions. */
void MotorBackend_Init(void) { }
void MotorBackend_RefreshFeedback(void) { }
void MotorBackend_InhibitOutputsLocked(void) { assert(primask); }
void MotorBackend_StopAll(void) { assert(!primask); Motor_DRV8848_StopAll(); }
void MotorBackend_Flush(void) { assert(!primask); }
void MotorBackend_ApplyOutput(motor_id_t id, float ref, float output)
{ (void)ref; Motor_DRV8848_SetRaw(id, output); }
#endif

#ifdef TEST_REAL_BSP
static void reference_injection(int);
#endif
static void position(motor_id_t id, float value)
{
    motor_feedback_t fb = { .id = id, .position = value, .velocity = 0.0f,
                           .valid_fields = MOTOR_FB_POSITION | MOTOR_FB_VELOCITY };
    MotorBackend_OnFeedback(&fb);
}
static void force(motor_id_t id, uint16_t value)
{
    motor_feedback_t fb = { .id = id, .position = NAN, .velocity = NAN,
                           .force_mN = value, .valid_fields = MOTOR_FB_FORCE };
    MotorBackend_OnFeedback(&fb);
}
static void deliver_positions(void)
{
    for (unsigned i = 0; i < MOTOR_COUNT; ++i) position((motor_id_t)i, positions[i]);
}
static void step(uint32_t elapsed, bool deliver)
{
    clock_ms += elapsed;
    if (deliver) deliver_positions();
    Motor_Update();
    assert(!primask);
}
static void reset_at(uint32_t now)
{
    clock_ms = now; primask = 0; positive_writes = starts = cleanups = 0;
    injection = 0; hardware_fault = false;
    stop_at_start = fail_at_start = probe_cleanup_at = fail_cleanup_at = 0U;
    cleanup_probe_without_stop = false;
    stop_in_arm_delay = false;
    guard_injection = false;
    memset(mock_gpio, 0, sizeof(mock_gpio));
    memset(timers, 0, sizeof(timers));
    memset(&hhrtim1, 0, sizeof(hhrtim1));
    memset(&hrtim_registers, 0, sizeof(hrtim_registers));
    hhrtim1.Instance = &hrtim_registers;
    hrtim_registers.sCommonRegs.ODSR = 0xFFFU;
    for (unsigned i = 0; i < 6; ++i) {
        hrtim_registers.sTimerxRegs[i].PERxR = 7999U;
        hrtim_registers.sTimerxRegs[i].TIMxCR = HRTIM_PRESCALERRATIO_DIV1;
    }
    for (unsigned i = 0; i < 3; ++i) timers[i].ARR = 7999U;
    memset(raw, 0, sizeof(raw));
    memset(positions, 0, sizeof(positions));
    for (unsigned i = 0; i < MOTOR_COUNT; ++i) calibrated[i] = local[i] = true;
    setup = true;
    Motor_InitAll();
    setup = false;
    deliver_positions();
    Motor_Update();
    step(10U, true);
}
static void reset(void) { reset_at(1000U); }
static void assert_stopped(void)
{
    assert(!Motor_DRV8848_IsArmed());
    assert((mock_gpio[0].pins & (GPIO_PIN_0 | GPIO_PIN_2 | GPIO_PIN_4)) == 0U);
    assert((mock_gpio[1].pins & GPIO_PIN_4) == 0U);
    for (unsigned i = 0; i < MOTOR_COUNT; ++i) {
        assert(!Motor_IsEnabled((motor_id_t)i));
        assert(Motor_DRV8848_DrivingDir((motor_id_t)i) == 0);
    }
    for (unsigned i = 0; i < 6; ++i)
        for (unsigned j = 0; j < 2; ++j) assert(hhrtim1.compare[i][j] == 3U);
    for (unsigned i = 0; i < 6; ++i) {
        assert(hrtim_registers.sTimerxRegs[i].SETx1R == 0U);
        assert(hrtim_registers.sTimerxRegs[i].SETx2R == 0U);
    }
    for (unsigned i = 0; i < 3; ++i)
        for (unsigned j = 0; j < 2; ++j) assert(timers[i].compare[j] == 0U);
    assert(!primask);
}
static void arm(void)
{
    /* Explicit fixture action only: firmware default has no Arm caller. */
    assert(Motor_DRV8848_Arm());
    Motor_Update(); /* Synchronize actual clock after the 1-ms arm delay. */
}
static void enable(motor_id_t id)
{
    assert(Motor_Enable(id));
    Motor_SetTarget(id, 1.0f, 80U);
}
static void report(const char *name) { ++scenarios; printf("PASS %s\n", name); }

/* Stop precisely after the production PID result, before its output commit.
 * GCC instrumentation is host-only. None of this is linked into firmware. */
void __cyg_profile_func_enter(void *, void *) __attribute__((no_instrument_function));
void __cyg_profile_func_exit(void *, void *) __attribute__((no_instrument_function));
void __cyg_profile_func_enter(void *function, void *caller)
{ (void)function; (void)caller; }
void __cyg_profile_func_exit(void *function, void *caller)
{
    (void)caller;
#ifdef TEST_COMMS_FIXTURE
    comms_instrument_exit(function);
#endif
    if (function == (void *)Encoder_BSP_GetRawCount && last_raw_id == MOTOR_INDEX && guard_injection) {
        guard_injection = false;
        assert(!primask);
        raw[MOTOR_INDEX] += 10; /* A newer guard poll sees actual progress. */
        MotorGuard_Poll();
        for (unsigned i = 0; i < MOTOR_GUARD_STALL_MS / 20U; ++i) step(20U, true);
        MotorGuard_Poll(); /* It then trips at this new count. */
        assert(MotorSafety_GetStopCauses() & MOTOR_STOP_GUARD);
        return; /* Outer poll resumes with the old pre-trip BSP sample. */
    }
    if (function != (void *)MotorPID_Update || injection == 0) return;
    int mode = injection;
    injection = 0;
    assert(!primask); /* PID calculation is outside the critical section. */
    if (mode == 1) MotorSafety_RequestStopCause(MOTOR_STOP_GUARD, MOTOR_INDEX);
    if (mode == 2) {
        Motor_StopAll();
        assert(Motor_RecoverFromFault());
        assert_stopped();
        assert(Motor_DRV8848_Arm());
        assert(Motor_Enable(MOTOR_INDEX));
        Motor_SetTarget(MOTOR_INDEX, 0.0f, 0U);
    }
    if (mode == 3) clock_ms += MOTOR_CONTROL_MAX_ELAPSED_MS + 1U;
    if (mode == 4) force(MOTOR_INDEX, 1600U);
    #ifdef TEST_REAL_BSP
    if (mode == 7 || mode == 8) reference_injection(mode);
#endif
    if (mode == 6) Motor_DRV8848_Disarm();
    if (mode == 5) Motor_SetTarget(MOTOR_INDEX, 1.2f, 80U);
}

static void test_default(void)
{
    reset(); assert_stopped(); assert(starts == 0U);
    Motor_SetAllTargets(1.0f, 100U);
    for (unsigned i = 0; i < 8; ++i) step(10U, true);
    assert_stopped(); assert(starts == 0U);
    assert(!Motor_Enable(MOTOR_INDEX));
    report("default: commands and cycles never arm");
}
static void test_guard_global(void)
{
    reset(); arm();
    for (unsigned i = 0; i < MOTOR_COUNT; ++i) enable((motor_id_t)i);
    step(10U, true);
    assert(timers[0].compare[0] && timers[1].compare[0]);
    MotorGuard_Poll();
    uint32_t trips = 0U;
    for (unsigned i = 0; i < MOTOR_GUARD_STALL_MS / 20U; ++i) {
        step(20U, true); trips |= MotorGuard_Poll();
    }
    assert(trips & (1U << MOTOR_THUMB));
    assert(MotorSafety_GetStopCauses() & MOTOR_STOP_GUARD);
    assert_stopped();
    uint32_t writes = positive_writes;
    for (unsigned i = 0; i < 12; ++i) {
        Motor_SetAllTargets(0.5f, 100U); step(10U, true); MotorGuard_Poll();
    }
    assert(positive_writes == writes); assert_stopped();
    assert(!Motor_DRV8848_Arm()); assert(!Motor_Enable(MOTOR_THUMB));
    assert(!MotorSafety_Recover()); /* Raw position has not moved after trip. */
    raw[MOTOR_THUMB] += MOTOR_GUARD_EPS_COUNTS;
    MotorGuard_Poll();
    assert(MotorSafety_IsFaultActive()); /* Motion never clears the latch. */
    position(MOTOR_WRIST_Y, NAN);
    assert(!Motor_RecoverFromFault()); /* Every previously enabled axis must be valid. */
    position(MOTOR_WRIST_Y, 0.0f);
    assert(Motor_RecoverFromFault()); assert_stopped();
    assert(MotorSafety_GetStopCauses() == 0U);
    arm(); assert(Motor_Enable(MOTOR_THUMB));
    step(10U, true);
    assert(positive_writes == writes); /* Previous target/limit discarded. */
    report("guard: global wrists included, persistent across PID/commands, conditional recovery");
}
static void test_wrist_guard(void)
{
    reset(); arm(); enable(MOTOR_WRIST_Y); enable(MOTOR_WRIST_X);
    step(10U, true); MotorGuard_Poll();
    /* X moves, Y remains stalled, so Y is the cause. */
    for (unsigned i = 0; i < MOTOR_GUARD_STALL_MS / 20U; ++i) {
        raw[MOTOR_WRIST_X] += MOTOR_GUARD_EPS_COUNTS;
        step(20U, true); MotorGuard_Poll();
    }
    assert(MotorSafety_GetCauseMotorMask(MOTOR_STOP_GUARD) & (1U << MOTOR_WRIST_Y));
    assert_stopped();
    report("wrist Y guard stops both wrist axes and all motors");
}
static void test_activation(void)
{
    reset(); arm();
    Motor_InvalidatePositionFeedback(MOTOR_INDEX);
    assert(!Motor_Enable(MOTOR_INDEX));
    position(MOTOR_INDEX, 0.0f);
    clock_ms += MOTOR_FEEDBACK_TIMEOUT_MS + 1U;
    assert(!Motor_HasFreshPositionFeedback(MOTOR_INDEX));
    assert(!Motor_Enable(MOTOR_INDEX));
    report("activation refuses absent or stale feedback");
}
static void test_stale_periodic(void)
{
    reset(); arm(); enable(MOTOR_INDEX); step(10U, true);
#ifdef TEST_HW_BACKEND
    calibrated[MOTOR_INDEX] = false;
    step(10U, false);
#else
    for (unsigned i = 0; i < 11; ++i) step(10U, false);
#endif
    assert(MotorSafety_GetStopCauses() & MOTOR_STOP_FEEDBACK); assert_stopped();
    assert(!Motor_RecoverFromFault());
    calibrated[MOTOR_INDEX] = true;
    position(MOTOR_INDEX, 0.0f); step(10U, true);
    assert(Motor_RecoverFromFault()); assert_stopped();
    report("periodic feedback loss latches; recovery needs restored valid feedback/calibration");
}
static void test_nan(void)
{
    reset(); arm(); enable(MOTOR_INDEX); step(10U, true);
    position(MOTOR_INDEX, NAN);
    assert(!Motor_HasFreshPositionFeedback(MOTOR_INDEX));
    assert(MotorSafety_GetStopCauses() & MOTOR_STOP_FEEDBACK); assert_stopped();
    assert(!Motor_RecoverFromFault());
    position(MOTOR_INDEX, INFINITY); assert(!Motor_RecoverFromFault());
    position(MOTOR_INDEX, 0.0f); assert(Motor_RecoverFromFault()); assert_stopped();
    report("NaN/Inf invalidates old position immediately and prevents recovery");
}
static void test_valid_fields(void)
{
    reset();
    motor_feedback_t fb = { .id = MOTOR_INDEX, .position = 0.0f, .velocity = NAN,
                           .force_mN = 65535U, .valid_fields = MOTOR_FB_POSITION };
    MotorBackend_OnFeedback(&fb);
    assert(!MotorSafety_IsFaultActive()); assert(Motor_HasFreshPositionFeedback(MOTOR_INDEX));
    fb.valid_fields = 0; fb.position = NAN; MotorBackend_OnFeedback(&fb);
    assert(Motor_HasFreshPositionFeedback(MOTOR_INDEX));
    fb.valid_fields = MOTOR_FB_VELOCITY; MotorBackend_OnFeedback(&fb);
    assert(!Motor_HasFreshPositionFeedback(MOTOR_INDEX));
    report("valid_fields: unclaimed force/NaN ignored, claimed nonfinite velocity invalidates");
}
static void test_elapsed(void)
{
    reset(); arm(); enable(MOTOR_INDEX);
    step(40U, true);
    float output = 10.0f * (180.0f * 0.0174532925f) * 0.040f;
    uint32_t expected = (uint32_t)(output * 7999.0f / 100.0f);
    assert(hhrtim1.compare[HRTIM_TIMERINDEX_TIMER_A][HRTIM_COMPAREUNIT_2] == expected);
    assert(!MotorSafety_IsFaultActive());
    report("PID/reference use actual 40-ms elapsed time, not nominal 10 ms");
}
static void test_delay(void)
{
    reset(); arm(); enable(MOTOR_INDEX); step(10U, true);
    step(MOTOR_CONTROL_MAX_ELAPSED_MS + 1U, true);
    assert(MotorSafety_GetStopCauses() & MOTOR_STOP_CONTROL_DELAY); assert_stopped();
    assert(!Motor_RecoverFromFault());
    step(10U, true); assert(Motor_RecoverFromFault()); assert_stopped();
    report("excessive delay latches; recovery requires a subsequent healthy control interval");
}
static void test_force(void)
{
    reset(); arm(); enable(MOTOR_INDEX); step(10U, true);
    force(MOTOR_INDEX, 1600U); /* No new command or PID cycle. */
    assert(MotorSafety_GetStopCauses() & MOTOR_STOP_FORCE); assert_stopped();
    assert(!Motor_RecoverFromFault());
    motor_feedback_t absent = { .id = MOTOR_INDEX, .force_mN = 0, .valid_fields = 0 };
    MotorBackend_OnFeedback(&absent); assert(!Motor_RecoverFromFault());
    for (unsigned i = 0; i < 11; ++i) step(10U, true);
    assert(!Motor_RecoverFromFault()); /* Old high force became stale. */
    force(MOTOR_INDEX, 1200U); assert(Motor_RecoverFromFault()); assert_stopped();
    report("force trips on reception without command; high/absent/stale force bars recovery");
}
static void test_force_expiry(void)
{
    reset(); arm(); force(MOTOR_INDEX, 0U); enable(MOTOR_INDEX);
    for (unsigned i = 0; i < 11; ++i) step(10U, true);
    assert(MotorSafety_GetStopCauses() & MOTOR_STOP_FEEDBACK); assert_stopped();
    assert(!Motor_RecoverFromFault());
    force(MOTOR_INDEX, 0U); assert(Motor_RecoverFromFault()); assert_stopped();
    report("a previously used force stream expires; no fabricated zero for recovery");
}
static void test_soft_force(void)
{
    reset(); force(MOTOR_INDEX, 1300U);
    float target = 0.5f; uint8_t speed = 80U;
    assert(MotorSafety_FilterCommand(MOTOR_INDEX, &target, &speed)); assert(speed == 25U);
    assert(!MotorSafety_IsFaultActive());
    report("existing soft-force speed limit retained without invented calibration");
}
static void test_driver_fault(void)
{
    reset(); arm(); enable(MOTOR_INDEX); step(10U, true);
    hardware_fault = true; MotorSafety_Poll();
    assert(MotorSafety_GetStopCauses() & MOTOR_STOP_DRIVER); assert_stopped();
    assert(!Motor_RecoverFromFault());
    hardware_fault = false; assert(Motor_RecoverFromFault()); assert_stopped();
    report("asserted DRV fault prevents recovery; deasserted recovery stays disarmed");
}
static void test_stop_before_commit(void)
{
    reset(); arm(); enable(MOTOR_INDEX);
    uint32_t writes = positive_writes; injection = 1; step(10U, true);
    assert(!injection); assert(MotorSafety_GetStopCauses() & MOTOR_STOP_GUARD);
    assert_stopped(); assert(positive_writes == writes);
    report("concurrency: stop after real PID calculation rejects prepared output");
}
static void test_output_before_stop(void)
{
    reset(); arm(); enable(MOTOR_INDEX); step(10U, true);
    assert(positive_writes > 0U);
    MotorSafety_RequestStopCause(MOTOR_STOP_GUARD, MOTOR_INDEX); assert_stopped();
    uint32_t writes = positive_writes;
    Motor_DRV8848_SetRaw(MOTOR_INDEX, 80.0f); Motor_DRV8848_Brake(MOTOR_INDEX);
    Motor_SetTarget(MOTOR_INDEX, 1.0f, 100U); step(10U, true);
    assert_stopped(); assert(positive_writes == writes);
    report("concurrency: output before stop is zeroed; raw/brake/PID cannot restore it");
}
static void test_stop_recover_before_commit(void)
{
    reset(); arm(); enable(MOTOR_INDEX);
    uint32_t writes = positive_writes; injection = 2; step(10U, true);
    assert(!injection); assert(!MotorSafety_IsFaultActive());
    assert(positive_writes == writes); /* Epoch rejects old PID even after explicit rearm. */
    step(10U, true); assert(positive_writes == writes);
    Motor_StopAll(); assert_stopped();
    report("concurrency: stop/recover/explicit rearm cannot commit the old PID epoch");
}
static void test_delay_during_pid(void)
{
    reset(); arm(); enable(MOTOR_INDEX); injection = 3; step(10U, true);
    assert(MotorSafety_GetStopCauses() & MOTOR_STOP_CONTROL_DELAY); assert_stopped();
    report("commit rechecks excessive time spent calculating PID");
}
static void test_force_during_pid(void)
{
    reset(); arm(); enable(MOTOR_INDEX); injection = 4; step(10U, true);
    assert(MotorSafety_GetStopCauses() & MOTOR_STOP_FORCE); assert_stopped();
    assert(positive_writes == 0U);
    report("concurrency: force arriving after PID calculation inhibits before commit");
}
static void test_wrap(void)
{
    reset_at(UINT32_MAX - 30U); arm(); enable(MOTOR_INDEX);
    for (unsigned i = 0; i < 8; ++i) step(10U, true);
    assert(Motor_HasFreshPositionFeedback(MOTOR_INDEX)); assert(!MotorSafety_IsFaultActive());
    MotorGuard_Poll();
    for (unsigned i = 0; i < MOTOR_GUARD_STALL_MS / 20U; ++i) {
        step(20U, true); MotorGuard_Poll();
    }
    assert(MotorSafety_GetStopCauses() & MOTOR_STOP_GUARD); assert_stopped();
    report("HAL millisecond wrap preserves freshness, dt and guard elapsed arithmetic");
}
static void test_command_during_pid(void)
{
    reset(); arm(); enable(MOTOR_INDEX); step(10U, true);
    injection = 5; step(10U, true); /* Command revision rejects this PID. */
    assert(!injection);
    step(10U, true);
    /* The accepted history is 20 ms old: total reference ramp is 30 ms. */
    float output = 10.0f * (180.0f * 0.0174532925f) * 0.030f;
    uint32_t expected = (uint32_t)(output * 7999.0f / 100.0f);
    assert(hhrtim1.compare[HRTIM_TIMERINDEX_TIMER_A][HRTIM_COMPAREUNIT_2] == expected);
    assert(!MotorSafety_IsFaultActive());
    report("concurrent command discards old revision and retains real elapsed PID history");
}
static void test_missing_control(void)
{
    reset(); arm(); enable(MOTOR_INDEX); step(10U, true);
    clock_ms += MOTOR_CONTROL_MAX_ELAPSED_MS + 1U;
    /* SafetyTask's production poll detects absence, even before ControlTask returns. */
    MotorSafety_Poll();
    assert(MotorSafety_GetStopCauses() & MOTOR_STOP_CONTROL_DELAY); assert_stopped();
    assert(!Motor_RecoverFromFault());
    step(10U, true); /* First resumed control interval is still too long. */
    assert(!Motor_RecoverFromFault());
    step(10U, true); assert(Motor_RecoverFromFault()); assert_stopped();
    report("periodic safety stops a missing ControlTask without waiting for its return");
}
static void test_raw_nan(void)
{
    reset(); arm(); enable(MOTOR_INDEX); step(10U, true);
    Motor_DRV8848_SetRaw(MOTOR_INDEX, NAN);
    assert(MotorSafety_GetStopCauses() & MOTOR_STOP_PID); assert_stopped();
    report("nonfinite direct output requests the same global interlock");
}
static void test_arm_cancel(void)
{
    reset(); stop_in_arm_delay = true;
    assert(!Motor_DRV8848_Arm());
    assert(MotorSafety_GetStopCauses() & MOTOR_STOP_MANUAL); assert_stopped();
    assert(!Motor_DRV8848_IsTransitioning());
    assert(Motor_RecoverFromFault()); assert_stopped();
    arm();
    uint32_t before = starts;
    assert(!Motor_DRV8848_Arm()); assert(starts == before);
    Motor_StopAll(); assert_stopped();
    report("stop during explicit arm cancels it; repeated arm cannot start a second transition");
}
static void test_guard_old_sample(void)
{
    reset(); arm(); enable(MOTOR_INDEX); step(10U, true); MotorGuard_Poll();
    guard_injection = true; MotorGuard_Poll();
    assert(!guard_injection); assert_stopped();
    assert(!Motor_RecoverFromFault()); /* No motion since the newer trip. */
    raw[MOTOR_INDEX] += MOTOR_GUARD_EPS_COUNTS;
    assert(Motor_RecoverFromFault()); assert_stopped();
    report("concurrent guard poll cannot clear a newer trip with an old BSP sample");
}
int main(void)
{
    test_default(); test_guard_global(); test_wrist_guard(); test_activation();
    test_stale_periodic(); test_nan(); test_valid_fields(); test_elapsed();
    test_delay(); test_force(); test_force_expiry(); test_soft_force();
    test_driver_fault(); test_stop_before_commit(); test_output_before_stop();
    test_stop_recover_before_commit(); test_delay_during_pid(); test_force_during_pid(); test_wrap();
    test_command_during_pid();
    test_missing_control();
    test_raw_nan(); test_arm_cancel();
    test_guard_old_sample();
#ifdef TEST_HW_BACKEND
    printf("%u production scenarios passed with actual HW backend; HAL/BSP mocked.\n", scenarios);
#else
    printf("%u production scenarios passed with asynchronous I/O fixture; HAL/BSP mocked.\n", scenarios);
#endif
    return 0;
}
