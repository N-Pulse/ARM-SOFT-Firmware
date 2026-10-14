#include "motor_control.h"
#include "main.h"
#include "motor_backend.h"
#include "motor_safety.h"
#include "motor_safety_config.h"
#include "motor_lock.h"
#include "motor_guard.h"
#include "motor_pid_config.h"
#include "drv8848.h"
#include "motor_backend_state.h"
#include "hw_test_config.h"
#if N_PULSE_HW_SINGLE_MOTOR_TEST
#include "hw_commission.h"
#endif
#ifdef MOTOR_BACKEND_SIM
#include "motor_sim_config.h"
#endif
#include <string.h>

static motor_state_t s_motors[MOTOR_COUNT];
static uint32_t s_revision[MOTOR_COUNT];
static uint32_t s_pid_ms[MOTOR_COUNT];
static uint32_t s_stopped_controller_mask;
static uint32_t s_last_update_ms;
static uint32_t s_last_elapsed_ms;
static bool s_clock_started;
static bool s_timing_valid;

static bool feedback_fresh_locked(motor_id_t id, uint32_t now)
{
    motor_state_t *motor = &s_motors[id];
    if (!motor->feedback_valid || !isfinite(motor->measured_position) ||
        (uint32_t)(now - motor->feedback_ms) > MOTOR_FEEDBACK_TIMEOUT_MS) {
        motor->feedback_valid = false;
        return false;
    }
    return true;
}

void Motor_InhibitAllLocked(void)
{
    for (unsigned i = 0U; i < MOTOR_COUNT; ++i) {
        motor_state_t *motor = &s_motors[i];
        if (motor->enabled) s_stopped_controller_mask |= 1U << i;
        motor->enabled = false;
        motor->output_pwm = 0.0f;
        motor->pwm_limit = 0.0f;
        motor->target_position = isfinite(motor->measured_position) ? motor->measured_position : 0.0f;
        motor->reference_position = motor->target_position;
        MotorPID_Reset(&motor->pid);
        ++s_revision[i];
    }
}

void Motor_InitAll(void)
{
    MotorBackend_InitInputs();
    MotorSafety_Init();
    MotorGuard_Init();
    memset(s_motors, 0, sizeof(s_motors));
    memset(s_revision, 0, sizeof(s_revision));
    memset(s_pid_ms, 0, sizeof(s_pid_ms));
    s_stopped_controller_mask = 0U;
    s_clock_started = false;
    s_timing_valid = false;
    s_last_update_ms = 0U;
    s_last_elapsed_ms = 0U;
    for (unsigned i = 0U; i < MOTOR_COUNT; ++i) {
#ifdef MOTOR_BACKEND_SIM
        const motor_pid_config_t sim_rotor_pid = { .kp = 40.0f, .ki = 0.0f, .kd = 4.0f,
            .integral_limit = 10.0f, .derivative_tau_s = 0.02f, .max_dt_s = 0.05f };
        s_motors[i].max_reference_rate = SIM_REFERENCE_RATE_RAD_S;
        const motor_pid_config_t *config = &sim_rotor_pid;
#else
        s_motors[i].max_reference_rate = 180.0f * 0.0174532925f;
        const motor_pid_config_t *config = &g_motor_pid_config[i];
#endif
        if (!MotorPID_Init(&s_motors[i].pid, config)) {
            MotorSafety_RequestStopCause(MOTOR_STOP_PID, (motor_id_t)i);
            return;
        }
    }
    if (!MotorBackend_InitFeedbackInputs()) { Error_Handler(); return; }
    MotorBackend_Init();
}

void Motor_ServiceStop(void)
{
    if (!MotorSafety_TakeStopWork()) return;
    /* Outputs/SLEEP were already inhibited synchronously. HAL cleanup and
     * optional SIM UART are task work; recovery is barred until completion. */
    bool cleaned = MotorBackend_CleanupStopped();
    if (cleaned) MotorBackend_StopAll();
    MotorSafety_CompleteStopWork(cleaned);
}

