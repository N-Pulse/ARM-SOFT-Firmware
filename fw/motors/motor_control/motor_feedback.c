#include "motor_backend.h"
#include "motor_safety.h"

/* Identical field validation in HW and SIM; absent fields do not constitute
 * new samples. Their last valid sample expires in the periodic controller. */
void MotorBackend_OnFeedback(const motor_feedback_t *feedback)
{
    if (feedback == NULL || (uint32_t)feedback->id >= MOTOR_COUNT) return;
    MotorSafety_OnFeedback(feedback);
    if ((feedback->valid_fields & MOTOR_FB_VELOCITY) && !isfinite(feedback->velocity)) {
        Motor_InvalidatePositionFeedback(feedback->id);
        return;
    }
    if (feedback->valid_fields & MOTOR_FB_POSITION)
        Motor_OnPositionFeedback(feedback->id, feedback->position, feedback->velocity);
}

void MotorBackend_OnFeedbackAtTime(const motor_feedback_t *fb, uint32_t ms)
{
    if (!fb || (uint32_t)fb->id >= MOTOR_COUNT) return;
    MotorSafety_OnFeedbackAtTime(fb, ms);
    if ((fb->valid_fields & MOTOR_FB_VELOCITY) && !isfinite(fb->velocity)) {
        Motor_InvalidatePositionFeedback(fb->id); return;
    }
    if (fb->valid_fields & MOTOR_FB_POSITION)
        Motor_OnPositionFeedbackAtTime(fb->id, fb->position, fb->velocity, ms);
}
