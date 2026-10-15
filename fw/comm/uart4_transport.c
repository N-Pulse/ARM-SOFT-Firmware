#include "uart4_transport.h"
#include "board_uart_compat.h"
#include "queue.h"
#include "motor_lock.h"
#include "motor_safety.h"
#include "comms_session.h"
#ifdef MOTOR_BACKEND_SIM
#include "sim_session.h"
#include "motor_sim_config.h"
#endif
#include <string.h>
#if COMMS_UART_IRQ_PRIORITY < configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY
#error UART4 IRQ cannot call FreeRTOS above its configured syscall ceiling
#endif

typedef struct {
    uint16_t length; uint32_t epoch; bool safety_record;
#ifdef MOTOR_BACKEND_SIM
    uint32_t offered_ms; uint8_t slot;
#endif
    uint8_t bytes[COMMS_PAYLOAD_MAX + 2U];
} tx_frame_t;
static uart4_rx_event_t ring[COMMS_RX_CAPACITY];
static unsigned head, tail;
static uint32_t generation;
static volatile bool rx_recovery;
static uint8_t rx_byte;
static TaskHandle_t rx_task, tx_task;
static QueueHandle_t tx_queue;
static tx_frame_t transmitting; /* HAL IT retains this storage until complete/abort. */
static uart4_diagnostics_t diag;
static volatile bool tx_active, tx_completed;
static bool tx_quarantined;
#ifdef MOTOR_BACKEND_SIM
static tx_frame_t sim_slots[3];
static bool sim_pending[3];
#endif