bool Motor_OutputAllowedLocked(motor_id_t id)
{
    return (uint32_t)id < MOTOR_COUNT && HWTest_MotorAllowed(id) && s_motors[id].enabled &&
           MotorSafety_CanArmLocked() && MotorBackend_IsArmed();
}

uint32_t Motor_StoppedControllerMaskLocked(void) { return s_stopped_controller_mask; }

void Motor_CheckHealth(void)
{
    uint32_t key = Motor_Lock();
    uint32_t now = HAL_GetTick();
    if (s_clock_started && (uint32_t)(now - s_last_update_ms) > MOTOR_CONTROL_MAX_ELAPSED_MS) {
        s_timing_valid = false;
        MotorSafety_RequestStopCauseLocked(MOTOR_STOP_CONTROL_DELAY, MOTOR_COUNT);
    }
    Motor_Unlock(key);
    for (unsigned i = 0U; i < MOTOR_COUNT; ++i) {
        key = Motor_Lock();
        now = HAL_GetTick();
        bool fresh = feedback_fresh_locked((motor_id_t)i, now);
        if (s_motors[i].enabled && (!fresh || !MotorBackend_FeedbackReady((motor_id_t)i) ||
                                  !MotorSafety_CanEnableLocked((motor_id_t)i, now)))
            MotorSafety_RequestStopCauseLocked(MOTOR_STOP_FEEDBACK, (motor_id_t)i);
        Motor_Unlock(key);
    }
}

/* One ControlTask owns PID calculation. Other tasks/ISRs may update feedback,
 * targets or request stops. Compute from a per-motor snapshot outside the
 * lock; commit only if both the stop epoch and command revision still match. */
