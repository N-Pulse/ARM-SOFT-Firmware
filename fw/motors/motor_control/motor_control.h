#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include <stdint.h>
#include "motor_pid.h"
#include <stdbool.h>
#include <math.h>

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

typedef struct {
    float target_position;       // final target，rad
    float reference_position;    // current target，rad
    float max_reference_rate;    // maximum reference change rate，rad/s
    float pwm_limit;             // PWM upper bound，%
    float measured_position;     // position returned by feedback
    float output_pwm;
    motor_pid_t pid;
    bool enabled;
} motor_state_t;

void Motor_InitAll(void);
void Motor_Update(float dt);
void Motor_SetTarget(motor_id_t id, float position, uint8_t speed_percent);
void Motor_SetAllTargets(float position, uint8_t speed_percent);
void Motor_StopAll(void);
void Motor_OnPositionFeedback(motor_id_t id, float position_rad, float velocity_rad_s);

#endif // MOTOR_CONTROL_H
