#ifndef BOARD_LINK_H
#define BOARD_LINK_H

#include <stdint.h>
#include <stdbool.h>
#include "motor_control.h"
#if N_PULSE_OLD_HW
#include "old_hw.h"
board_role_t BoardLink_Role(void);
#endif

/* One role-selected binary; physical PID/feedback always remain local. */
void BoardLink_Init(void);

/* Kept for source compatibility with application code being migrated. */
bool          BoardLink_IsLocalMotor(motor_id_t id);
void          BoardLink_SendMotorCmd(motor_id_t id,
                                     float       target_rad,
                                     uint8_t     speed_percent);
void          BoardLink_TaskCreate(void);

#if N_PULSE_DUAL_BOARD_LINK && !defined(MOTOR_BACKEND_SIM)
#include "board_protocol.h"
#include "tx.h"
void BoardLink_RuntimeInit(void);
void BoardLink_SafetyPollLocked(uint32_t now);
bool BoardLink_CanRecoverLocked(uint32_t now);
bool BoardLink_ActuationAllowedLocked(uint32_t now);
bool BoardLink_HostAuthority(void);
bool BoardLink_RouteTarget(motor_id_t id,float target,uint8_t speed,uint32_t epoch);
bool BoardLink_GetRemote(motor_id_t id,bl_feedback_t *out);
void BoardLink_GetDiagnostic(link_diagnostic_t *out);
uint32_t BoardLink_Operator(uint32_t operation,uint32_t request,uint32_t epoch,uint32_t ms);
bool BoardLink_RxCallback(void *uart);
bool BoardLink_TxCallback(void *uart);
bool BoardLink_ErrorCallback(void *uart);
void BoardLink_StartTask(void);
#endif

#endif /* BOARD_LINK_H */