void Motor_Update(void)
{
    MotorBackend_RefreshFeedback();
    uint32_t key = Motor_Lock();
    uint32_t now = HAL_GetTick();
    uint32_t elapsed = s_clock_started ? (uint32_t)(now - s_last_update_ms) : 0U;
    s_last_update_ms = now;
    s_clock_started = true;
    if (elapsed > 0U) {
        s_last_elapsed_ms = elapsed;
        s_timing_valid = elapsed <= MOTOR_CONTROL_MAX_ELAPSED_MS;
        if (!s_timing_valid)
            MotorSafety_RequestStopCauseLocked(MOTOR_STOP_CONTROL_DELAY, MOTOR_COUNT);
    }
    Motor_Unlock(key);
    MotorSafety_Poll();
    /* Safety_Poll validates all enabled inputs before any output. A constant
     * newly received position is valid; stall detection belongs to the guard. */
    if (MotorSafety_IsFaultActive()) {
        Motor_ServiceStop();
        return;
    }
    /* No fictitious nominal dt, and no duplicate PID step in the same ms. */
    if (elapsed == 0U) return;
    for (unsigned i = 0U; i < MOTOR_COUNT; ++i) {
        motor_id_t id = (motor_id_t)i;
        motor_state_t prepared;
        key = Motor_Lock();
        uint32_t epoch = MotorSafety_EpochLocked();
        uint32_t revision = s_revision[i];
        bool allowed = Motor_OutputAllowedLocked(id);
        uint32_t sample_ms = HAL_GetTick();
        uint32_t pid_elapsed = (uint32_t)(sample_ms - s_pid_ms[i]);
        prepared = s_motors[i];
        prepared.pwm_limit = MotorSafety_PwmLimitLocked(id, prepared.pwm_limit, HAL_GetTick());
        Motor_Unlock(key);
        if (!allowed) continue;
        if (pid_elapsed == 0U) continue;
        if (pid_elapsed > MOTOR_CONTROL_MAX_ELAPSED_MS) {
            key = Motor_Lock();
            if (epoch == MotorSafety_EpochLocked() && revision == s_revision[i]) {
                s_timing_valid = false;
                MotorSafety_RequestStopCauseLocked(MOTOR_STOP_CONTROL_DELAY, MOTOR_COUNT);
            }
            Motor_Unlock(key);
            continue;
        }
        /* A command changed during the previous calculation may have caused
         * its commit to be discarded. Use time since the last accepted PID,
         * so its ramp and D history do not silently use a shorter interval. */
        float dt = (float)pid_elapsed / 1000.0f;
        float max_step = prepared.max_reference_rate * dt;
        float delta = prepared.target_position - prepared.reference_position;
        if (delta > max_step) delta = max_step;
        else if (delta < -max_step) delta = -max_step;
        prepared.reference_position += delta;
        bool valid_pid = MotorPID_Update(&prepared.pid, prepared.reference_position,
                                        prepared.measured_position, dt, prepared.pwm_limit,
                                        &prepared.output_pwm);
        key = Motor_Lock();
        /* Stop wins whether it occurred before or after calculation. The
         * backend commit is bounded register writes (HW) or staging (SIM). */
        if (epoch == MotorSafety_EpochLocked() && revision == s_revision[i] &&
            Motor_OutputAllowedLocked(id)) {
            now = HAL_GetTick();
            if ((uint32_t)(now - s_last_update_ms) > MOTOR_CONTROL_MAX_ELAPSED_MS) {
                s_timing_valid = false;
                MotorSafety_RequestStopCauseLocked(MOTOR_STOP_CONTROL_DELAY, MOTOR_COUNT);
            } else if (!feedback_fresh_locked(id, now) || !MotorBackend_FeedbackReady(id) ||
                       !MotorSafety_CanEnableLocked(id, now)) {
                MotorSafety_RequestStopCauseLocked(MOTOR_STOP_FEEDBACK, id);
            } else if (!valid_pid) {
                MotorSafety_RequestStopCauseLocked(MOTOR_STOP_PID, id);
            } else {
                /* A force sample may have arrived while PID was calculated. */
                float limit = MotorSafety_PwmLimitLocked(id, prepared.pwm_limit, now);
                if (prepared.output_pwm > limit) prepared.output_pwm = limit;
                if (prepared.output_pwm < -limit) prepared.output_pwm = -limit;
                s_motors[i].pid = prepared.pid;
                s_motors[i].reference_position = prepared.reference_position;
                s_motors[i].output_pwm = prepared.output_pwm;
                s_pid_ms[i] = sample_ms;
                MotorBackend_ApplyOutput(id, prepared.reference_position, prepared.output_pwm);
            }
        }
        Motor_Unlock(key);
    }
    if (MotorSafety_IsFaultActive()) Motor_ServiceStop();
    else MotorBackend_Flush();
}

bool Motor_Enable(motor_id_t id)
{
    if ((uint32_t)id >= MOTOR_COUNT || !HWTest_MotorAllowed(id)) return false;
#if N_PULSE_HW_SINGLE_MOTOR_TEST
    if (!HWCommission_Ready()) return false;
#endif
    MotorSafety_Poll();
    uint32_t key = Motor_Lock();
    uint32_t now = HAL_GetTick();
    bool allowed = MotorSafety_CanArmLocked() && MotorBackend_IsArmed() &&
                   !MotorBackend_FaultActive() && MotorBackend_FeedbackReady(id) &&
                   feedback_fresh_locked(id, now) && s_timing_valid &&
                   (uint32_t)(now - s_last_update_ms) <= MOTOR_CONTROL_MAX_ELAPSED_MS &&
                   MotorSafety_CanEnableLocked(id, now);
    if (allowed) {
        motor_state_t *motor = &s_motors[id];
#if N_PULSE_HW_SINGLE_MOTOR_TEST
        motor->max_reference_rate = HWCommission_ReferenceRate();
#endif
        motor->reference_position = motor->measured_position;
        motor->target_position = motor->measured_position;
        motor->pwm_limit = 0.0f; /* An explicit new command is required AFTER enable. */
        motor->output_pwm = 0.0f;
        MotorPID_Reset(&motor->pid);
        ++s_revision[id];
        s_pid_ms[id] = now;
        motor->enabled = true;
    }
    Motor_Unlock(key);
    return allowed;
}

