#ifndef LIFECYCLE_PROTOCOL_H
#define LIFECYCLE_PROTOCOL_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef enum {
    APP_INIT_HANDSHAKE=0, APP_INIT_MODULES=1, APP_INIT_RETURN=2,
    APP_READY=3, APP_WORK_WAIT=4, APP_WORK_EXECUTING=5,
    APP_STOPPING=6, APP_STOPPED=7, APP_FAULT=8
} app_state_t;
typedef enum {
    CONTROL_ALLOW_START=1, CONTROL_DENY_START=2, CONTROL_STOP=3,
    CONTROL_RESTART=4, CONTROL_PING=5, CONTROL_PONG=6,
    CONTROL_QUERY=7, CONTROL_ESTOP=8
} control_command_t;
typedef enum {
    APP_RESULT_OK=0, APP_RESULT_WRONG_STATE=1, APP_RESULT_QUEUE_FULL=2,
    APP_RESULT_BAD_SESSION=3, APP_RESULT_STALE=4, APP_RESULT_RECOVERY_DENIED=5,
    APP_RESULT_INIT_FAILED=6, APP_RESULT_MOTION_TIMEOUT=7,
    APP_RESULT_BAD_VERSION=8, APP_RESULT_PEER_CHANGED=9,
    APP_RESULT_TRANSPORT=10, APP_RESULT_LINK_TIMEOUT=11
} app_result_t;
typedef struct { uint32_t command, request_id, peer_id; } lifecycle_control_t;
typedef struct {
    uint32_t state, result, request_id, queue_depth, dropped, causes,
             action_sequence, action_done, detail, revision;
} lifecycle_status_t;
/* -1 invalid, 0 legacy Hello/Data envelope, 1 Control envelope.
 * Reject ambiguous/mixed oneofs and noncanonical varints. */
#ifdef __cplusplus
extern "C" {
#endif
int Lifecycle_DecodeDevice(const uint8_t *, size_t, lifecycle_control_t *);
size_t Lifecycle_EncodeControl(uint8_t *, size_t, const lifecycle_control_t *);
size_t Lifecycle_EncodeProbe(uint8_t *, size_t, const lifecycle_control_t *);
size_t Lifecycle_EncodeHello(uint8_t *, size_t);
size_t Lifecycle_EncodeStatus(uint8_t *, size_t, const lifecycle_status_t *);
size_t Lifecycle_EncodeSafety(uint8_t *, size_t, uint32_t ms, uint32_t cause,
                              uint32_t mask, uint32_t epoch, uint32_t state);
#ifdef __cplusplus
}
#endif
#endif
