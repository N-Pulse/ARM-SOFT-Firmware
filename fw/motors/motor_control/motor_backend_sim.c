#include "motor_backend.h"
#ifdef MOTOR_BACKEND_SIM
#include "motor_backend_state.h"
#include "motor_safety.h"
#include "motor_lock.h"
#include "sim_session.h"
void MotorBackend_InitInputs(void) { /* No physical encoder/ADC/driver operation. */ }
bool MotorBackend_InitFeedbackInputs(void) { return true; }
void MotorBackend_Init(void) { SimSession_Init(); }
bool MotorBackend_IsArmed(void) { return SimSession_IsArmedLocked(); }
bool MotorBackend_FaultActive(void) { return false; }
bool MotorBackend_IsTransitioning(void) { return false; }
bool MotorBackend_FeedbackReady(motor_id_t id) { return SimSession_FeedbackReadyLocked(id); }
void MotorBackend_InhibitActuationLocked(void) { SimSession_InhibitLocked(); }
bool MotorBackend_CleanupStopped(void) { return true; }
void MotorBackend_SetTarget(motor_id_t id,float position,uint8_t speed) { (void)id; (void)position; (void)speed; }
void MotorBackend_SendPreview(motor_id_t id,float position,uint8_t speed) { (void)id; (void)position; (void)speed; }
void MotorBackend_RefreshFeedback(void) { /* Only real SIM RX delivers new samples. */ }
void MotorBackend_Flush(void) { SimSession_Service(); }
void MotorBackend_InhibitOutputsLocked(void) { /* Actuation inhibition clears staging atomically. */ }
void MotorBackend_StopAll(void) { SimSession_Service(); }
void MotorBackend_ApplyOutput(motor_id_t id,float reference,float value)
{
    (void)reference; /* The remote actuator receives PID output, never a position target. */
    if((unsigned)id>=MOTOR_COUNT) return;
    if(!isfinite(value)) { MotorSafety_RequestStopCause(MOTOR_STOP_PID,id); return; }
    if(value>100.0f) value=100.0f;
    if(value< -100.0f) value= -100.0f;
    uint32_t key=Motor_Lock();
    if(Motor_OutputAllowedLocked(id)) SimSession_StageOutputLocked(id,value);
    else SimSession_StageOutputLocked(id,0.0f);
    Motor_Unlock(key);
}
#endif
