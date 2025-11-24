#ifndef MOTOR_BACKEND_H
#define MOTOR_BACKEND_H

#include <stdint.h>
#include "motor_control.h"

typedef struct
{
    motor_id_t id;
    float      position;
    float      velocity;
    uint16_t   force_mN;
} motor_feedback_t;

void MotorBackend_Init(void);
void MotorBackend_SetTarget(motor_id_t id, float position, uint8_t speed_percent);
void MotorBackend_Flush(void);
void MotorBackend_StopAll(void);
void MotorBackend_OnFeedback(const motor_feedback_t *feedback);

#endif /* MOTOR_BACKEND_H */

