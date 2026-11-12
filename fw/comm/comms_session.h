#ifndef COMMS_SESSION_H
#define COMMS_SESSION_H
#include "intent_router.h"
#include "rx.h"
#include "comms_config.h"
#include <stddef.h>
typedef struct {
    uint32_t received, valid, accepted, applied, repeated, guard_dropped,
             rejected, stale, discarded, hello, link_timeouts, transport_errors;
} comms_diagnostics_t;
typedef struct {
    uint8_t state,sequence,length,bytes[COMMS_PAYLOAD_MAX];
    uint32_t timestamp_ms;
    bool valid;
} comms_peer_status_t;
void CommsSession_Init(bool hw);
bool CommsSession_HasSession(void);
void CommsSession_HandshakeReplySentLocked(uint32_t epoch,uint32_t generation,uint32_t ms,bool matches);
uint32_t CommsSession_HandshakeGenerationLocked(void);
bool CommsSession_CanReceiveHelloLocked(void);
app_state_t CommsSession_GetState(void);
void CommsSession_SetState(app_state_t state);
app_state_t CommsSession_StateLocked(void);
void CommsSession_StateLockedSet(app_state_t state);
void CommsSession_Report(app_result_t result,uint32_t request_id,uint32_t detail);
bool CommsSession_TakeLinkLoss(void);
void CommsSession_GetStatus(lifecycle_status_t *out);
/* Raw Status from a CRC-checked DATA frame, separate from its intent vector. */
bool CommsSession_RecordPeerStatusLocked(const uint8_t *bytes,size_t length,uint8_t sequence,
    uint32_t received_ms,uint32_t epoch,uint32_t generation);
void CommsSession_GetPeerStatus(comms_peer_status_t *out);
bool CommsSession_OnMessage(const rx_result_t *message,uint32_t received_ms,
                            uint32_t epoch,bool *session_started);
void CommsSession_GetDiagnostics(comms_diagnostics_t *out);
void CommsSession_OnStopLocked(void);
void CommsSession_PollLocked(uint32_t now);
bool CommsSession_RecordActivityLocked(uint32_t received_ms,uint32_t epoch);
bool CommsSession_CanRecoverLocked(uint32_t now);
bool CommsSession_IsFreshLocked(uint32_t now);
void CommsSession_TransportErrorLocked(void);
#endif