void Motor_Disable(motor_id_t id)
{
    if ((uint32_t)id >= MOTOR_COUNT) return;
    uint32_t key = Motor_Lock();
    motor_state_t *motor = &s_motors[id];
    motor->enabled = false;
    motor->pwm_limit = 0.0f;
    motor->output_pwm = 0.0f;
    motor->target_position = motor->measured_position;
    motor->reference_position = motor->measured_position;
    MotorPID_Reset(&motor->pid);
    ++s_revision[id];
    /* ApplyOutput zero is sufficient; physical stop is HW-only. */
#ifndef MOTOR_BACKEND_SIM
    Motor_DRV8848_Stop(id);
#endif
    MotorBackend_ApplyOutput(id, motor->reference_position, 0.0f);
    Motor_Unlock(key);
}

bool Motor_IsEnabled(motor_id_t id)
{
    if ((uint32_t)id >= MOTOR_COUNT) return false;
    uint32_t key = Motor_Lock();
    bool enabled = s_motors[id].enabled;
    Motor_Unlock(key);
    return enabled;
}

bool Motor_CanRecoverLocked(uint32_t required_feedback_mask, uint32_t now)
{
    uint32_t causes = MotorSafety_GetStopCauses();
    if ((causes & (MOTOR_STOP_CONTROL_DELAY | MOTOR_STOP_PID)) &&
        (!s_timing_valid || (uint32_t)(now - s_last_update_ms) > MOTOR_CONTROL_MAX_ELAPSED_MS))
        return false;
    for (unsigned i = 0U; i < MOTOR_COUNT; ++i) {
        if (s_motors[i].enabled) return false;
        if (!(required_feedback_mask & (1U << i))) continue;
        if (!MotorBackend_FeedbackReady((motor_id_t)i) ||
            !feedback_fresh_locked((motor_id_t)i, now)) return false;
        if (causes & MOTOR_STOP_PID) {
            motor_pid_t check;
            if (!MotorPID_Init(&check, &s_motors[i].pid.config) ||
                (float)s_last_elapsed_ms / 1000.0f > check.config.max_dt_s) return false;
        }
    }
    return true;
}

motor_recovery_ticket_t Motor_RecoveryTicketLocked(void)
{ return (motor_recovery_ticket_t){MotorSafety_EpochLocked(),MotorSafety_StopSerialLocked()}; }

bool Motor_RecoverFromFault(void)
{
    uint32_t key=Motor_Lock();
    motor_recovery_ticket_t ticket=Motor_RecoveryTicketLocked();
    Motor_Unlock(key);
    return Motor_RecoverFromFaultWithTicket(ticket);
}

bool Motor_RecoverFromFaultWithTicket(motor_recovery_ticket_t ticket)
{
    MotorGuard_Poll();
    MotorSafety_Poll();
    Motor_ServiceStop();
    uint32_t key = Motor_Lock();
    uint32_t now = HAL_GetTick();
    bool recoverable = ticket.epoch == MotorSafety_EpochLocked() &&
                       ticket.stop_serial == MotorSafety_StopSerialLocked() &&
                       MotorSafety_CanRecoverLocked(now) && MotorGuard_CanRecoverLocked() &&
                       Motor_CanRecoverLocked(MotorSafety_RequiredFeedbackMaskLocked(), now);
    if (recoverable) {
        /* Already inhibited: clearing a latch neither arms nor starts anything. */
        Motor_InhibitAllLocked();
        s_stopped_controller_mask = 0U;
        MotorSafety_ClearFaultLocked();
    }
    Motor_Unlock(key);
    return recoverable;
}

