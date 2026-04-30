#ifndef ENCODER_BSP_H
#define ENCODER_BSP_H

#include <stdint.h>
#include "motor_control.h"

// Encoder BSP — quadrature shaft position via STM32 timer encoder mode.
//
// TODO: implement encoder_bsp.c once motor-to-timer mapping is confirmed.
// Call Encoder_BSP_Init() from MotorBackend_Init() (motor_backend_hw.c).
// Call Encoder_BSP_GetPosition() inside MotorBackend_SetTarget() for direction.

void  Encoder_BSP_Init(void);
float Encoder_BSP_GetPosition(motor_id_t id);  // radians
float Encoder_BSP_GetVelocity(motor_id_t id);  // rad/s

#endif /* ENCODER_BSP_H */
