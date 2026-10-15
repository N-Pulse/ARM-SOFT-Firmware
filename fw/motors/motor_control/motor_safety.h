#ifndef MOTOR_SAFETY_H
#define MOTOR_SAFETY_H

#include "motor_control.h"
#include "motor_backend.h"

typedef enum {
    MOTOR_STOP_MANUAL        = 1U << 0,
    MOTOR_STOP_GUARD         = 1U << 1,
    MOTOR_STOP_FEEDBACK      = 1U << 2,
    MOTOR_STOP_CONTROL_DELAY = 1U << 3,
    MOTOR_STOP_FORCE         = 1U << 4,
    MOTOR_STOP_DRIVER        = 1U << 5,
    MOTOR_STOP_PID           = 1U << 6,
    MOTOR_STOP_LINK          = 1U << 7,
    MOTOR_STOP_INIT          = 1U << 8,
    MOTOR_STOP_MOTION_TIMEOUT= 1U << 9
} motor_stop_cause_t;

typedef struct {
    uint32_t ms, cause, motor_mask, epoch, state, sequence;
} motor_safety_event_t;
/* Bounded deferred diagnostics: never UART/printf from safety or an ISR. */
bool MotorSafety_PeekEvent(motor_safety_event_t *out);
void MotorSafety_ConsumeEvent(uint32_t sequence);
uint32_t MotorSafety_DroppedEvents(void);
void MotorSafety_CancelCommandsLocked(void);
void MotorSafety_Init(void);
bool MotorSafety_FilterCommand(motor_id_t id, float *position, uint8_t *speed_percent);
void MotorSafety_OnFeedback(const motor_feedback_t *feedback);
void MotorSafety_OnFeedbackAtTime(const motor_feedback_t *feedback, uint32_t ms);
bool MotorSafety_HasFreshForceLocked(motor_id_t id, uint32_t now);
void MotorSafety_Poll(void);
bool MotorSafety_IsFaultActive(void);
uint32_t MotorSafety_GetStopCauses(void);
uint32_t MotorSafety_GetCauseMotorMask(motor_stop_cause_t cause);
/* All causes latch a GLOBAL stop. MOTOR_COUNT identifies a global cause. */
void MotorSafety_RequestStopCause(motor_stop_cause_t cause, motor_id_t source);
void MotorSafety_RequestStop(void);
/* Same conditional recovery as Motor_RecoverFromFault; never enables/arms. */
bool MotorSafety_Recover(void);

/* Internal operations: caller holds Motor_Lock. No UART or blocking HAL. */
void MotorSafety_RequestStopCauseLocked(motor_stop_cause_t cause, motor_id_t source);
uint32_t MotorSafety_EpochLocked(void);
bool MotorSafety_CanArmLocked(void);
bool MotorSafety_CanEnableLocked(motor_id_t id, uint32_t now);
bool MotorSafety_CanRecoverLocked(uint32_t now);
uint32_t MotorSafety_RequiredFeedbackMaskLocked(void);
void MotorSafety_ClearFaultLocked(void);
float MotorSafety_PwmLimitLocked(motor_id_t id, float requested, uint32_t now);
/* Shutdown work is claimed/completed in task context outside Motor_Lock. */
bool MotorSafety_TakeStopWork(void);
void MotorSafety_CompleteStopWork(bool cleaned);

#endif
