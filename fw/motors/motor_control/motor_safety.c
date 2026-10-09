#include "motor_safety.h"
#include "motor_lock.h"
#include "motor_safety_config.h"
#include "comms_session.h"
#include "motor_backend_state.h"
#ifdef MOTOR_BACKEND_SIM
#include "sim_session.h"
#include "motor_sim_config.h"
#endif
#include <math.h>
#include <string.h>

#define DEGREES_TO_RAD(x) ((x) * 0.0174532925f)

#ifndef MOTOR_BACKEND_SIM
static const float s_pos_min[MOTOR_COUNT] = {
    DEGREES_TO_RAD(0.0f), DEGREES_TO_RAD(0.0f), DEGREES_TO_RAD(0.0f),
    DEGREES_TO_RAD(0.0f), DEGREES_TO_RAD(0.0f), DEGREES_TO_RAD(-90.0f),
    DEGREES_TO_RAD(-90.0f), DEGREES_TO_RAD(-5.0f)
};
static const float s_pos_max[MOTOR_COUNT] = {
    DEGREES_TO_RAD(90.0f), DEGREES_TO_RAD(90.0f), DEGREES_TO_RAD(90.0f),
    DEGREES_TO_RAD(90.0f), DEGREES_TO_RAD(90.0f), DEGREES_TO_RAD(90.0f),
    DEGREES_TO_RAD(90.0f), DEGREES_TO_RAD(15.0f)
};
#endif
static const uint16_t s_force_limits[MOTOR_COUNT] = {
#ifdef MOTOR_BACKEND_SIM
    SIM_FORCE_HARD_MN, SIM_FORCE_HARD_MN, SIM_FORCE_HARD_MN, SIM_FORCE_HARD_MN,
    SIM_FORCE_HARD_MN, SIM_FORCE_HARD_MN, SIM_FORCE_HARD_MN, SIM_FORCE_HARD_MN
#else
    MOTOR_FORCE_HARD_LIMIT_MN, MOTOR_FORCE_HARD_LIMIT_MN, MOTOR_FORCE_HARD_LIMIT_MN, MOTOR_FORCE_HARD_LIMIT_MN,
    MOTOR_FORCE_HARD_LIMIT_MN, MOTOR_FORCE_WRIST_HARD_LIMIT_MN, MOTOR_FORCE_WRIST_HARD_LIMIT_MN, MOTOR_FORCE_HARD_LIMIT_MN
#endif
};

static uint16_t s_force[MOTOR_COUNT];
static uint32_t s_force_ms[MOTOR_COUNT];
static bool s_force_received[MOTOR_COUNT];
static uint32_t s_causes;
static uint32_t s_cause_motors[8];
static uint32_t s_epoch;
static bool s_stop_pending;
static bool s_stop_in_progress;

static unsigned cause_index(motor_stop_cause_t cause)
{
    unsigned index = 0U;
    uint32_t bits = (uint32_t)cause;
    while ((bits >>= 1U) != 0U) ++index;
    return index;
}

static bool force_fresh(motor_id_t id, uint32_t now)
{
    return s_force_received[id] &&
           (uint32_t)(now - s_force_ms[id]) <= MOTOR_FEEDBACK_TIMEOUT_MS;
}

void MotorSafety_Init(void)
{
    memset(s_force, 0, sizeof(s_force));
    memset(s_force_ms, 0, sizeof(s_force_ms));
    memset(s_force_received, 0, sizeof(s_force_received));
    memset(s_cause_motors, 0, sizeof(s_cause_motors));
    s_causes = 0U;
    s_epoch = 0U;
    s_stop_pending = false;
    s_stop_in_progress = false;
    CommsSession_Init(false); /* Comms_Init enables HW session supervision. */
}

