/* Mock UART/RTOS fixture shared by streaming and handshake tests. */
/* Reuse the lot-2 HAL/BSP fixture, not an alternative safety implementation. */
#define TEST_COMMS_FIXTURE
#define main lot2_fixture_suite
#include "../safety/test_safety.c"
#undef main
#include "comm.h"
#include "comms_session.h"
#include "comms_parser.h"
#include "uart4_transport.h"
#include "board_uart_compat.h"
#include "queue.h"
#include "motor_sim_feedback.h"
#include "motor_lock.h"
#include "tx.h"
#include "hw_telemetry.h"

UART_HandleTypeDef huart4;
static uint8_t *rx_destination;
static uint32_t ipsr, notifications[3], waits, waits_last, uart_tx_calls, rx_abort, tx_abort;
static bool tx_auto_complete, tx_fail, rx_fail, abort_fail;
static bool stop_at_notify;
static unsigned tx_injection;
static bool stop_pending_irq, recover_injection, late_wakeup;
static void comms_restore_irq(void) {
    if (stop_pending_irq) {
        stop_pending_irq=false; Motor_StopAll();
        if (recover_injection) assert(Motor_RecoverFromFault());
    }
}
static void comms_instrument_exit(void *function) {
    if (tx_injection == 3U && function == (void *)MotorSafety_EpochLocked) {
        tx_injection=0; assert(primask); stop_pending_irq=true;
    }
}
static uint8_t wire[130];
static uint16_t wire_size;
static unsigned queue_head, queue_count, queue_size, queue_item;
static uint8_t queue_store[8][160];
static unsigned current_task = 2U;

uint32_t __get_IPSR(void) { return ipsr; }
void HAL_NVIC_EnableIRQ(int irq) { assert(!primask && irq == UART4_IRQn); }
void HAL_NVIC_DisableIRQ(int irq) { assert(!primask && irq == UART4_IRQn); }
void HAL_NVIC_ClearPendingIRQ(int irq) { assert(!primask && irq == UART4_IRQn); }
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *h, uint8_t *p, uint16_t n)
{ assert(!primask && h == &huart4 && n == 1U); rx_destination = p; return rx_fail ? HAL_ERROR : HAL_OK; }
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *h)
{ assert(!primask && h == &huart4); ++rx_abort; return HAL_OK; }
HAL_StatusTypeDef HAL_UART_AbortTransmit(UART_HandleTypeDef *h)
{ assert(!primask && h == &huart4); ++tx_abort; h->gState = HAL_UART_STATE_READY; return abort_fail ? HAL_ERROR : HAL_OK; }
HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef *h, uint8_t *p, uint16_t n)
{
    assert(primask && !ipsr && h == &huart4 && n <= sizeof(wire));
    h->gState = HAL_UART_STATE_BUSY_TX; h->TxXferCount = n;
    if (tx_injection == 4U) { tx_injection=0; stop_pending_irq=true; }
    ++uart_tx_calls; memcpy(wire, p, n); wire_size = n;
    return tx_fail ? HAL_ERROR : HAL_OK;
}
BaseType_t xTaskCreate(void (*fn)(void *), const char *name, unsigned stack,
                       void *arg, unsigned priority, TaskHandle_t *handle)
{
    (void)fn; (void)stack; (void)arg;
    assert(priority < 3U);
    if (handle) *handle = (TaskHandle_t)1;
    assert(!strcmp(name, "comms") || !strcmp(name, "uart4tx"));
    return pdPASS;
}
TickType_t xTaskGetTickCount(void) { return clock_ms; }
TaskHandle_t xTaskGetCurrentTaskHandle(void) { return (TaskHandle_t)(uintptr_t)current_task; }
uint32_t ulTaskNotifyTake(BaseType_t clear, TickType_t wait)
{
    assert(!primask && !ipsr); (void)clear;
    if (stop_at_notify && !wait) { stop_at_notify = false; Motor_StopAll(); if(recover_injection) assert(Motor_RecoverFromFault()); }
    if (late_wakeup && current_task == 1U && wait) {
        late_wakeup=false;
        /* Previous completion cannot match HAL's BUSY new transfer. */
        ipsr=1U; HAL_UART_TxCpltCallback(&huart4); ipsr=0U;
        ++notifications[1];
    }
    if (wait) { ++waits; waits_last = wait; if (current_task == 1U && !tx_auto_complete) clock_ms += wait; }
    if (current_task == 1U && wait && tx_auto_complete) {
        huart4.gState = HAL_UART_STATE_READY; huart4.TxXferCount = 0U;
        ipsr = 1U; HAL_UART_TxCpltCallback(&huart4); ipsr = 0U;
    }
    uint32_t n = notifications[current_task]; notifications[current_task] = 0U; return n;
}
void vTaskNotifyGiveFromISR(TaskHandle_t task, BaseType_t *woken)
{ assert(ipsr && !primask); ++notifications[(uintptr_t)task]; *woken = pdFALSE; }
void xTaskNotifyGive(TaskHandle_t task) { assert(!primask && !ipsr); ++notifications[(uintptr_t)task]; }
void vTaskDelay(TickType_t wait) { assert(!primask && !ipsr && wait); ++waits; }
QueueHandle_t xQueueCreate(unsigned size, unsigned item)
{
    assert(size <= 8U && item <= 160U);
    queue_head = queue_count = 0U; queue_size = size; queue_item = item; return (void *)1;
}
BaseType_t xQueueSend(QueueHandle_t q, const void *item, TickType_t wait)
{
    assert(q && !primask && !ipsr && wait == 0U);
    if (queue_count == queue_size) return pdFALSE;
    memcpy(queue_store[(queue_head + queue_count++) % queue_size], item, queue_item); return pdPASS;
}
BaseType_t xQueueReceive(QueueHandle_t q, void *out, TickType_t wait)
{
    assert(q && !primask && !ipsr); if (wait) ++waits;
    if (!queue_count) return pdFALSE;
    memcpy(out, queue_store[queue_head], queue_item);
    queue_head = (queue_head + 1U) % queue_size; --queue_count;
    if (tx_injection == 1U) { tx_injection=0; Motor_StopAll(); if(recover_injection) assert(Motor_RecoverFromFault()); }
    return pdPASS;
}
static const uint8_t hello[] = {0xAA, 6, 0x0A, 4, 0x08, 1, 0x10, 1};
static const uint8_t close_hand[] = {0xAA, 6, 0x12, 4, 0x0A, 2, 0x08, 1};
static void inject(const uint8_t *bytes, size_t n)
{
    assert(rx_destination);
    for (size_t i = 0; i < n; ++i) {
        *rx_destination = bytes[i]; ipsr = 1U; HAL_UART_RxCpltCallback(&huart4); ipsr = 0U;
    }
}
static void drain(void) { current_task = 2U; Comms_ProcessRxOnce(COMMS_RX_WAIT_MS); }
static void fixture(void)
{
    reset(); ipsr = 0; memset(notifications, 0, sizeof(notifications));
    waits = waits_last = uart_tx_calls = rx_abort = tx_abort = 0U;
    tx_auto_complete = true; tx_fail = rx_fail = abort_fail = false;
    tx_injection=0; stop_at_notify=stop_pending_irq=recover_injection=late_wakeup=false;
    huart4.gState=HAL_UART_STATE_READY; huart4.TxXferCount=0U;
    Comms_Init(); Comms_TaskCreate();
    assert(UART4Transport_StartRx((void *)2));
}
static inline uint32_t epoch(void)
{ uint32_t key=Motor_Lock(); uint32_t n=MotorSafety_EpochLocked(); Motor_Unlock(key); return n; }
