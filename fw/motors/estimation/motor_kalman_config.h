#ifndef MOTOR_KALMAN_CONFIG_H
#define MOTOR_KALMAN_CONFIG_H
#include "motor_kalman.h"
/* Deliberately UNCONFIGURED HW. Populate per motor only after identifying the
 * motor-coordinate model J,b,Kt and noise. Synthetic host parameters belong
 * exclusively in tests/patch7/test_kalman.c. No PID feedback selection here. */
static const motor_kalman_config_t g_motor_kalman_config[8] = { [0]={0} };
#endif
