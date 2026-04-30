#ifndef HRTIM_BSP_H
#define HRTIM_BSP_H

#include <stdint.h>
#include "motor_control.h"

// HRTIM BSP — high-resolution PWM output for motor driver ICs.
//
// TODO: fill in hrtim_bsp.c once pin mapping is confirmed.
// Call HRTIM_BSP_Init() from MotorBackend_Init() (motor_backend_hw.c).

void HRTIM_BSP_Init(void);
void HRTIM_BSP_SetDuty(motor_id_t id, float duty_0_to_1);
void HRTIM_BSP_SetAllDuty(float duty_0_to_1);

#endif /* HRTIM_BSP_H */
