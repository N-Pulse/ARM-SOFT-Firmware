#ifndef MOTOR_GRIP_CONFIG_H
#define MOTOR_GRIP_CONFIG_H
/* Closing is positive PWM in the calibrated finger convention. Contact seek
 * runs until each selected motor stalls, rather than to a fixed pose angle.
 * Low software starting value; tune against the actual mechanism. */
#ifndef MOTOR_GRIP_PWM_PERCENT
#define MOTOR_GRIP_PWM_PERCENT 10U
#endif
#if MOTOR_GRIP_PWM_PERCENT == 0 || MOTOR_GRIP_PWM_PERCENT > 100U
#error Invalid grip PWM limit
#endif
#endif
