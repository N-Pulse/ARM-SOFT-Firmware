#ifndef MOTOR_BACKEND_H
#define MOTOR_BACKEND_H

#include <stdint.h>
#include <math.h>
#include <stddef.h>
#include "drv8848.h"
#include "motor_control.h"

enum {
    MOTOR_FB_POSITION = 1U << 0,
    MOTOR_FB_VELOCITY = 1U << 1,
    MOTOR_FB_FORCE    = 1U << 2
};

typedef struct
{
    motor_id_t id;
    float      position;
    float      velocity;
    uint16_t   force_mN;
    uint8_t valid_fields;
} motor_feedback_t;

void MotorBackend_Init(void);
void MotorBackend_SetTarget(motor_id_t id, float position, uint8_t speed_percent);
void MotorBackend_SendPreview(motor_id_t id, float position, uint8_t speed_percent);
void MotorBackend_Flush(void);
void MotorBackend_StopAll(void);
/* Cancel staged local outputs without UART/waiting; caller holds lock. */
void MotorBackend_InhibitOutputsLocked(void);
void MotorBackend_RefreshFeedback(void);
void MotorBackend_OnFeedback(const motor_feedback_t *feedback);
void MotorBackend_OnFeedbackAtTime(const motor_feedback_t *feedback, uint32_t ms);
void MotorBackend_HandleFeedback(motor_id_t id, float position_rad, float velocity_rad_s);
void MotorBackend_ApplyOutput(motor_id_t id, float reference_position, float output_percent);

#endif /* MOTOR_BACKEND_H */
