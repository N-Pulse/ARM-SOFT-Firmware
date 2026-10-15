#include "comm.h"
#ifndef MOTOR_BACKEND_SIM
#include "comms_parser.h"
#else
#include "sim_protocol.h"
#include "sim_session.h"
#endif
#include "comms_session.h"
#include "uart4_transport.h"
#include "motor_lock.h"
#include "motor_safety.h"
#ifndef MOTOR_BACKEND_SIM
#include "rx.h"
#include "tx.h"
#endif

#ifdef MOTOR_BACKEND_SIM
static sim_parser_t parser;
#else
static comms_parser_t parser;
#endif
static uint32_t current_generation;
#ifndef MOTOR_BACKEND_SIM
static uint32_t last_status_ms, last_hello_ms, sent_revision;
static bool have_sent_status;
static bool send_boot_hello(void) {
    uint8_t p[24]; size_t n=Lifecycle_EncodeHello(p,sizeof(p));
    return n && UART4Transport_QueuePayload(p,n);
}
static void service_lifecycle(void) {
    uint8_t payload[96]; uint32_t now=HAL_GetTick();
    if(!CommsSession_HasSession() && (uint32_t)(now-last_hello_ms)>=500U) {
        if(send_boot_hello()) last_hello_ms=now;
    }
    lifecycle_control_t probe;
    if(CommsSession_GetProbe(&probe)) {
        size_t n=Lifecycle_EncodeProbe(payload,sizeof(payload),&probe);
        if(!n || !UART4Transport_QueuePayload(payload,n)) {
            uint32_t k=Motor_Lock(); CommsSession_TransportErrorLocked(); Motor_Unlock(k);
        }
    }
    /* One safety record per iteration, only removed after a successful offer. */
    motor_safety_event_t event;
    if(MotorSafety_PeekEvent(&event)) {
        size_t n=Lifecycle_EncodeSafety(payload,sizeof(payload),event.ms,event.cause,
                                        event.motor_mask,event.epoch,event.state);
        if(n && UART4Transport_QueueSafetyPayload(payload,n)) MotorSafety_ConsumeEvent(event.sequence);
    }
    lifecycle_status_t status; CommsSession_GetStatus(&status);
    if(!have_sent_status || (status.revision!=sent_revision && (uint32_t)(now-last_status_ms)>=100U) || (uint32_t)(now-last_status_ms)>=1000U) {
        size_t n=Lifecycle_EncodeStatus(payload,sizeof(payload),&status);
        if(n && UART4Transport_QueuePayload(payload,n)) {
            sent_revision=status.revision; last_status_ms=now; have_sent_status=true;
        }
    }
}
#endif

void Comms_Init(void)
{
#ifndef MOTOR_BACKEND_SIM
    last_status_ms=last_hello_ms=sent_revision=0; have_sent_status=false;
#endif
#ifdef MOTOR_BACKEND_SIM
    CommsSession_Init(false);
#else
    CommsSession_Init(true);
#endif
#ifdef MOTOR_BACKEND_SIM
    SimParser_Init(&parser);
#else
    CommsParser_Init(&parser);
#endif
    UART4Transport_Init();
}
bool Comms_GetNextIntent(intent_t *out) { return CommsSession_GetIntent(out); }

#ifndef MOTOR_BACKEND_SIM
static bool frame_received(const uint8_t *bytes, size_t length, uint32_t ms,
                           uint32_t epoch, void *context)
{
    (void)context;
    rx_result_t result = HandleDeviceMessage(bytes, length);
    bool started = false;
    uint32_t key = Motor_Lock();
    if (UART4Transport_RxGenerationMatches(current_generation))
        CommsSession_OnMessage(&result, ms, epoch, &started);
    Motor_Unlock(key);
    if (started) send_boot_hello(); /* Complete framed message; zero-wait offer. */
    return result.type != RX_TYPE_NONE;
}
#else
static bool frame_received(uint8_t type,const uint8_t *p,size_t n,uint32_t ms,uint32_t epoch,void *context)
{
    if(!UART4Transport_RxGenerationMatches(current_generation)) return false;
    (void)context;
    return SimSession_OnFrame(type,p,n,ms,epoch,&current_generation);
}
#endif
void Comms_ProcessRxOnce(TickType_t wait)
{
    /* Notification wait releases the CPU, including while a frame is incomplete.
     * Bounded drain; priority below control. No timer callback or busy polling. */
    ulTaskNotifyTake(pdTRUE, wait);
#ifdef MOTOR_BACKEND_SIM
    if (UART4Transport_RecoverRx()) SimParser_Reset(&parser);
#else
    if (UART4Transport_RecoverRx()) CommsParser_Reset(&parser);
#endif
    uart4_rx_event_t event;
    for (unsigned n = 0U; n < COMMS_RX_CAPACITY && UART4Transport_PopRx(&event); ++n) {
#ifdef MOTOR_BACKEND_SIM
        if(current_generation!=event.generation) SimParser_Reset(&parser);
        current_generation=event.generation;
        SimParser_Feed(&parser,event.byte,event.ms,event.epoch,frame_received,NULL);
#else
        current_generation = event.generation;
        CommsParser_Feed(&parser, event.byte, event.ms, event.epoch, frame_received, NULL);
#endif
    }
#ifdef MOTOR_BACKEND_SIM
    SimParser_Poll(&parser, HAL_GetTick());
    SimSession_Service();
#else
    CommsParser_Poll(&parser, HAL_GetTick());
    service_lifecycle();
#endif
}
static void Comms_Task(void *unused)
{
    (void)unused;
    UART4Transport_StartRx(xTaskGetCurrentTaskHandle());
    for (;;) Comms_ProcessRxOnce(pdMS_TO_TICKS(COMMS_RX_WAIT_MS));
}
void Comms_TaskCreate(void)
{
    UART4Transport_CreateTxTask();
    BaseType_t status = xTaskCreate(Comms_Task, "comms", 768, NULL,
                                   tskIDLE_PRIORITY + 2U, NULL);
    configASSERT(status == pdPASS);
}
