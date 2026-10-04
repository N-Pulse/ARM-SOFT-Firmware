#include "motor_pid.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

static float clampf(float x, float lo, float hi)
{
    return x < lo ? lo : (x > hi ? hi : x);
}

static bool valid_config(const motor_pid_config_t *c)
{
    return c != NULL &&
        isfinite(c->kp) && c->kp >= 0.0f &&
        isfinite(c->ki) && c->ki >= 0.0f &&
        isfinite(c->kd) && c->kd >= 0.0f &&
        isfinite(c->integral_limit) && c->integral_limit >= 0.0f &&
        isfinite(c->derivative_tau_s) && c->derivative_tau_s >= 0.0f &&
        isfinite(c->max_dt_s) && c->max_dt_s > 0.0f;
}

void MotorPID_Reset(motor_pid_t *pid)
{
    if (pid == NULL) return;
    pid->integral = 0.0f;
    pid->previous_position = 0.0f;
    pid->filtered_velocity = 0.0f;
    pid->has_measurement = false;
}

bool MotorPID_Init(motor_pid_t *pid, const motor_pid_config_t *config)
{
    if (pid == NULL) return false;
    /* Copy before clearing in case config refers to pid->config. */
    motor_pid_config_t copy = {0};
    bool valid = valid_config(config);
    if (valid) copy = *config;
    memset(pid, 0, sizeof(*pid));
    if (!valid) return false;
    pid->config = copy;
    pid->initialized = true;
    return true;
}

bool MotorPID_Update(motor_pid_t *pid, float target_rad,
                     float position_rad, float dt_s, float pwm_limit,
                     float *output_percent)
{
    if (output_percent != NULL) *output_percent = 0.0f;
    if (pid == NULL) return false;
    if (output_percent == NULL || !pid->initialized ||
        !valid_config(&pid->config) ||
        !isfinite(target_rad) || !isfinite(position_rad) ||
        !isfinite(dt_s) || dt_s <= 0.0f || dt_s > pid->config.max_dt_s ||
        !isfinite(pwm_limit) || pwm_limit < 0.0f || pwm_limit > 100.0f) {
        MotorPID_Reset(pid);
        return false;
    }
    if (pwm_limit == 0.0f) {
        MotorPID_Reset(pid);
        return true;
    }

    const motor_pid_config_t *c = &pid->config;
    float error = target_rad - position_rad;
    float velocity = 0.0f;
    if (pid->has_measurement) {
        float raw_velocity = (position_rad - pid->previous_position) / dt_s;
        float alpha = 1.0f / (1.0f + c->derivative_tau_s / dt_s);
        velocity = pid->filtered_velocity + alpha * (raw_velocity - pid->filtered_velocity);
    }
    float proportional = c->kp * error;
    float derivative = -c->kd * velocity;
    float increment = c->ki * error * dt_s;
    float old_integral = c->ki == 0.0f ? 0.0f :
        clampf(pid->integral, -c->integral_limit, c->integral_limit);
    float raw_integral = old_integral + increment;
    if (!isfinite(error) || !isfinite(velocity) || !isfinite(proportional) ||
        !isfinite(derivative) || !isfinite(increment) || !isfinite(raw_integral)) {
        MotorPID_Reset(pid);
        return false;
    }
    float candidate = clampf(raw_integral, -c->integral_limit, c->integral_limit);
    float tentative = proportional + candidate + derivative;
    if (!isfinite(tentative)) {
        MotorPID_Reset(pid);
        return false;
    }
    /* Reject integral changes that drive further into output saturation.
     * Opposite-sign changes remain allowed to unwind existing integral.
     */
    float change = candidate - old_integral;
    if ((tentative > pwm_limit && change > 0.0f) ||
        (tentative < -pwm_limit && change < 0.0f)) {
        candidate = old_integral;
    }
    float output = proportional + candidate + derivative;
    if (!isfinite(output)) {
        MotorPID_Reset(pid);
        return false;
    }
    pid->integral = candidate;
    pid->previous_position = position_rad;
    pid->filtered_velocity = velocity;
    pid->has_measurement = true;
    *output_percent = clampf(output, -pwm_limit, pwm_limit);
    return true;
}
