// motor_pid_config.h
#ifndef MOTOR_PID_CONFIG_H
#define MOTOR_PID_CONFIG_H

#include "motor_pid.h"
#include "motor_control.h"  // MOTOR_COUNT

extern const motor_pid_config_t g_motor_pid_config[MOTOR_COUNT];

#endif