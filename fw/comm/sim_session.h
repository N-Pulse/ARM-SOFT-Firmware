#ifndef SIM_SESSION_H
#define SIM_SESSION_H
#ifdef MOTOR_BACKEND_SIM
#include "sim_protocol.h"
#include "motor_control.h"
typedef struct { uint64_t session; uint32_t run, accepted, rejected, old, malformed, stops, tx_replaced; bool live, configured, armed; } sim_diagnostics_t;
void SimSession_Init(void);
bool SimSession_OnFrame(uint8_t,const uint8_t *,size_t,uint32_t,uint32_t,void *);
void SimSession_PollLocked(uint32_t);
void SimSession_TransportErrorLocked(void);
bool SimSession_CanRecoverLocked(uint32_t);
bool SimSession_IsArmedLocked(void);
bool SimSession_FeedbackReadyLocked(motor_id_t);
void SimSession_InhibitLocked(void);
/* SIM_OUTPUT body after the common header: run u32, epoch u32, enabled_mask
 * u8, then MOTOR_COUNT entries (motor_ID u8, direction i8 -1/0/+1,
 * nonnegative speed f32 rad/s). Little endian, 6 bytes per entry; payload is
 * 73 bytes including the 16-byte common header. Replaces the old PWM layout. */
void SimSession_StageOutputLocked(motor_id_t id,int8_t direction,float speed_rad_s);
void SimSession_Service(void);
bool SimSession_GuardSampleLocked(motor_id_t,float *,float *);
void SimSession_GetDiagnostics(sim_diagnostics_t *);
#endif
#endif
