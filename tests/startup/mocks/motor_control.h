#ifndef MOCK_MOTOR_CONTROL_H
#define MOCK_MOTOR_CONTROL_H
#include <stdint.h>
#include <stdbool.h>
typedef enum { MOTOR_THUMB, MOTOR_INDEX, MOTOR_MIDDLE, MOTOR_RING,
    MOTOR_LITTLE, MOTOR_WRIST_X, MOTOR_WRIST_Y, MOTOR_PALM, MOTOR_COUNT } motor_id_t;
bool Motor_IsEnabled(motor_id_t);
void Motor_InvalidatePositionFeedback(motor_id_t);
#endif
