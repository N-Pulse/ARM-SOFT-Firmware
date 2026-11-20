#include "haptics.h"
#include "haptics_uart2.h"
#include "motor_lock.h"
#include <string.h>

#ifndef MOTOR_BACKEND_SIM
extern UART_HandleTypeDef huart2;
/* USART2: PD5 TX, PD6 RX, 115200 8N1. PG9 reset remains inactive.
 * The radio must already provide transparent serial delivery. No AT commands.
 * RX interrupts only store bytes; all protocol work belongs to haptics_task. */
enum { RX_CAPACITY = 1024, TX_TIMEOUT_MS = 100 };
typedef struct { uint8_t byte; uint32_t ms; } rx_event_t;
static rx_event_t ring[RX_CAPACITY];
static volatile unsigned head, tail;
static uint8_t rx_byte, tx_bytes[HAPTICS_FRAME_MAX];
static volatile bool recovering;
static volatile bool started;
static bool tx_active;
static uint32_t tx_started_ms;

bool Haptics_TransportStart(void)
{
    HAL_NVIC_DisableIRQ(USART2_IRQn);
    head = tail = 0;
    recovering = tx_active = false;
    started = false;
    if (huart2.Instance != USART2) return false;
    HAL_NVIC_SetPriority(USART2_IRQn, 6U, 0U);
    HAL_NVIC_ClearPendingIRQ(USART2_IRQn);
    started = HAL_UART_Receive_IT(&huart2, &rx_byte, 1U) == HAL_OK;
    if (started) HAL_NVIC_EnableIRQ(USART2_IRQn);
    return started;
}

bool HapticsUART2_RxComplete(UART_HandleTypeDef *uart)
{
    if (uart != &huart2) return false;
    if (!started || recovering) return true;
    unsigned next = (head + 1U) % RX_CAPACITY;
    if (next == tail) { recovering = true; return true; }
    ring[head] = (rx_event_t){rx_byte, HAL_GetTick()};
    head = next;
    if (HAL_UART_Receive_IT(uart, &rx_byte, 1U) != HAL_OK) recovering = true;
    return true;
}

bool HapticsUART2_Error(UART_HandleTypeDef *uart)
{
    if (uart != &huart2) return false;
    recovering = true;
    return true;
}

static bool service_transport(void)
{
    if (!started) return false;
    if (tx_active) {
        if (huart2.gState == HAL_UART_STATE_READY) tx_active = false;
        else if ((uint32_t)(HAL_GetTick() - tx_started_ms) >= TX_TIMEOUT_MS)
            recovering = true;
    }
    if (!recovering) return true;
    /* A lost byte invalidates framing and peer status. Flush the entire session
     * and wait for a new SYNC rather than presenting a partial frame as fresh. */
    HAL_NVIC_DisableIRQ(USART2_IRQn);
    bool aborted = HAL_UART_AbortReceive(&huart2) == HAL_OK;
    aborted = (HAL_UART_AbortTransmit(&huart2) == HAL_OK) && aborted;
    head = tail = 0;
    tx_active = false;
    __HAL_UART_CLEAR_FLAG(&huart2, UART_CLEAR_OREF | UART_CLEAR_NEF |
                         UART_CLEAR_FEF | UART_CLEAR_PEF);
    __HAL_UART_SEND_REQ(&huart2, UART_RXDATA_FLUSH_REQUEST);
    Haptics_Init();
    recovering = false;
    started = aborted && HAL_UART_Receive_IT(&huart2, &rx_byte, 1U) == HAL_OK;
    HAL_NVIC_ClearPendingIRQ(USART2_IRQn);
    if (started) HAL_NVIC_EnableIRQ(USART2_IRQn);
    return false;
}

bool Haptics_TransportReadByte(uint8_t *byte, uint32_t *timestamp_ms)
{
    if (!byte || !timestamp_ms || !service_transport()) return false;
    uint32_t key = Motor_Lock();
    bool available = !recovering && tail != head;
    if (available) {
        *byte = ring[tail].byte;
        *timestamp_ms = ring[tail].ms;
        tail = (tail + 1U) % RX_CAPACITY;
    }
    Motor_Unlock(key);
    return available;
}

bool Haptics_TransportTrySend(const uint8_t *frame, size_t length)
{
    if (!frame || !length || length > sizeof(tx_bytes) || !started || recovering ||
        tx_active || huart2.gState != HAL_UART_STATE_READY) return false;
    /* One in-flight transfer, no pending telemetry queue. HAL keeps this copy
     * until completion; a busy retry will gather the latest sensor snapshot. */
    memcpy(tx_bytes, frame, length);
    if (HAL_UART_Transmit_IT(&huart2, tx_bytes, (uint16_t)length) != HAL_OK) return false;
    tx_started_ms = HAL_GetTick();
    tx_active = true;
    return true;
}

void USART2_IRQHandler(void) { HAL_UART_IRQHandler(&huart2); }
#endif
