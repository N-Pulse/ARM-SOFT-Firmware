#ifndef MOTOR_KALMAN_H
#define MOTOR_KALMAN_H
#include <stdint.h>
#include <stdbool.h>
/* Independent, allocation-free MOTOR-coordinate estimator. No HAL, PID,
 * actuation or safety dependencies. All parameters have explicit SI units. */
typedef struct {
    double inertia_kg_m2, damping_nm_s, torque_nm_a;
    double q_position, q_cross, q_velocity; /* symmetric Q per second */
    double r_position, initial_position_variance, initial_velocity_variance;
    double innovation_limit_rad, missing_current_q_factor, max_dt_s;
    uint32_t consecutive_limit, current_max_age_ms, encoder_max_age_ms;
    double encoder_to_motor_ratio; /* explicit calibrated encoder radians -> motor radians */
} motor_kalman_config_t;
enum { KALMAN_UNCONFIGURED=1, KALMAN_NEEDS_REFERENCE=2, KALMAN_READY=4,
    KALMAN_DEGRADED=8, KALMAN_OUTLIER=16, KALMAN_INCONSISTENT=32,
    KALMAN_NUMERIC_ERROR=64, KALMAN_TIME_ERROR=128, KALMAN_ENCODER_MISSING=256 };
typedef struct {
    motor_kalman_config_t config;
    double position, velocity, p00, p01, p11, innovation;
    uint32_t timestamp_ms, encoder_ms, status;
    uint32_t predictions, corrections, missing_current, outliers, consecutive, resets;
    bool configured, initialized, encoder_seen;
} motor_kalman_t;
bool MotorKalman_Validate(const motor_kalman_config_t *config);
bool MotorKalman_Init(motor_kalman_t *state,const motor_kalman_config_t *config);
/* Explicit operator/reference authority required, never automatic re-zeroing. */
bool MotorKalman_Reset(motor_kalman_t *state,double position,uint32_t timestamp_ms,
    bool disabled,bool verified_stationary,bool trustworthy_reference);
bool MotorKalman_Predict(motor_kalman_t *state,uint32_t timestamp_ms,
    double current_a,bool current_valid,uint32_t current_age_ms);
bool MotorKalman_Correct(motor_kalman_t *state,double encoder_position,bool valid,uint32_t encoder_ms);
bool MotorKalman_IsValid(const motor_kalman_t *state);
#endif
