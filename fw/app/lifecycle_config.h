#ifndef LIFECYCLE_CONFIG_H
#define LIFECYCLE_CONFIG_H
/* Initial software defaults; validate all values against the real mechanism.
 * Boot pose must physically be OPEN. RESTART preserves the established zero. */
#ifndef APP_POSITION_TOLERANCE_RAD
#define APP_POSITION_TOLERANCE_RAD 0.035f
#endif
#ifndef APP_SETTLE_MS
#define APP_SETTLE_MS 250U
#endif
#ifndef APP_MOTION_TIMEOUT_MS
#define APP_MOTION_TIMEOUT_MS 15000U
#endif
#ifndef APP_INIT_TIMEOUT_MS
#define APP_INIT_TIMEOUT_MS 10000U
#endif
#ifndef APP_HOME_SPEED_PERCENT
#define APP_HOME_SPEED_PERCENT 25U
#endif
#ifndef APP_ENCODER_PPR
#define APP_ENCODER_PPR 12U
#endif
#ifndef APP_ENCODER_QUADRATURE
#define APP_ENCODER_QUADRATURE 4U
#endif
#ifndef APP_ENCODER_GEAR_RATIO
#define APP_ENCODER_GEAR_RATIO 61.0f
#endif
/* Override per motor by editing the table in app.c (especially LPTIM PALM). */
#ifndef APP_ENCODER_DIRECTION
#define APP_ENCODER_DIRECTION 1
#endif
#if APP_SETTLE_MS == 0 || APP_SETTLE_MS >= 0x80000000U || APP_MOTION_TIMEOUT_MS <= APP_SETTLE_MS || APP_MOTION_TIMEOUT_MS >= 0x80000000U || APP_INIT_TIMEOUT_MS == 0 || APP_INIT_TIMEOUT_MS >= 0x80000000U || APP_HOME_SPEED_PERCENT == 0 || APP_HOME_SPEED_PERCENT > 100
#error Invalid lifecycle bounds
#endif
#endif
