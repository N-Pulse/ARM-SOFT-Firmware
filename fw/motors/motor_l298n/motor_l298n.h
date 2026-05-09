#ifndef MOTOR_L298N_H
#define MOTOR_L298N_H

#include <stdint.h>
#include "motor_control.h"

/*
 * motor_l298n — generic L298N H-bridge driver for the 8 hand motors.
 *
 * On boot, the module looks at BoardLink_IsLocalMotor() for each motor id
 * and only initializes the GPIO + PWM channels for motors physically wired
 * to THIS board. The other motors get their commands forwarded over the
 * board-link by motor_control.c, so this module never sees them.
 *
 * Open-loop position control: with no encoder, the move duration for a
 * given angular delta is derived from L298N_DEG_PER_SEC_AT_FULL (an
 * empirical constant — see README to recalibrate per motor + gearbox).
 * Each motor has its own per-id auto-stop FreeRTOS timer.
 *
 * See README.md for the full pin map and wiring.
 */

void Motor_L298N_Init(void);

/* Drive `id` toward `target_rad` (clamped to [-pi, +pi]) at `speed_pct`
 * (0..100). The motor runs only as long as needed to traverse the angular
 * delta from its cached commanded position; the auto-stop timer cuts the
 * H-bridge when the open-loop estimate of "we got there" elapses. */
void Motor_L298N_MoveToAngle(motor_id_t id, float target_rad, uint8_t speed_pct);

/* Cut a single motor (and cancel its pending auto-stop). */
void Motor_L298N_Stop(motor_id_t id);

/* Cut every locally-owned motor. */
void Motor_L298N_StopAll(void);

#endif /* MOTOR_L298N_H */
