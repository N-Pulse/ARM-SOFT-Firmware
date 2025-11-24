#ifndef MOTOR_SAFETY_H
#define MOTOR_SAFETY_H

#include <stdbool.h>
#include <stdint.h>

#include "motor_control.h"
#include "motor_backend.h"

void MotorSafety_Init(void);
bool MotorSafety_FilterCommand(motor_id_t id, float *position, uint8_t *speed_percent);
void MotorSafety_OnFeedback(const motor_feedback_t *feedback);
bool MotorSafety_IsFaultActive(void);
void MotorSafety_RequestStop(void);

#endif /* MOTOR_SAFETY_H */

