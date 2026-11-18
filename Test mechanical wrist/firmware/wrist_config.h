#ifndef WRIST_CONFIG_H
#define WRIST_CONFIG_H
/* Firmware-wide technical bounds, NOT motor electrical specifications.
 * Per-motor calibration/PID/limits/safety are entered from the PC.
 * Changing these values requires rebuilding and reflashing this firmware. */
#define WRIST_CONTROL_MS 10U
#define WRIST_MAX_CONTROL_MS 50U
#define WRIST_HEARTBEAT_MS 500U
#define WRIST_HARD_PWM_PERCENT 25.0f
#define WRIST_SETTLE_MS 100U
#define WRIST_REVERSAL_MS 2U
#define WRIST_PWM_PERIOD 8499U /* 170 MHz / 8500 = 20 kHz */
/* Logical positive PWM uses IN1/IN3 high. Reverse these signs if needed.
 * Positive motor command must increase the calibrated encoder coordinate. */
#define WRIST_MOTOR1_POLARITY 1
#define WRIST_MOTOR2_POLARITY 1
#if (WRIST_MOTOR1_POLARITY != 1 && WRIST_MOTOR1_POLARITY != -1) || \
    (WRIST_MOTOR2_POLARITY != 1 && WRIST_MOTOR2_POLARITY != -1)
#error Motor polarity must be 1 or -1
#endif
#endif
