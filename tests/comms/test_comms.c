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
static void session(void) { inject(hello, sizeof(hello)); drain(); }
static uint32_t epoch(void)
{ uint32_t key = Motor_Lock(); uint32_t n = MotorSafety_EpochLocked(); Motor_Unlock(key); return n; }
static void silent_for(uint32_t ms)
{ for (uint32_t n = 0; n < ms; n += 10U) step(10U, true); }
static void parser_and_rx_tests(void)
{
    fixture(); intent_t item;
    inject(close_hand, sizeof(close_hand)); drain(); assert(!Comms_GetNextIntent(&item));
    inject(hello, 3); drain(); inject(hello + 3, sizeof(hello) - 3U); drain();
    inject(close_hand, 1); drain(); inject(close_hand + 1, 2); drain();
    inject(close_hand + 3, sizeof(close_hand) - 3U); inject(close_hand, sizeof(close_hand)); drain();
    assert(Comms_GetNextIntent(&item) && item.id == ACTION_CLOSE_HAND);
    assert(Comms_GetNextIntent(&item) && item.id == ACTION_CLOSE_HAND);
    assert(!Comms_GetNextIntent(&item));
    assert(waits && waits_last == COMMS_RX_WAIT_MS); assert_stopped();
    report("HW Hello/action fragmented and consecutive; incomplete/empty RX uses RTOS notification wait");

    const uint8_t open_hand[] = {0xAA,4,0x12,2,0x0A,0};
    inject(open_hand, sizeof(open_hand)); drain();
    assert(Comms_GetNextIntent(&item) && item.id == ACTION_OPEN_HAND);
    for (unsigned action = 1U; action <= 4U; ++action) {
        uint8_t frame[sizeof(close_hand)]; memcpy(frame, close_hand, sizeof(frame));
        frame[sizeof(frame)-1U] = action; inject(frame, sizeof(frame)); drain();
        assert(Comms_GetNextIntent(&item) && (unsigned)item.id == action);
    }
    report("existing action IDs 0..4, including proto3 zero omission, accepted without arming");

    uint8_t invalid[] = {0xAA,0, 0xAA,129, 0xAA,255, 0xAA,1,0xFF,
                         0xAA,6,0x12,4,0x0A,2,0x08,99};
    inject(invalid, sizeof(invalid)); inject(close_hand, sizeof(close_hand)); drain();
    assert(Comms_GetNextIntent(&item)); assert(!Comms_GetNextIntent(&item));
    assert(HandleDeviceMessage(NULL, 1).type == RX_TYPE_NONE);
    uint8_t huge[255] = {0}; assert(HandleDeviceMessage(huge, sizeof(huge)).type == RX_TYPE_NONE);
    assert(HandleProsthesisMessage(huge, sizeof(huge)).type == RX_TYPE_NONE);
    report("null/excessive lengths, malformed protobuf and unknown action rejected, next valid frame recovered");

    /* Dropped final byte: the following AA has entered the corrupt candidate. */
    inject(close_hand, sizeof(close_hand) - 1U); inject(close_hand, sizeof(close_hand)); drain();
    assert(Comms_GetNextIntent(&item)); assert(!Comms_GetNextIntent(&item));
    inject(close_hand, 4U); drain(); silent_for(COMMS_INTERBYTE_TIMEOUT_MS + 1U); drain();
    inject(close_hand, sizeof(close_hand)); drain(); assert(Comms_GetNextIntent(&item));
    report("lost byte suffix resync and interrupted-frame timeout allow subsequent frame");
}
static void loss_tests(void)
{
    fixture(); session();
    for (unsigned i = 0; i < COMMS_INTENT_CAPACITY + 1U; ++i) { inject(close_hand, sizeof(close_hand)); drain(); }
    comms_diagnostics_t d; CommsSession_GetDiagnostics(&d);
    assert(d.queued == COMMS_INTENT_CAPACITY && d.queue_full == 1U);
    /* Repeated rejected frames/full queue cannot refresh link. */
    for (unsigned i = 0; i < 201U; ++i) {
        step(10U, true); inject(close_hand, sizeof(close_hand)); drain();
    }
    assert(MotorSafety_GetStopCauses() & MOTOR_STOP_LINK); assert_stopped();
    assert(!Motor_RecoverFromFault());
    intent_t item; assert(!Comms_GetNextIntent(&item));
    session(); assert(MotorSafety_IsFaultActive()); assert_stopped();
    assert(Motor_RecoverFromFault()); assert_stopped(); assert(!Comms_GetNextIntent(&item));
    report("queue saturation is diagnosed without false enqueue; timeout independent of full queue; Hello restores link but not latch/arming");

    fixture(); session(); inject(close_hand, sizeof(close_hand)); drain();
    assert(Comms_GetNextIntent(&item)); uint32_t old = item.epoch;
    Motor_StopAll(); assert(!Motor_SetTargetAtEpoch(MOTOR_INDEX, 1.0f, 80U, old));
    assert(Motor_RecoverFromFault()); arm(); enable(MOTOR_INDEX);
    Motor_SetTarget(MOTOR_INDEX, 0.0f, 0U);
    unsigned writes = positive_writes;
    IntentRouter_Handle(&item); step(10U, true);
    assert(positive_writes == writes);
    assert(Motor_DRV8848_DrivingDir(MOTOR_INDEX) == 0);
    report("intent popped before stop cannot be applied after explicit recovery/rearm/activation");

    fixture(); session(); inject(close_hand, sizeof(close_hand)); drain();
    assert(Comms_GetNextIntent(&item)); IntentRouter_Handle(&item);
    Motor_StopAll(); assert(Motor_RecoverFromFault()); assert_stopped();
    arm(); assert(Motor_Enable(MOTOR_INDEX));
    unsigned writes_after_arm = positive_writes; step(10U, true);
    assert(positive_writes == writes_after_arm);
    report("command application before stop is cleared, reverse ordering is rejected");

    fixture(); session();
    uint8_t noise[COMMS_RX_CAPACITY + 2U]; memset(noise, 0x55, sizeof(noise));
    inject(noise, sizeof(noise)); assert(MotorSafety_GetStopCauses() & MOTOR_STOP_LINK);
    assert_stopped(); drain(); session(); assert(Motor_RecoverFromFault()); assert_stopped();
    uart4_diagnostics_t ud; UART4Transport_GetDiagnostics(&ud); assert(ud.rx_overflow == 1U);
    report("RX overflow stops immediately and discards candidates; recovery requires a new Hello");

    fixture(); session(); inject(close_hand, 4U);
    ipsr = 1U; HAL_UART_ErrorCallback(&huart4); ipsr = 0U;
    assert(MotorSafety_GetStopCauses() & MOTOR_STOP_LINK); assert(rx_abort);
    drain(); inject(close_hand, sizeof(close_hand)); drain(); assert(!Motor_RecoverFromFault());
    session(); assert(Motor_RecoverFromFault()); assert_stopped();
    report("UART error invalidates partial RX and session, task rearms; commands alone do not restore link");

    fixture(); session(); force(MOTOR_INDEX, 1600U);
    assert(!Motor_RecoverFromFault()); session(); assert(!Motor_RecoverFromFault());
    force(MOTOR_INDEX, 100U); assert(Motor_RecoverFromFault()); assert_stopped();
    report("restored link cannot bypass a persisting force cause");

    fixture(); clock_ms = UINT32_MAX - 1000U; Motor_Update(); step(10U, true);
    /* Remove the deliberate time jump cause, then begin a session near wrap. */
    assert(Motor_RecoverFromFault()); session();
    silent_for(2000U); assert(!(MotorSafety_GetStopCauses() & MOTOR_STOP_LINK));
    step(10U, true); assert(MotorSafety_GetStopCauses() & MOTOR_STOP_LINK);
    report("session timeout uses unsigned elapsed time across HAL millisecond wrap");

    fixture(); session(); uint32_t old_epoch = epoch(); Motor_StopAll();
    assert(Motor_RecoverFromFault());
    rx_result_t action = {.type=RX_TYPE_ACTION, .data.action_id=1}; bool started;
    assert(!CommsSession_OnMessage(&action, clock_ms, old_epoch, &started));
    assert(!Comms_GetNextIntent(&item)); assert_stopped();
    report("RX backlog stamped before stop remains rejected after recovery");

    fixture(); session();
    silent_for(COMMS_LINK_TIMEOUT_MS - 10U);
    uint8_t rejected[] = {0x55, 0xAA, 1, 0xFF, 0xAA,6,0x12,4,0x0A,2,0x08,99,
                         0xAA,6,0x0A,4,0x08,1,0x10,2};
    inject(rejected, sizeof(rejected)); drain(); step(20U, true);
    assert(MotorSafety_GetStopCauses() & MOTOR_STOP_LINK); assert_stopped();
    report("noise, invalid protobuf, unknown action and incompatible Hello do not maintain link freshness");

    fixture(); session(); rx_fail = true;
    inject(close_hand, 1U); assert(MotorSafety_GetStopCauses() & MOTOR_STOP_LINK);
    drain(); assert(!Motor_RecoverFromFault()); rx_fail = false;
    drain(); session(); assert(Motor_RecoverFromFault()); assert_stopped();
    report("HAL RX rearm failure is diagnosed and cannot hide the link stop; task recovery retries");
}
static unsigned parsed_frames;
static bool count_frame(const uint8_t *bytes, size_t length, uint32_t ms, uint32_t generation, void *context)
{
    (void)ms; (void)generation; (void)context;
    bool valid = HandleDeviceMessage(bytes, length).type != RX_TYPE_NONE;
    if (valid) ++parsed_frames;
    return valid;
}
static void parser_time_tests(void)
{
    comms_parser_t p; CommsParser_Init(&p); parsed_frames = 0;
    uint32_t ms = UINT32_MAX - 5U;
    for (unsigned i = 0; i < sizeof(hello); ++i)
        CommsParser_Feed(&p, hello[i], ms + (i >= 3U ? 20U : 0U), 0U, count_frame, NULL);
    assert(parsed_frames == 1U && p.timeouts == 0U);
    CommsParser_Init(&p); parsed_frames = 0;
    for (unsigned i = 0; i < sizeof(hello); ++i)
        CommsParser_Feed(&p, hello[i], ms + i * 20U, 0U, count_frame, NULL);
    assert(parsed_frames == 0U && p.timeouts == 1U);
    for (unsigned i = 0; i < sizeof(hello); ++i)
        CommsParser_Feed(&p, hello[i], ms + 200U, 0U, count_frame, NULL);
    assert(parsed_frames == 1U);
    CommsParser_Init(&p); parsed_frames = 0;
    for (unsigned i = 0; i < 3U; ++i) CommsParser_Feed(&p, hello[i], ms, 0U, count_frame, NULL);
    CommsParser_Poll(&p, ms + COMMS_INTERBYTE_TIMEOUT_MS + 1U);
    assert(p.used == 0U && p.timeouts == 1U);
    report("production parser total/interbyte deadlines survive wrap; a valid fragmented frame spanning wrap is accepted");
}
static void tx_tests(void)
{
    fixture();
    for (unsigned i = 0; i < COMMS_TX_CAPACITY; ++i)
        assert(UART4Transport_QueuePayload(close_hand + 2U, 6U));
    assert(!UART4Transport_QueuePayload(close_hand + 2U, 6U));
    uint32_t key = Motor_Lock(); assert(!UART4Transport_QueuePayload(close_hand + 2U, 6U)); Motor_Unlock(key);
    current_task = 1U;
    for (unsigned i = 0; i < COMMS_TX_CAPACITY; ++i) {
        assert(UART4Transport_TxOnce(10U)); assert(wire_size == sizeof(close_hand));
        assert(!memcmp(wire, close_hand, sizeof(close_hand)));
    }
    assert(uart_tx_calls == COMMS_TX_CAPACITY);
    report("TX queue saturation and complete frame serialization; offering under Motor_Lock is refused");
    assert(UART4Transport_QueuePayload(close_hand + 2U, 6U)); tx_auto_complete = false;
    assert(!UART4Transport_TxOnce(10U)); assert(tx_abort);
    tx_auto_complete = true; tx_fail = true;
    assert(UART4Transport_QueuePayload(close_hand + 2U, 6U)); assert(!UART4Transport_TxOnce(10U));
    tx_fail = false;
    assert(UART4Transport_QueuePayload(close_hand + 2U, 6U)); Motor_StopAll();
    unsigned calls = uart_tx_calls; assert(!UART4Transport_TxOnce(10U)); assert(uart_tx_calls == calls);
    uart4_diagnostics_t d; UART4Transport_GetDiagnostics(&d);
    assert(d.tx_full == 1U && d.tx_timeouts == 1U && d.tx_errors == 1U && d.tx_stale == 1U);
    report("TX failure and completion timeout are bounded RTOS waits; old queued TX generation discarded");
}
int main(void)
{
    scenarios = 0U;
    fixture();
    const uint8_t stop_request[]={0xAA,2,0x22,0};
    inject(stop_request,sizeof(stop_request)); drain();
    assert(MotorSafety_GetStopCauses()&MOTOR_STOP_MANUAL); assert_stopped();
    report("framed HW STOP accepted without Hello, arm, enable or valid previous command state");
    fixture();
    const uint8_t commission[]={0xAA,8,0x1a,6,0x08,1,0x18,5,0x20,1};
    assert(HandleDeviceMessage(commission+2,sizeof(commission)-2).type==RX_TYPE_COMMISSION);
    hw_command_t c={.operation=5,.motor_id=MOTOR_INDEX};
    assert(HWCommission_Execute(&c,clock_ms,epoch())==HW_PRODUCTION);
    inject(commission,sizeof(commission)); drain(); assert_stopped();
    report("production rejects commissioning protobuf commands and cannot reach arm/enable");
    fixture(); session();
    current_task=1U;
    while(UART4Transport_TxOnce(0)) {}
    for(unsigned i=0;i<COMMS_TX_CAPACITY+1;++i) HWTelemetry_Service();
    uart4_diagnostics_t saturated; UART4Transport_GetDiagnostics(&saturated);
    assert(saturated.tx_full>0);
    inject(stop_request,sizeof(stop_request)); drain(); assert_stopped();
    report("saturated diagnostic TX drops zero-wait telemetry; incoming STOP still inhibits all outputs");
    parser_and_rx_tests(); loss_tests(); tx_tests(); parser_time_tests();
    fixture(); current_task = 1U;
    assert(UART4Transport_QueuePayload(close_hand + 2U, 6U));
    stop_at_notify = true;
    bool sent = UART4Transport_TxOnce(0U);
    printf("REPRO TX: fault=%d HAL calls=%u\n", MotorSafety_IsFaultActive(), uart_tx_calls);
    assert(!sent && uart_tx_calls == 0U);
    report("STOP during notification drain rejects frame before commitment");
    for (unsigned phase=1;phase<=4;++phase) for(unsigned recovery=0;recovery<=1;++recovery) {
        fixture(); current_task=1U; recover_injection=recovery;
        assert(UART4Transport_QueuePayload(close_hand+2U,6U));
        if(phase==2) stop_at_notify=true; else tx_injection=phase;
        bool complete=UART4Transport_TxOnce(0U);
        uart4_diagnostics_t ud; UART4Transport_GetDiagnostics(&ud);
        if(phase<=2) { assert(!complete && !uart_tx_calls && ud.tx_stale==1U); }
        else { assert(complete && uart_tx_calls==1U && ud.tx_complete==1U && ud.tx_stale==0U); }
        assert_stopped();
    }
    report("TX phase ordering: popped/drained frames rejected; IRQ pending after validation or at HAL start executes after commitment, with/without recovery");
    fixture(); current_task=1U; tx_auto_complete=false;
    assert(UART4Transport_QueuePayload(close_hand+2U,6U)); assert(!UART4Transport_TxOnce(0U));
    ipsr=1U; HAL_UART_TxCpltCallback(&huart4); ipsr=0U;
    notifications[1]=1U; late_wakeup=true;
    assert(UART4Transport_QueuePayload(close_hand+2U,6U)); assert(!UART4Transport_TxOnce(0U));
    tx_auto_complete=true;
    assert(UART4Transport_QueuePayload(close_hand+2U,6U)); assert(UART4Transport_TxOnce(0U));
    uart4_diagnostics_t ud; UART4Transport_GetDiagnostics(&ud);
    assert(ud.tx_timeouts==2U && ud.tx_complete==1U);
    report("late callback/wakeup after abort and during next BUSY transfer cannot complete it; next true completion succeeds");
    fixture(); current_task=1U; tx_auto_complete=false; abort_fail=true;
    assert(UART4Transport_QueuePayload(close_hand+2U,6U)); assert(!UART4Transport_TxOnce(0U));
    unsigned attempts=uart_tx_calls;
    assert(UART4Transport_QueuePayload(close_hand+2U,6U)); assert(!UART4Transport_TxOnce(0U));
    assert(uart_tx_calls==attempts);
    UART4Transport_GetDiagnostics(&ud); assert(ud.tx_timeouts==1U && ud.tx_errors==1U);
    report("failed HAL abort quarantines persistent buffer and prevents further starts");
    printf("%u lot-3 production scenarios passed; HAL/BSP/RTOS scheduling mocked.\n", scenarios);
    return 0;
}
