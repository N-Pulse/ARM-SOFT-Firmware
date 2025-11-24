#include "motor_backend.h"
#include "motor_safety.h"

#if !defined(MOTOR_BACKEND_SIM)

void MotorBackend_Init(void)
{
    /* TODO: initialize timers, ADCs, and driver ICs when hardware is ready. */
}

void MotorBackend_SetTarget(motor_id_t id, float position, uint8_t speed_percent)
{
    (void)id;
    (void)position;
    (void)speed_percent;
    /* TODO: convert target into PWM/FOC commands for the real motors. */
}

void MotorBackend_Flush(void)
{
    /* HW backend acts immediately; nothing to flush. */
}

void MotorBackend_StopAll(void)
{
    /* TODO: actively brake/disable all motor phases. */
}

void MotorBackend_OnFeedback(const motor_feedback_t *feedback)
{
    MotorSafety_OnFeedback(feedback);
}

#endif /* !MOTOR_BACKEND_SIM */