void Motor_SetTarget(motor_id_t id, float position, uint8_t speed_percent)
{
    if ((uint32_t)id >= MOTOR_COUNT || !HWTest_MotorAllowed(id)) return;
    if (!isfinite(position)) {
        MotorSafety_RequestStopCause(MOTOR_STOP_PID, id);
        return;
    }
    float safe_position = position;
    uint8_t safe_speed = speed_percent;
    uint32_t key = Motor_Lock();
#if N_PULSE_HW_SINGLE_MOTOR_TEST
    if (!s_motors[id].enabled || !HWCommission_FilterTarget(position, &safe_speed)) {
        Motor_Unlock(key); return;
    }
#endif
    if (MotorSafety_FilterCommand(id, &safe_position, &safe_speed)) {
        s_motors[id].target_position = safe_position;
        s_motors[id].pwm_limit = safe_speed > 100U ? 100U : safe_speed;
        ++s_revision[id];
    }
    Motor_Unlock(key);
}

void Motor_SetAllTargets(float position, uint8_t speed_percent)
{
    for (unsigned i = 0U; i < MOTOR_COUNT; ++i)
        Motor_SetTarget((motor_id_t)i, position, speed_percent);
}

void Motor_InvalidatePositionFeedback(motor_id_t id)
{
    if ((uint32_t)id >= MOTOR_COUNT) return;
    uint32_t key = Motor_Lock();
    s_motors[id].feedback_valid = false;
    ++s_revision[id];
    if (s_motors[id].enabled)
        MotorSafety_RequestStopCauseLocked(MOTOR_STOP_FEEDBACK, id);
    Motor_Unlock(key);
}

bool Motor_SetTargetAtEpoch(motor_id_t id, float position, uint8_t speed_percent, uint32_t epoch)
{
    uint32_t key = Motor_Lock();
    bool allowed = epoch == MotorSafety_EpochLocked() && MotorSafety_CanArmLocked();
    if (allowed) Motor_SetTarget(id, position, speed_percent);
    Motor_Unlock(key);
    return allowed;
}

void Motor_OnPositionFeedback(motor_id_t id, float position_rad, float velocity_rad_s)
{
    Motor_OnPositionFeedbackAtTime(id, position_rad, velocity_rad_s, HAL_GetTick());
}

void Motor_OnPositionFeedbackAtTime(motor_id_t id, float position_rad, float velocity_rad_s, uint32_t ms)
{
    (void)velocity_rad_s; /* D is derived from position; velocity is not used. */
    if ((uint32_t)id >= MOTOR_COUNT) return;
    if (!isfinite(position_rad)) {
        Motor_InvalidatePositionFeedback(id);
        return;
    }
    uint32_t key = Motor_Lock();
    s_motors[id].measured_position = position_rad;
    s_motors[id].feedback_ms = ms;
    s_motors[id].feedback_valid = true;
#if N_PULSE_HW_SINGLE_MOTOR_TEST
    if(s_motors[id].enabled && !HWCommission_MeasurementAllowed(position_rad))
        MotorSafety_RequestStopCauseLocked(MOTOR_STOP_FEEDBACK,id);
#endif
    Motor_Unlock(key);
}

bool Motor_HasFreshPositionFeedback(motor_id_t id)
{
    if ((uint32_t)id >= MOTOR_COUNT) return false;
    uint32_t key = Motor_Lock();
    bool fresh = feedback_fresh_locked(id, HAL_GetTick());
    if (!fresh && s_motors[id].enabled)
        MotorSafety_RequestStopCauseLocked(MOTOR_STOP_FEEDBACK, id);
    Motor_Unlock(key);
    return fresh;
}

void Motor_StopAll(void)
{
    MotorSafety_RequestStop();
    /* Request is safe from task/ISR context. Cleanup is serviced by tasks. */
}

bool Motor_GetSnapshot(motor_id_t id, motor_state_t *out)
{
    if ((uint32_t)id >= MOTOR_COUNT || !out) return false;
    uint32_t key = Motor_Lock(); *out = s_motors[id]; Motor_Unlock(key);
    return true;
}
