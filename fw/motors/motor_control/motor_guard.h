#ifndef MOTOR_GUARD_H
#define MOTOR_GUARD_H

#include <stdbool.h>
#include <stdint.h>

void MotorGuard_Init(void);
/* BSP reads only; never starts an encoder. Returns newly tripped motor bits. */
uint32_t MotorGuard_Poll(void);
/* Caller holds Motor_Lock. A tripped encoder must have moved by EPS counts
 * after the trip before recovery can be accepted; no automatic recovery. */
bool MotorGuard_CanRecoverLocked(void);

#endif
