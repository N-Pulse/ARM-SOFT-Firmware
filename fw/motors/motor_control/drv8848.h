#ifndef DRV8848_H
#define DRV8848_H

#include <stdint.h>
#include "motor_control.h"

void Motor_DRV8848_Init(void);
bool Motor_DRV8848_Arm(void);
/* Public safe disarm: latches global stop, discards activation/commands. */
void Motor_DRV8848_Disarm(void);
/* Stop-service only; exclusive HAL cleanup, false means retry before recovery. */
bool Motor_DRV8848_CleanupStoppedInternal(void);
/* Immediate zero + SLEEP low, register/GPIO writes only; caller holds lock. */
void Motor_DRV8848_InhibitLocked(void);
bool Motor_DRV8848_IsArmed(void);
/* Internal recovery check, caller holds Motor_Lock. */
bool Motor_DRV8848_IsTransitioning(void);
bool Motor_DRV8848_FaultActive(void);
void Motor_DRV8848_SetRaw(motor_id_t id, float output_percent);
void Motor_DRV8848_SetSignedRaw(motor_id_t id, int direction, uint8_t speed_percent);
/* Output-zero primitives only: do not invalidate controller requests.
 * Use Disarm or Motor_StopAll for a persistent safe global stop. */
void Motor_DRV8848_Stop(motor_id_t id);
void Motor_DRV8848_Brake(motor_id_t id);
void Motor_DRV8848_StopAll(void);
int  Motor_DRV8848_DrivingDir(motor_id_t id);

#endif /* DRV8848_H */
