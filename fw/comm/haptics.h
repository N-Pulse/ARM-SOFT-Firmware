#ifndef HAPTICS_H
#define HAPTICS_H
#include "motor_control.h"
#include <stdint.h>
#include <stddef.h>
#include "haptics_config.h"
typedef struct { uint8_t proto_ver,status_len,data_len,hb_period_ms; } haptics_parameters_t;
typedef enum { HAPTICS_WAIT_SYNC, HAPTICS_SEND_HELLO, HAPTICS_WAIT_REPLY,
    HAPTICS_STREAMING, HAPTICS_INCOMPATIBLE } haptics_state_t;
typedef struct {
    uint8_t state,sequence,length,bytes[253];
    uint32_t timestamp_ms,revision;
    bool valid;
} haptics_status_t;
/* A successful send starts one physical transfer and copies the supplied bytes.
 * A busy transport returns false. It must NOT enqueue telemetry for later.
 * Feed/Service are owned by the same task; snapshots are interlock protected. */
typedef bool (*haptics_send_fn)(const uint8_t *,size_t,void *);
void Haptics_Init(void);
void Haptics_FeedByte(uint8_t byte,uint32_t now_ms);
void Haptics_Service(uint32_t now_ms,haptics_send_fn send,void *context);
haptics_state_t Haptics_GetState(void);
void Haptics_GetParameters(haptics_parameters_t *out);
void Haptics_GetStatus(haptics_status_t *out);
/* One-transfer USART2 board adapter; disconnected weak fallbacks for SIM.
 * Force ADC wiring is independent and remains unassigned. No UART4 reuse. */
bool Haptics_TransportStart(void);
bool Haptics_TransportReadByte(uint8_t *byte,uint32_t *timestamp_ms);
bool Haptics_TransportTrySend(const uint8_t *frame,size_t length);
void Haptics_TaskCreate(void);
#endif
