// motor_pid_config.c
#include "motor_pid_config.h"
#include "motor_control.h"

// Parameters for PIDs for each motor
const motor_pid_config_t g_motor_pid_config[MOTOR_COUNT] = {
    [MOTOR_THUMB] = {
        .kp = 10.0f,
        .ki = 0.0f,
        .kd = 0.0f,
        .integral_limit = 10.0f,
        .derivative_tau_s = 0.02f,
        .max_dt_s = 0.05f,
    },
    [MOTOR_INDEX] = {
        .kp = 10.0f,
        .ki = 0.0f,
        .kd = 0.0f,
        .integral_limit = 10.0f,
        .derivative_tau_s = 0.02f,
        .max_dt_s = 0.05f,
    },
    [MOTOR_MIDDLE] = {
        .kp = 10.0f,
        .ki = 0.0f,
        .kd = 0.0f,
        .integral_limit = 10.0f,
        .derivative_tau_s = 0.02f,
        .max_dt_s = 0.05f,
    },
    [MOTOR_RING] = {
        .kp = 10.0f,
        .ki = 0.0f,
        .kd = 0.0f,
        .integral_limit = 10.0f,
        .derivative_tau_s = 0.02f,
        .max_dt_s = 0.05f,
    },
    [MOTOR_LITTLE] = {
        .kp = 10.0f,
        .ki = 0.0f,
        .kd = 0.0f,
        .integral_limit = 10.0f,
        .derivative_tau_s = 0.02f,
        .max_dt_s = 0.05f,
    },
    [MOTOR_WRIST_X] = {
        .kp = 10.0f,
        .ki = 0.0f,
        .kd = 0.0f,
        .integral_limit = 10.0f,
        .derivative_tau_s = 0.02f,
        .max_dt_s = 0.05f,
    },
    [MOTOR_WRIST_Y] = {
        .kp = 10.0f,
        .ki = 0.0f,
        .kd = 0.0f,
        .integral_limit = 10.0f,
        .derivative_tau_s = 0.02f,
        .max_dt_s = 0.05f,
    },
    [MOTOR_PALM] = {
        .kp = 10.0f,
        .ki = 0.0f,
        .kd = 0.0f,
        .integral_limit = 10.0f,
        .derivative_tau_s = 0.02f,
        .max_dt_s = 0.05f,
    }
};