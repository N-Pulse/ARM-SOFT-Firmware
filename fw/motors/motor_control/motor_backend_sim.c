#include "motor_backend.h"
#ifdef MOTOR_BACKEND_SIM
#include "motor_backend_state.h"
#include "motor_safety.h"
#include "motor_lock.h"
#include "sim_session.h"
#include "motor_sim_config.h"
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
    if((unsigned)id>=MOTOR_COUNT) return;
    if(!isfinite(reference) || !isfinite(value)) { MotorSafety_RequestStopCause(MOTOR_STOP_PID,id); return; }
    /* Preserve the PID sign independently of target/reference coordinates. */
    int8_t direction=(value>0.0f)?1:((value<0.0f)?-1:0);
    float speed=fabsf(value);
    if(speed>100.0f) speed=100.0f;
    speed=speed*SIM_MAX_SPEED_RAD_S/100.0f;
    uint32_t key=Motor_Lock();
    if(Motor_OutputAllowedLocked(id)) SimSession_StageOutputLocked(id,direction,speed);
    else SimSession_StageOutputLocked(id,0,0.0f);
    Motor_Unlock(key);
}
#endif
