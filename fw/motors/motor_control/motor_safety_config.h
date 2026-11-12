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
/* Settled position hold may produce a small nonzero PID drive. Do not call
 * that a stall; force/feedback/driver supervision remains active. */
#ifndef MOTOR_GUARD_HOLD_TOLERANCE_RAD
#define MOTOR_GUARD_HOLD_TOLERANCE_RAD 0.035f
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

/* Hardware force-sensor voltage thresholds. kg=1.1937*V+0.0064;
 * old mN -> kg uses 9806.65 mN/kg. No runtime force conversion is needed.
 * 1200/1600/2000 mN correspond to these voltage limits. */
#ifndef MOTOR_FORCE_SOFT_LIMIT_V
#define MOTOR_FORCE_SOFT_LIMIT_V 0.09714832f
#endif
#ifndef MOTOR_FORCE_HARD_LIMIT_V
#define MOTOR_FORCE_HARD_LIMIT_V 0.13131825f
#endif
#ifndef MOTOR_FORCE_WRIST_HARD_LIMIT_V
#define MOTOR_FORCE_WRIST_HARD_LIMIT_V 0.16548818f
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
#if MOTOR_FORCE_REDUCED_SPEED > 100U
#error "Invalid software force thresholds"
#endif

#endif
