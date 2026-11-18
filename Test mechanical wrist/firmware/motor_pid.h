#ifndef MOTOR_PID_H
#define MOTOR_PID_H

#include <stdbool.h>

/* Units: rad, seconds, signed PWM percentage [-100, 100].
 * One instance per motor. Caller owns scheduling, measurement validity,
 * direction mapping, enable/stop state and hardware/simulator output.
 * Serialize access to each instance. No HAL or FreeRTOS dependency.
 */
typedef struct {
    float kp;                 /* PWM % / rad */
    float ki;                 /* PWM % / (rad * s) */
    float kd;                 /* PWM % * s / rad */
    float integral_limit;     /* Max absolute integral contribution, PWM % */
    float derivative_tau_s;   /* D measurement low-pass time constant; 0 = off */
    float max_dt_s;           /* Reject longer intervals; > 0 */
} motor_pid_config_t;

typedef struct {
    motor_pid_config_t config;
    float integral;
    float previous_position;
    float filtered_velocity;
    bool has_measurement;
    bool initialized;
} motor_pid_t;

/* Invalid initialization leaves the instance disabled. Gains must be >= 0.
 * No universal gains are supplied: tune for the motor and simulator model.
 */
bool MotorPID_Init(motor_pid_t *pid, const motor_pid_config_t *config);
void MotorPID_Reset(motor_pid_t *pid);

/* target/position: same continuous angular coordinate (no automatic wrapping).
 * dt_s: time between measurement samples, NOT time between target messages.
 * pwm_limit: current allowed magnitude [0,100], e.g. safe_speed.
 * D acts on measured position, avoiding target derivative kick.
 * Integral uses conditional anti-windup and an independent contribution limit.
 * A zero limit resets dynamic state and produces zero output.
 * Invalid input resets dynamic state, writes zero output and returns false.
 * Caller MUST apply a stop on false; do not retain an old hardware output.
 * First sample has no D contribution. The target is not ramped by this module.
 */
bool MotorPID_Update(motor_pid_t *pid, float target_rad,
                     float position_rad, float dt_s, float pwm_limit,
                     float *output_percent);

#endif