void MotorSafety_RequestStopCauseLocked(motor_stop_cause_t cause, motor_id_t source)
{
    uint32_t bits = (uint32_t)cause;
    /* This entry accepts one known cause, preserving an unambiguous history. */
    if (bits == 0U || (bits & (bits - 1U)) != 0U || bits > MOTOR_STOP_LINK) return;
    unsigned index = cause_index(cause);
    uint32_t mask = (uint32_t)source < MOTOR_COUNT ? (1U << source) :
                    ((1U << MOTOR_COUNT) - 1U);
    if ((s_causes & bits) && (s_cause_motors[index] & mask) == mask) return;
    s_causes |= bits;
    s_cause_motors[index] |= mask;
    ++s_epoch;
    CommsSession_OnStopLocked();
    /* Invalidate all prepared output, discard every target and immediately
     * inhibit all eight physical motors, including both wrist axes. */
    Motor_InhibitAllLocked();
    MotorBackend_InhibitActuationLocked();
    MotorBackend_InhibitOutputsLocked();
    s_stop_pending = true;
}

void MotorSafety_RequestStopCause(motor_stop_cause_t cause, motor_id_t source)
{
    uint32_t key = Motor_Lock();
    MotorSafety_RequestStopCauseLocked(cause, source);
    Motor_Unlock(key);
}

void MotorSafety_RequestStop(void)
{
    MotorSafety_RequestStopCause(MOTOR_STOP_MANUAL, MOTOR_COUNT);
}

bool MotorSafety_IsFaultActive(void)
{
    uint32_t key = Motor_Lock();
    bool result = s_causes != 0U;
    Motor_Unlock(key);
    return result;
}

uint32_t MotorSafety_GetStopCauses(void)
{
    uint32_t key = Motor_Lock();
    uint32_t result = s_causes;
    Motor_Unlock(key);
    return result;
}

uint32_t MotorSafety_GetCauseMotorMask(motor_stop_cause_t cause)
{
    uint32_t bits = (uint32_t)cause;
    if (!bits || (bits & (bits - 1U)) || bits > MOTOR_STOP_LINK) return 0U;
    uint32_t key = Motor_Lock();
    uint32_t result = s_cause_motors[cause_index(cause)];
    Motor_Unlock(key);
    return result;
}

uint32_t MotorSafety_EpochLocked(void) { return s_epoch; }

bool MotorSafety_CanArmLocked(void)
{
    return s_causes == 0U && !s_stop_pending && !s_stop_in_progress;
}

bool MotorSafety_CanEnableLocked(motor_id_t id, uint32_t now)
{
    return !s_force_received[id] ||
           (force_fresh(id, now) && s_force[id] < s_force_limits[id]);
}

float MotorSafety_PwmLimitLocked(motor_id_t id, float requested, uint32_t now)
{
#ifdef MOTOR_BACKEND_SIM
    if (force_fresh(id, now) && s_force[id] >= SIM_FORCE_SOFT_MN &&
        requested > SIM_FORCE_REDUCED_PERCENT) return SIM_FORCE_REDUCED_PERCENT;
#else
    if (force_fresh(id, now) && s_force[id] >= MOTOR_FORCE_SOFT_LIMIT_MN &&
        requested > MOTOR_FORCE_REDUCED_SPEED) return MOTOR_FORCE_REDUCED_SPEED;
#endif
    return requested;
}

bool MotorSafety_FilterCommand(motor_id_t id, float *position, uint8_t *speed_percent)
{
    if ((uint32_t)id >= MOTOR_COUNT || position == NULL || speed_percent == NULL ||
        !isfinite(*position)) return false;
    uint32_t key = Motor_Lock();
    bool allowed = s_causes == 0U;
    if (allowed) {
#ifdef MOTOR_BACKEND_SIM
        if (*position < -SIM_POSITION_LIMIT_RAD) *position = -SIM_POSITION_LIMIT_RAD;
        if (*position > SIM_POSITION_LIMIT_RAD) *position = SIM_POSITION_LIMIT_RAD;
#else
        if (*position < s_pos_min[id]) *position = s_pos_min[id];
        if (*position > s_pos_max[id]) *position = s_pos_max[id];
#endif
        *speed_percent = (uint8_t)MotorSafety_PwmLimitLocked(id, *speed_percent, HAL_GetTick());
    }
    Motor_Unlock(key);
    return allowed;
}

void MotorSafety_OnFeedback(const motor_feedback_t *feedback)
{
    MotorSafety_OnFeedbackAtTime(feedback, HAL_GetTick());
}

