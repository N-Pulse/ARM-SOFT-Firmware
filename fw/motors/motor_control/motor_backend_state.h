#ifndef MOTOR_BACKEND_STATE_H
#define MOTOR_BACKEND_STATE_H
#include "motor_backend.h"
/* Activation, input readiness and inhibition belong to the selected backend.
 * HW keeps the lot-5 checks. SIM has no reference to a physical start API. */
#ifdef MOTOR_BACKEND_SIM
void MotorBackend_InitInputs(void);
bool MotorBackend_InitFeedbackInputs(void);
bool MotorBackend_IsArmed(void);
bool MotorBackend_FaultActive(void);
bool MotorBackend_IsTransitioning(void);
bool MotorBackend_FeedbackReady(motor_id_t id);
void MotorBackend_InhibitActuationLocked(void);
bool MotorBackend_CleanupStopped(void);
#else
#include "encoder_bsp.h"
#include "main.h"
static inline void MotorBackend_InitInputs(void) {
    Motor_DRV8848_Init();
}
static inline bool MotorBackend_InitFeedbackInputs(void) { return Encoder_BSP_Init(); }
static inline bool MotorBackend_IsArmed(void) { return Motor_DRV8848_IsArmed(); }
static inline bool MotorBackend_FaultActive(void) { return Motor_DRV8848_FaultActive(); }
static inline bool MotorBackend_IsTransitioning(void) { return Motor_DRV8848_IsTransitioning(); }
static inline bool MotorBackend_FeedbackReady(motor_id_t id) { return Encoder_BSP_IsCalibrated(id); }
static inline void MotorBackend_InhibitActuationLocked(void) { Motor_DRV8848_InhibitLocked(); }
static inline bool MotorBackend_CleanupStopped(void) { return Motor_DRV8848_CleanupStoppedInternal(); }
#endif
#endif
