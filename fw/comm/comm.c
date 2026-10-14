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
#include "hw_commission.h"
#include "hw_telemetry.h"
#if N_PULSE_DUAL_BOARD_LINK
#include "board_link.h"
#endif
#endif

#ifdef MOTOR_BACKEND_SIM
static sim_parser_t parser;
#else
static comms_parser_t parser;
#endif
static uint32_t current_generation;

void Comms_Init(void)
{
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
    if (result.type == RX_TYPE_STOP) {
        Motor_StopAll();
        HWTelemetry_Reply(0U, HW_OK);
        return true;
    }
    if (result.type == RX_TYPE_COMMISSION) {
#if N_PULSE_DUAL_BOARD_LINK
        if (result.command.operation==12 || result.command.operation==13) {
            uint32_t code=UART4Transport_RxGenerationMatches(current_generation) && result.command.epoch==epoch ?
                BoardLink_Operator(result.command.operation,result.command.request_id,epoch,ms) : HW_STALE;
            HWTelemetry_Reply(result.command.request_id,code);
            return code==HW_OK;
        }
#endif
        uint32_t code = UART4Transport_RxGenerationMatches(current_generation) ?
            HWCommission_Execute(&result.command, ms, epoch) : HW_STALE;
        HWTelemetry_Reply(result.command.request_id, code);
        return code == HW_OK;
    }
    if (result.type == RX_TYPE_DIAGNOSTIC) {
        HWTelemetry_Select(result.data.mode);
        return true;
    }
    bool started = false;
    uint32_t key = Motor_Lock();
    if (UART4Transport_RxGenerationMatches(current_generation))
        CommsSession_OnMessage(&result, ms, epoch, &started);
    Motor_Unlock(key);
    if (started) SendHelloMessage(); /* Complete framed message; zero-wait offer. */
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
