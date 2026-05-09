#ifndef FINGER_TEST_H
#define FINGER_TEST_H

#include <stdint.h>
#include "motor_map.h"

/*
 * Single-finger test bench: drives one DC motor through an L298N H-bridge.
 *
 * Wiring (Nucleo-G474RE):
 *   ENA → PA8  (TIM1_CH1, Arduino D7)   — PWM 20 kHz, AF6
 *   IN1 → PB5  (Arduino D4)             — GPIO push-pull
 *   IN2 → PB4  (Arduino D5)             — GPIO push-pull
 *   GND → Nucleo GND, common with motor PSU
 *
 * Motor power: 6–12 V external on L298N Vmotor, NOT from Nucleo 5V.
 *
 * Direction (L298N truth table, EN driven by PWM):
 *   IN1=1 IN2=0 → forward
 *   IN1=0 IN2=1 → backward
 *   IN1=0 IN2=0 → coast (PWM=0 also coasts)
 *   IN1=1 IN2=1 → brake (do not use; not exposed by this API)
 */

void Finger_Test_Init(void);
void Finger_Test_Forward(uint8_t speed_percent);
void Finger_Test_Backward(uint8_t speed_percent);
void Finger_Test_Stop(void);

/* Drive the test motor toward the angle defined by `pose` for the configured
 * test finger (see FINGER_TEST_MOTOR_ID in finger_test.c). The driver caches
 * its commanded position; on each call it picks the direction (forward /
 * backward) and computes how long to run, based on the delta to the cached
 * position and the empirical FINGER_DEG_PER_SEC_AT_FULL calibration. */
void Finger_Test_MoveToPose(motor_pose_id_t pose, uint8_t speed_percent);

#endif /* FINGER_TEST_H */
