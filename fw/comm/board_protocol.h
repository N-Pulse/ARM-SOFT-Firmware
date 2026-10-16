#ifndef BOARD_PROTOCOL_H
#define BOARD_PROTOCOL_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Dedicated RET6 two-board protocol. Every instance owns independent state.
 * Caller serializes access; hooks never block, allocate, or send UART data. */
#define BL_VERSION 3U
#define BL_MASTER 1U
#define BL_SLAVE 2U
#define BL_PAYLOAD 48U
#define BL_WIRE_MAX 86U
#define BL_QUEUE 8U
#define BL_HEARTBEAT_MS 50U
#define BL_TIMEOUT_MS 150U
#define BL_COMMAND_MS 100U
#define BL_COMMAND_QUEUE_MS 50U
#define BL_FRAME_MS 25U
#define BL_FEEDBACK_MS 100U
enum { BL_HELLO=1, BL_HEARTBEAT, BL_TARGET, BL_FEEDBACK, BL_STOP, BL_ACK, BL_APPLICATION };
enum { BL_ACCEPTED, BL_FORBIDDEN, BL_INVALID, BL_STALE };
enum { BL_VALID=1, BL_CALIBRATED=2, BL_ENABLED=4, BL_ARMED=8, BL_RAW_VALID=16,
       BL_CONFIGURED=32, BL_PREFLIGHT=64, BL_THUMB_BLOCKED=128 };
typedef struct {
    uint8_t id, flags;
    uint32_t age_ms, faults;
    int32_t position_mrad, velocity_mrad_s, target_mrad, pwm_milli, reference_mrad;
    int64_t raw_count;
} bl_feedback_t;
typedef struct {
    void (*stop)(void *context);
    uint8_t (*target)(void *context, uint8_t id, int32_t mrad, uint8_t speed, uint32_t epoch);
    void *context;
} bl_hooks_t;
typedef struct {
    uint8_t type, length, data[BL_PAYLOAD];
    uint32_t epoch, peer_epoch, offered_ms;
    uint64_t peer;
} bl_message_t;
typedef struct { uint32_t id, ms; uint8_t motor; bool used; } bl_pending_t;
typedef struct {
    uint8_t role;
    uint64_t session, peer;
    uint32_t epoch, faults, peer_epoch, peer_faults, tx_seq, rx_seq;
    uint32_t rx_ms, ack_ms, peer_ack, beat_ms, command_id, command_rx_id;
    bool initialized, established, have_heartbeat, authorized, peer_authorized, stop_pending, hello_pending;
    bool permanent_fault, dropped, have_peer_epoch;
    uint8_t encoded[BL_WIRE_MAX];
    unsigned encoded_length;
    uint32_t frame_ms;
    bl_message_t queue[BL_QUEUE];
    unsigned head, count;
    bl_pending_t pending[BL_QUEUE];
    bl_feedback_t feedback[8];
    uint32_t feedback_ms[8];
    bool have_feedback[8];
    uint32_t accepted_id[8];
    uint8_t accepted_status[8];
    uint32_t malformed, duplicates, rejected, overflows, stops, restarts;
    bl_hooks_t hooks;
    bl_message_t application; /* One frame, consumed outside the IRQ-masked parser. */
    bool have_application;
} board_protocol_t;

bool BL_Local(uint8_t role, uint8_t motor);
bool BL_Init(board_protocol_t *, uint8_t role, uint64_t session, uint32_t now, bl_hooks_t);
void BL_SetLocalState(board_protocol_t *, uint32_t epoch, uint32_t faults, uint32_t now);
void BL_Fail(board_protocol_t *, uint32_t now);
void BL_Poll(board_protocol_t *, uint32_t now);
bool BL_Fresh(const board_protocol_t *, uint32_t now);
bool BL_Ready(const board_protocol_t *, uint32_t now);
bool BL_Authorize(board_protocol_t *, uint32_t now);
bool BL_Target(board_protocol_t *, uint8_t motor, int32_t mrad, uint8_t speed, uint32_t epoch, uint32_t now);
bool BL_OfferFeedback(board_protocol_t *, const bl_feedback_t *, uint32_t now);
bool BL_GetFeedback(const board_protocol_t *, uint8_t motor, uint32_t now, bl_feedback_t *);
bool BL_OfferApplication(board_protocol_t *, const uint8_t *, uint8_t length, uint32_t now);
bool BL_TakeApplication(board_protocol_t *, bl_message_t *);
void BL_Feed(board_protocol_t *, uint8_t byte, uint32_t received_ms, uint32_t now);
/* Assignment of transport sequence occurs at dequeue, preserving wire order.
 * STOP/HELLO/heartbeat have reserved slots, independent of normal saturation. */
size_t BL_Take(board_protocol_t *, uint32_t now, uint8_t out[BL_WIRE_MAX]);
/* Exposed codec for native fault-injection tests and explicit wire inspection. */
size_t BL_Encode(uint8_t type, uint8_t role, uint64_t source, uint64_t destination,
                 uint32_t sequence, uint32_t epoch, uint32_t peer_epoch,
                 const uint8_t *payload, size_t length, uint8_t out[BL_WIRE_MAX]);
void BL_Put32(uint8_t *, uint32_t);
uint32_t BL_Get32(const uint8_t *);
#endif
