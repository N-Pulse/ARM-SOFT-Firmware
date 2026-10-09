#ifndef MOTOR_SAFETY_CONFIG_H
#define MOTOR_SAFETY_CONFIG_H

/* Software bounds only, not validated on a board. Override at compilation.
 * Existing PID max_dt_s (50 ms) remains an additional independent bound. */
#ifndef MOTOR_FEEDBACK_TIMEOUT_MS
#define MOTOR_FEEDBACK_TIMEOUT_MS 100U
#endif
#ifndef MOTOR_CONTROL_MAX_ELAPSED_MS
#define MOTOR_CONTROL_MAX_ELAPSED_MS 50U
#endif
#ifndef MOTOR_GUARD_PERIOD_MS
#define MOTOR_GUARD_PERIOD_MS 20U
#endif
#ifndef MOTOR_GUARD_STALL_MS
#define MOTOR_GUARD_STALL_MS 180U
#endif
#ifndef MOTOR_GUARD_EPS_COUNTS
#define MOTOR_GUARD_EPS_COUNTS 6U
#endif

/* Existing software force limits only. No calibration or current protection. */
#ifndef MOTOR_FORCE_SOFT_LIMIT_MN
#define MOTOR_FORCE_SOFT_LIMIT_MN 1200U
#endif
#ifndef MOTOR_FORCE_HARD_LIMIT_MN
#define MOTOR_FORCE_HARD_LIMIT_MN 1600U
#endif
#ifndef MOTOR_FORCE_WRIST_HARD_LIMIT_MN
#define MOTOR_FORCE_WRIST_HARD_LIMIT_MN 2000U
#endif
#ifndef MOTOR_FORCE_REDUCED_SPEED
#define MOTOR_FORCE_REDUCED_SPEED 25U
#endif

#if MOTOR_FEEDBACK_TIMEOUT_MS == 0 || MOTOR_FEEDBACK_TIMEOUT_MS > 0x7FFFFFFFU || \
    MOTOR_CONTROL_MAX_ELAPSED_MS == 0 || MOTOR_CONTROL_MAX_ELAPSED_MS > 0x7FFFFFFFU || \
    MOTOR_GUARD_STALL_MS == 0 || MOTOR_GUARD_STALL_MS > 0x7FFFFFFFU || \
    MOTOR_GUARD_PERIOD_MS == 0 || MOTOR_GUARD_EPS_COUNTS == 0
#error "Motor safety timeouts and guard thresholds must be positive bounded values"
#endif
#if MOTOR_FORCE_SOFT_LIMIT_MN >= MOTOR_FORCE_HARD_LIMIT_MN || \
    MOTOR_FORCE_SOFT_LIMIT_MN >= MOTOR_FORCE_WRIST_HARD_LIMIT_MN || \
    MOTOR_FORCE_HARD_LIMIT_MN > 65535U || MOTOR_FORCE_WRIST_HARD_LIMIT_MN > 65535U || \
    MOTOR_FORCE_REDUCED_SPEED > 100U
#error "Invalid software force thresholds"
#endif

#endif
