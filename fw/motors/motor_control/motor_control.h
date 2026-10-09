#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include <stdint.h>
#include "motor_pid.h"
#include <stdbool.h>
#include <math.h>

typedef enum {
    MOTOR_THUMB,
    MOTOR_INDEX,
    MOTOR_MIDDLE,
    MOTOR_RING,
    MOTOR_LITTLE,
    MOTOR_WRIST_X,
    MOTOR_WRIST_Y,
    MOTOR_PALM,
    MOTOR_COUNT
} motor_id_t;

typedef struct {
    float target_position;       // final target，rad
    float reference_position;    // current target，rad
    float max_reference_rate;    // maximum reference change rate，rad/s
    float pwm_limit;             // PWM upper bound，%
    float measured_position;     // position returned by feedback
    float output_pwm;
    motor_pid_t pid;
    bool enabled;
    bool feedback_valid;
    uint32_t feedback_ms;
} motor_state_t;

void Motor_InitAll(void);
/* Called by ControlTask. Measures elapsed HAL milliseconds itself. */
void Motor_Update(void);
void Motor_SetTarget(motor_id_t id, float position, uint8_t speed_percent);
/* Queued commands must still belong to the current stop/recovery generation. */
bool Motor_SetTargetAtEpoch(motor_id_t id, float position, uint8_t speed_percent, uint32_t epoch);
void Motor_SetAllTargets(float position, uint8_t speed_percent);
bool Motor_Enable(motor_id_t id);
void Motor_Disable(motor_id_t id);
bool Motor_IsEnabled(motor_id_t id);
/* Explicit task-context recovery; refuses persistent causes, never arms. */
bool Motor_RecoverFromFault(void);
void Motor_StopAll(void);
void Motor_OnPositionFeedback(motor_id_t id, float position_rad, float velocity_rad_s);
void Motor_OnPositionFeedbackAtTime(motor_id_t id, float position, float velocity, uint32_t ms);
bool Motor_GetSnapshot(motor_id_t id, motor_state_t *out);
void Motor_InvalidatePositionFeedback(motor_id_t id);
bool Motor_HasFreshPositionFeedback(motor_id_t id);
/* Internal interlock entry points: caller must hold Motor_Lock. */
void Motor_InhibitAllLocked(void);
bool Motor_CanRecoverLocked(uint32_t required_feedback_mask, uint32_t now);
bool Motor_OutputAllowedLocked(motor_id_t id);
/* Task context only: finish HAL/backend shutdown outside the critical section. */
void Motor_ServiceStop(void);
/* Periodic safety health check also works when ControlTask stops running. */
void Motor_CheckHealth(void);
uint32_t Motor_StoppedControllerMaskLocked(void);

#endif // MOTOR_CONTROL_H
