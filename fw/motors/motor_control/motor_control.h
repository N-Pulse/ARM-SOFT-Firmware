#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include <stdint.h>

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

void Motor_InitAll(void);
void Motor_Update(void);
void Motor_SetTarget(motor_id_t id, float position, uint8_t speed_percent);
void Motor_SetAllTargets(float position, uint8_t speed_percent);
void Motor_StopAll(void);

#endif // MOTOR_CONTROL_H