void UART4Transport_Init(void)
{
    head = tail = generation = 0U;
    rx_recovery = false; rx_task = tx_task = NULL;
    memset(&diag, 0, sizeof(diag));
    tx_active = tx_completed = tx_quarantined = false;
#ifdef MOTOR_BACKEND_SIM
    memset(sim_pending,0,sizeof(sim_pending));
#endif
    tx_queue = xQueueCreate(COMMS_TX_CAPACITY, sizeof(tx_frame_t));
    configASSERT(tx_queue != NULL);
}
static void rx_fault(bool overflow)
{
    uint32_t key = Motor_Lock();
    if (overflow) ++diag.rx_overflow; else ++diag.rx_errors;
    ++generation; rx_recovery = true; tail = head;
    /* Immediate local stop, independent of RTOS queues and RX task progress. */
    CommsSession_TransportErrorLocked();
#ifdef MOTOR_BACKEND_SIM
    SimSession_TransportErrorLocked();
#endif
    Motor_Unlock(key);
}
static void notify_rx(void)
{
    BaseType_t woken = pdFALSE;
    if (rx_task) vTaskNotifyGiveFromISR(rx_task, &woken);
    portYIELD_FROM_ISR(woken);
}
bool UART4Transport_StartRx(TaskHandle_t owner)
{
    configASSERT(owner != NULL && tx_queue != NULL);
    rx_task = owner;
    HAL_NVIC_ClearPendingIRQ(UART4_IRQn);
    HAL_NVIC_EnableIRQ(UART4_IRQn);
    if (HAL_UART_Receive_IT(BoardUART_GetHandle(), &rx_byte, 1U) == HAL_OK) return true;
    ++diag.rx_start_errors;
    rx_fault(false);
    return false;
}
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart)
{
    if (uart != BoardUART_GetHandle() || !rx_task || rx_recovery) return;
    uint32_t key = Motor_Lock();
    unsigned next = (head + 1U) % COMMS_RX_CAPACITY;
    bool full = next == tail;
    if (!full) {
        ring[head] = (uart4_rx_event_t){ .byte = rx_byte, .ms = HAL_GetTick(),
            .epoch = MotorSafety_EpochLocked(), .generation = generation };
        head = next; ++diag.rx_bytes;
    }
    Motor_Unlock(key);
    if (full) rx_fault(true);
    else if (HAL_UART_Receive_IT(uart, &rx_byte, 1U) != HAL_OK) rx_fault(false);
    notify_rx();
}
void HAL_UART_ErrorCallback(UART_HandleTypeDef *uart)
{
    if (uart != BoardUART_GetHandle() || !rx_task) return;
    rx_fault(false);
    /* End IT RX now, including nonblocking FE/NE errors which HAL may leave
     * BUSY_RX. No DMA, wait or decoding here; recovery belongs to the task. */
    HAL_UART_AbortReceive(uart);
    notify_rx();
}
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *uart)
{
    if (uart != BoardUART_GetHandle() || !tx_task) return;
    /* Only HAL's ended transfer can complete the active owner. A late wakeup
     * alone is never evidence of completion. Abort disables TX IRQ sources. */
    if (!tx_active || uart->gState != HAL_UART_STATE_READY || uart->TxXferCount != 0U) return;
    tx_completed = true;
    BaseType_t woken = pdFALSE;
    vTaskNotifyGiveFromISR(tx_task, &woken);
    portYIELD_FROM_ISR(woken);
}
bool UART4Transport_PopRx(uart4_rx_event_t *out)
{
    uint32_t key = Motor_Lock();
    bool present = !rx_recovery && tail != head;
    if (present) { *out = ring[tail]; tail = (tail + 1U) % COMMS_RX_CAPACITY; }
    Motor_Unlock(key); return present;
}
bool UART4Transport_RxGenerationMatches(uint32_t value)
{
    uint32_t key = Motor_Lock();
    bool result = !rx_recovery && value == generation;
    Motor_Unlock(key); return result;
}
bool UART4Transport_RecoverRx(void)
{
    if (!rx_recovery || !rx_task) return false;
    HAL_NVIC_DisableIRQ(UART4_IRQn);
    HAL_UART_AbortReceive(BoardUART_GetHandle());
    __HAL_UART_CLEAR_FLAG(BoardUART_GetHandle(), UART_CLEAR_OREF | UART_CLEAR_NEF | UART_CLEAR_FEF | UART_CLEAR_PEF);
    __HAL_UART_SEND_REQ(BoardUART_GetHandle(), UART_RXDATA_FLUSH_REQUEST);
    uint32_t key = Motor_Lock();
    tail = head; rx_recovery = false;
    Motor_Unlock(key);
    if (HAL_UART_Receive_IT(BoardUART_GetHandle(), &rx_byte, 1U) != HAL_OK) {
        ++diag.rx_start_errors; rx_fault(false);
    }
    HAL_NVIC_ClearPendingIRQ(UART4_IRQn);
    HAL_NVIC_EnableIRQ(UART4_IRQn);
    return true;
}
bool UART4Transport_QueueFrameAtEpoch(const uint8_t *bytes, size_t size, uint32_t epoch)
{
    if (!bytes || !size || size > sizeof(transmitting.bytes) || !tx_queue ||
        __get_PRIMASK() || __get_IPSR()) return false;
    tx_frame_t frame = { .length = (uint16_t)size };
    memcpy(frame.bytes, bytes, size);
    frame.epoch = epoch;
    uint32_t key = Motor_Lock();
    bool stale = epoch != MotorSafety_EpochLocked();
    if (stale) ++diag.tx_stale;
    Motor_Unlock(key);
    if (stale) return false;
    bool queued = xQueueSend(tx_queue, &frame, 0U) == pdPASS;
    key = Motor_Lock();
    if (queued) ++diag.tx_queued; else ++diag.tx_full;
    Motor_Unlock(key); return queued;
}
bool UART4Transport_QueueFrame(const uint8_t *bytes, size_t size)
{
    uint32_t key = Motor_Lock(); uint32_t epoch = MotorSafety_EpochLocked(); Motor_Unlock(key);
    return UART4Transport_QueueFrameAtEpoch(bytes, size, epoch);
}
bool UART4Transport_QueuePayload(const uint8_t *payload, size_t length)
{
    if (!payload || !length || length > COMMS_PAYLOAD_MAX) return false;
    uint8_t frame[COMMS_PAYLOAD_MAX + 2U];
    frame[0] = 0xAAU; frame[1] = (uint8_t)length;
    memcpy(frame + 2U, payload, length);
    return UART4Transport_QueueFrame(frame, length + 2U);
}
bool UART4Transport_QueueSafetyPayload(const uint8_t *payload,size_t length) {
    if(!payload || !length || length>COMMS_PAYLOAD_MAX || !tx_queue || __get_PRIMASK() || __get_IPSR()) return false;
    tx_frame_t frame={.length=(uint16_t)(length+2),.safety_record=true};
    frame.bytes[0]=0xaa; frame.bytes[1]=(uint8_t)length; memcpy(frame.bytes+2,payload,length);
    bool ok=xQueueSend(tx_queue,&frame,0)==pdPASS;
    uint32_t k=Motor_Lock(); if(ok) ++diag.tx_queued; else ++diag.tx_full; Motor_Unlock(k); return ok;
}
#ifdef MOTOR_BACKEND_SIM
void UART4Transport_DiscardSimOutputLocked(void) { sim_pending[0]=false; }
bool UART4Transport_QueueSimFrame(const uint8_t *bytes,size_t size,uint32_t epoch,uint32_t ms,uint8_t slot)
{
    if(!bytes || !size || size>sizeof(transmitting.bytes) || slot>2U || !tx_queue ||
       __get_PRIMASK() || __get_IPSR()) return false;
    uint32_t key=Motor_Lock();
    if(epoch!=MotorSafety_EpochLocked()) { ++diag.tx_stale; Motor_Unlock(key); return false; }
    tx_frame_t *frame=&sim_slots[slot];
    if(sim_pending[slot]) ++diag.tx_replaced;
    frame->length=(uint16_t)size; frame->epoch=epoch; frame->offered_ms=ms; frame->slot=slot;
    memcpy(frame->bytes,bytes,size); sim_pending[slot]=true; ++diag.tx_queued;
    if(slot==2U) sim_pending[0]=false;
    Motor_Unlock(key);
    if(tx_task) xTaskNotifyGive(tx_task);
    return true;
}
static bool take_sim_frame(TickType_t wait)
{
    for(unsigned attempt=0U;attempt<2U;++attempt) {
        uint32_t key=Motor_Lock();
        for(int i=2;i>=0;--i) if(sim_pending[i]) {
            transmitting=sim_slots[i]; sim_pending[i]=false; Motor_Unlock(key); return true;
        }
        Motor_Unlock(key);
        if(!attempt) ulTaskNotifyTake(pdTRUE,wait);
    }
    return false;
}
#endif
bool UART4Transport_TxOnce(TickType_t wait)
{
    if (tx_quarantined) { vTaskDelay(pdMS_TO_TICKS(COMMS_TX_TIMEOUT_MS)); return false; }
#ifdef MOTOR_BACKEND_SIM
    if (!take_sim_frame(wait)) return false;
#else
    if (xQueueReceive(tx_queue, &transmitting, wait) != pdPASS) return false;
#endif
    /* RTOS operations precede commitment and remain outside PRIMASK. */
    ulTaskNotifyTake(pdTRUE, 0U);
    uint32_t key = Motor_Lock();
    if (!transmitting.safety_record && transmitting.epoch != MotorSafety_EpochLocked()) {
        ++diag.tx_stale;
        Motor_Unlock(key);
        return false;
    }
#ifdef MOTOR_BACKEND_SIM
    if(transmitting.slot==0U && (uint32_t)(HAL_GetTick()-transmitting.offered_ms)>SIM_OUTPUT_QUEUE_MAX_MS) {
        ++diag.tx_expired; Motor_Unlock(key); return false;
    }
#endif
    tx_completed = false;
    /* Local HAL 1311..1370: fixed state/pointer work, no wait/callback/DMA.
     * ATOMIC_SET_BIT expands to LDREX/STREX retry, not peripheral polling:
     * PRIMASK excludes every returning interrupt/task switch; our NMI/fault
     * handlers halt permanently. The sole UART owner has no competing writer
     * to this control register, so the exclusive pair cannot be interrupted.
     * The TXEIE/TXFTIE enable is commitment in the same lock domain as STOP.
     * This justification must be re-audited if HAL/exception ownership changes.
     * Bits already committed cannot be recalled. No blocking UART here. */
    HAL_StatusTypeDef status = HAL_UART_Transmit_IT(BoardUART_GetHandle(), transmitting.bytes, transmitting.length);
    tx_active = status == HAL_OK;
    if (status != HAL_OK) ++diag.tx_errors;
    Motor_Unlock(key);
    if (status == HAL_OK) {
        TickType_t started = xTaskGetTickCount();
        TickType_t remaining = pdMS_TO_TICKS(COMMS_TX_TIMEOUT_MS);
        for (;;) {
            ulTaskNotifyTake(pdTRUE, remaining);
            key = Motor_Lock();
            bool complete = tx_completed;
            if (complete) { tx_active = false; ++diag.tx_complete; }
            Motor_Unlock(key);
            if (complete) return true;
            TickType_t elapsed = xTaskGetTickCount() - started;
            if (elapsed >= pdMS_TO_TICKS(COMMS_TX_TIMEOUT_MS)) break;
            remaining = pdMS_TO_TICKS(COMMS_TX_TIMEOUT_MS) - elapsed;
        }
        key = Motor_Lock(); ++diag.tx_timeouts; Motor_Unlock(key);
    }
    key = Motor_Lock(); tx_active = false;
    CommsSession_TransportErrorLocked();
#ifdef MOTOR_BACKEND_SIM
    SimSession_TransportErrorLocked();
#endif
    Motor_Unlock(key);
    /* Outside lock: even an unexpected DMA configuration cannot wait under
     * interlock. Failed abort quarantines the buffer/owner instead of reuse. */
    if (HAL_UART_AbortTransmit(BoardUART_GetHandle()) != HAL_OK) {
        key = Motor_Lock(); ++diag.tx_errors; tx_quarantined = true; Motor_Unlock(key);
    }
    vTaskDelay(pdMS_TO_TICKS(COMMS_FRAME_TIMEOUT_MS + 1U));
    return false;
}
static void tx_worker(void *unused)
{
    (void)unused;
    HAL_NVIC_EnableIRQ(UART4_IRQn); /* Objects and tasks are now initialized. */
    for (;;) UART4Transport_TxOnce(portMAX_DELAY);
}
void UART4Transport_CreateTxTask(void)
{
    BaseType_t status = xTaskCreate(tx_worker, "uart4tx", 384, NULL,
                                    tskIDLE_PRIORITY + 1U, &tx_task);
    configASSERT(status == pdPASS);
}
void UART4Transport_GetDiagnostics(uart4_diagnostics_t *out)
{
    if (!out) return;
    uint32_t key = Motor_Lock(); *out = diag; Motor_Unlock(key);
}
