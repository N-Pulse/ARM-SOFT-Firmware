#ifndef UART4_TRANSPORT_H
#define UART4_TRANSPORT_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "comms_config.h"
typedef struct { uint8_t byte; uint32_t ms, epoch, generation; } uart4_rx_event_t;
typedef struct {
    uint32_t rx_bytes, rx_overflow, rx_errors, rx_start_errors,
             tx_queued, tx_full, tx_errors, tx_timeouts, tx_complete, tx_stale, tx_replaced, tx_expired;
} uart4_diagnostics_t;
void UART4Transport_Init(void);
void UART4Transport_CreateTxTask(void);
bool UART4Transport_StartRx(TaskHandle_t owner);
bool UART4Transport_PopRx(uart4_rx_event_t *out);
bool UART4Transport_RecoverRx(void);
bool UART4Transport_RxGenerationMatches(uint32_t generation);
/* Zero wait, complete frame copied into a bounded queue. TASK context only,
 * outside Motor_Lock. Success means queued, not transmitted/acknowledged. */
bool UART4Transport_QueueFrame(const uint8_t *frame, size_t length);
bool UART4Transport_QueueFrameAtEpoch(const uint8_t *frame, size_t length, uint32_t epoch);
bool UART4Transport_QueuePayload(const uint8_t *payload, size_t length);
/* Only immutable SafetyEvent records; survives later actuator epoch changes. */
bool UART4Transport_QueueSafetyPayload(const uint8_t *payload, size_t length);
#ifdef MOTOR_BACKEND_SIM
/* Three bounded replaceable mailboxes: latest output, status/reply, urgent STOP.
 * A successful offer is never a delivery acknowledgement. */
bool UART4Transport_QueueSimFrame(const uint8_t *,size_t,uint32_t,uint32_t,uint8_t);
void UART4Transport_DiscardSimOutputLocked(void);
#endif
bool UART4Transport_TxOnce(TickType_t wait);
void UART4Transport_GetDiagnostics(uart4_diagnostics_t *out);
#endif