void MotorSafety_OnFeedbackAtTime(const motor_feedback_t *feedback, uint32_t ms)
{
    if (feedback == NULL || (uint32_t)feedback->id >= MOTOR_COUNT ||
        !(feedback->valid_fields & MOTOR_FB_FORCE)) return;
    motor_id_t id = feedback->id;
    uint32_t key = Motor_Lock();
    s_force[id] = feedback->force_mN;
    s_force_ms[id] = ms;
    s_force_received[id] = true;
    if (s_force[id] >= s_force_limits[id])
        MotorSafety_RequestStopCauseLocked(MOTOR_STOP_FORCE, id);
    Motor_Unlock(key);
}

void MotorSafety_Poll(void)
{
    Motor_CheckHealth();
    uint32_t key = Motor_Lock();
    CommsSession_PollLocked(HAL_GetTick());
#ifdef MOTOR_BACKEND_SIM
    SimSession_PollLocked(HAL_GetTick());
#endif
    if (MotorBackend_FaultActive())
        MotorSafety_RequestStopCauseLocked(MOTOR_STOP_DRIVER, MOTOR_COUNT);
    Motor_Unlock(key);
    for (unsigned i = 0U; i < MOTOR_COUNT; ++i) {
        key = Motor_Lock();
        uint32_t now = HAL_GetTick();
        if (force_fresh((motor_id_t)i, now) && s_force[i] >= s_force_limits[i])
            MotorSafety_RequestStopCauseLocked(MOTOR_STOP_FORCE, (motor_id_t)i);
        else if (s_force_received[i] && !force_fresh((motor_id_t)i, now) &&
                 Motor_IsEnabled((motor_id_t)i))
            MotorSafety_RequestStopCauseLocked(MOTOR_STOP_FEEDBACK, (motor_id_t)i);
        Motor_Unlock(key);
    }
}

uint32_t MotorSafety_RequiredFeedbackMaskLocked(void)
{
    return Motor_StoppedControllerMaskLocked() |
           s_cause_motors[cause_index(MOTOR_STOP_GUARD)] |
           s_cause_motors[cause_index(MOTOR_STOP_FEEDBACK)] |
           s_cause_motors[cause_index(MOTOR_STOP_PID)];
}

bool MotorSafety_CanRecoverLocked(uint32_t now)
{
    if (!CommsSession_CanRecoverLocked(now)) return false;
#ifdef MOTOR_BACKEND_SIM
    if (!SimSession_CanRecoverLocked(now)) return false;
#endif
    if (s_stop_pending || s_stop_in_progress || MotorBackend_IsTransitioning() || MotorBackend_IsArmed() ||
        MotorBackend_FaultActive()) return false;
    uint32_t mask = s_cause_motors[cause_index(MOTOR_STOP_FORCE)];
    for (unsigned i = 0U; i < MOTOR_COUNT; ++i) {
        if ((mask & (1U << i)) &&
            (!force_fresh((motor_id_t)i, now) || s_force[i] >= s_force_limits[i])) return false;
        /* Once force reception has been used by a stopped controller, a
         * missing force sample cannot be replaced by a fabricated zero. */
        if ((MotorSafety_RequiredFeedbackMaskLocked() & (1U << i)) &&
            s_force_received[i] && !force_fresh((motor_id_t)i, now)) return false;
    }
    return true;
}

void MotorSafety_ClearFaultLocked(void)
{
    ++s_epoch;
    CommsSession_OnStopLocked();
    s_causes = 0U;
    memset(s_cause_motors, 0, sizeof(s_cause_motors));
}

bool MotorSafety_Recover(void) { return Motor_RecoverFromFault(); }

bool MotorSafety_TakeStopWork(void)
{
    uint32_t key = Motor_Lock();
    bool result = s_stop_pending && !s_stop_in_progress;
    if (result) {
        s_stop_pending = false;
        s_stop_in_progress = true;
    }
    Motor_Unlock(key);
    return result;
}

void MotorSafety_CompleteStopWork(bool cleaned)
{
    uint32_t key = Motor_Lock();
    s_stop_in_progress = false;
    if (!cleaned) s_stop_pending = true;
    Motor_Unlock(key);
}

bool MotorSafety_HasFreshForceLocked(motor_id_t id, uint32_t now)
{ return (uint32_t)id < MOTOR_COUNT && force_fresh(id, now) && s_force[id] < s_force_limits[id]; }
