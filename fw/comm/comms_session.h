#ifndef COMMS_SESSION_H
#define COMMS_SESSION_H
#include "intent_router.h"
#include "rx.h"
#include <stddef.h>
typedef struct {
    uint32_t received, valid, accepted, queued, queue_full, rejected, stale,
             discarded, hello, link_timeouts, transport_errors;
} comms_diagnostics_t;
void CommsSession_Init(bool hw);
bool CommsSession_HasSession(void);
void CommsSession_BeginRestartLocked(void);
app_state_t CommsSession_GetState(void);
void CommsSession_SetState(app_state_t state);
/* Locked versions share the motor interlock; no UART or RTOS work. */
app_state_t CommsSession_StateLocked(void);
void CommsSession_StateLockedSet(app_state_t state);
void CommsSession_Report(app_result_t result, uint32_t request_id, uint32_t detail);
bool CommsSession_TakeControl(lifecycle_control_t *out, uint32_t *epoch);
bool CommsSession_GetProbe(lifecycle_control_t *out);
void CommsSession_GetStatus(lifecycle_status_t *out);
void CommsSession_ActionStarted(void);
void CommsSession_ActionCompleted(void);
bool CommsSession_OnMessage(const rx_result_t *message, uint32_t received_ms,
                            uint32_t epoch, bool *session_started);
bool CommsSession_GetIntent(intent_t *intent);
void CommsSession_GetDiagnostics(comms_diagnostics_t *out);
/* Shared interlock domain, bounded state operations, no RTOS/UART. */
void CommsSession_OnStopLocked(void);
void CommsSession_PollLocked(uint32_t now);
bool CommsSession_CanRecoverLocked(uint32_t now);
bool CommsSession_IsFreshLocked(uint32_t now);
void CommsSession_TransportErrorLocked(void);
#endif
