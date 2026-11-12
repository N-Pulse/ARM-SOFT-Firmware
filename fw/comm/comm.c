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
#include "hw_test_config.h"
#include "handshake_config.h"
#include "handshake_protocol.h"
#include "data_protocol.h"
#ifndef MOTOR_BACKEND_SIM
#include "rx.h"
#include "tx.h"
#include "hw_commission.h"
#include "hw_telemetry.h"
#endif

#ifdef MOTOR_BACKEND_SIM
static sim_parser_t parser;
#else
static comms_parser_t parser;
#endif
static uint32_t current_generation;
#ifndef MOTOR_BACKEND_SIM
static uint32_t last_status_ms, last_sync_ms, sent_revision;
static bool have_sent_status, sync_sent;
static uint8_t pending_reply[HANDSHAKE_HELLO_SIZE];
static bool reply_pending, reply_queued, reply_matches;
static uint32_t reply_epoch, current_handshake_generation;
static bool sync_handshake_generation(void) {
    uint32_t key=Motor_Lock();
    uint32_t generation=CommsSession_HandshakeGenerationLocked();
    Motor_Unlock(key);
    if(current_handshake_generation!=generation) {
        CommsParser_Reset(&parser);
        reply_pending=reply_queued=sync_sent=false;
        current_handshake_generation=generation;
        return true;
    }
    return false;
}
static void service_lifecycle(void) {
    sync_handshake_generation();
    if(N_PULSE_HW_SINGLE_MOTOR_TEST) return; /* Commissioning retains its independent monitor. */
    uint8_t payload[96]; uint32_t now=HAL_GetTick();
    if(!CommsSession_HasSession()) {
        uint32_t key=Motor_Lock();
        uint32_t epoch=MotorSafety_EpochLocked();
        Motor_Unlock(key);
        if(reply_pending && reply_epoch!=epoch) {
            reply_pending=reply_queued=false; sync_sent=false;
        }
        if(reply_pending) {
            if(!reply_queued)
                reply_queued=UART4Transport_QueueHandshakeReply(pending_reply,
                    sizeof(pending_reply),reply_epoch,current_handshake_generation,reply_matches);
            return;
        }
        if(CommsSession_GetState()==APP_INIT_HANDSHAKE && !MotorSafety_IsFaultActive() &&
           (!sync_sent || (uint32_t)(now-last_sync_ms)>=HANDSHAKE_SYNC_PERIOD_MS)) {
            uint8_t sync[HANDSHAKE_SYNC_SIZE];
            size_t n=Handshake_EncodeSync(sync,sizeof(sync));
            if(UART4Transport_QueueFrame(sync,n)) { last_sync_ms=now; sync_sent=true; }
        }
        return;
    }
    reply_pending=reply_queued=false;
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
    last_status_ms=last_sync_ms=sent_revision=0; have_sent_status=sync_sent=false;
    current_generation=0;
    reply_pending=reply_queued=false; reply_epoch=current_handshake_generation=0;
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

#ifndef MOTOR_BACKEND_SIM
static bool frame_received(const uint8_t *bytes, size_t length, uint32_t ms,
                           uint32_t epoch, void *context)
{
    (void)context;
    bool data_packet=length && bytes[0]==DATA_PACKET_TYPE;
    if(data_packet) {
        const uint8_t *data; size_t data_length;
        if(!Data_Decode(bytes,length,&data,&data_length)) return false;
        uint32_t key=Motor_Lock();
        if(UART4Transport_RxGenerationMatches(current_generation))
            CommsSession_RecordPeerStatusLocked(bytes+3U,HANDSHAKE_STATUS_LEN,bytes[1],ms,
                epoch,current_handshake_generation);
        Motor_Unlock(key);
        bytes=data; length=data_length;
    }
    if(!data_packet && length==HANDSHAKE_HELLO_SIZE && bytes[0]==HANDSHAKE_HELLO) {
        bool matches;
        if(N_PULSE_HW_SINGLE_MOTOR_TEST || !Handshake_DecodeHello(bytes,length,&matches)) return false;
        uint32_t key=Motor_Lock();
        bool allowed=UART4Transport_RxGenerationMatches(current_generation) &&
            epoch==MotorSafety_EpochLocked() &&
            current_handshake_generation==CommsSession_HandshakeGenerationLocked() && CommsSession_CanReceiveHelloLocked() &&
            (CommsSession_StateLocked()!=APP_INIT_HANDSHAKE || sync_sent) &&
            (uint32_t)(HAL_GetTick()-ms)<=COMMS_FRAME_TIMEOUT_MS;
        Motor_Unlock(key);
        if(allowed) {
            key=Motor_Lock(); CommsSession_RecordActivityLocked(ms,epoch); Motor_Unlock(key);
        }
        if(reply_queued && CommsSession_HasSession()) reply_pending=reply_queued=false;
        if(allowed && !reply_pending) {
            Handshake_EncodeReply(pending_reply,sizeof(pending_reply),bytes[1],matches);
            reply_epoch=epoch; reply_matches=matches; reply_pending=true;
            reply_queued=UART4Transport_QueueHandshakeReply(pending_reply,
                sizeof(pending_reply),epoch,current_handshake_generation,matches);
        }
        return true;
    }
#if !N_PULSE_HW_SINGLE_MOTOR_TEST
    /* Production intents must carry the new DATA length/CRC envelope. Legacy
     * AA/protobuf actions would otherwise bypass the negotiated integrity check. */
    if(!data_packet) return true;
#endif
    rx_result_t result = HandleDeviceMessage(bytes, length);
    if(data_packet && result.type!=RX_TYPE_ACTION) return true;
    /* Remote safety/control commands are not part of the hardware protocol. */
    if(result.type==RX_TYPE_STOP || result.type==RX_TYPE_CONTROL) return true;
    if(result.type==RX_TYPE_COMMISSION) {
#if N_PULSE_HW_SINGLE_MOTOR_TEST
        uint32_t code=UART4Transport_RxGenerationMatches(current_generation) ?
            HWCommission_Execute(&result.command,ms,epoch) : HW_STALE;
        HWTelemetry_Reply(result.command.request_id,code);
#endif
        return true;
    }
    uint32_t activity_key=Motor_Lock();
    if(result.type==RX_TYPE_DIAGNOSTIC && UART4Transport_RxGenerationMatches(current_generation) &&
       current_handshake_generation==CommsSession_HandshakeGenerationLocked())
        CommsSession_RecordActivityLocked(ms,epoch);
    Motor_Unlock(activity_key);
    if (result.type == RX_TYPE_DIAGNOSTIC) {
        HWTelemetry_Select(result.data.mode);
        return true;
    }
    bool started = false;
    uint32_t key = Motor_Lock();
    if (UART4Transport_RxGenerationMatches(current_generation) &&
        current_handshake_generation==CommsSession_HandshakeGenerationLocked())
        CommsSession_OnMessage(&result, ms, epoch, &started);
    Motor_Unlock(key);
    if (started) {
        if(N_PULSE_HW_SINGLE_MOTOR_TEST) SendHelloMessage();
    } /* Complete framed message; zero-wait offer. */
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
#ifndef MOTOR_BACKEND_SIM
    service_lifecycle(); /* New-session SYNC precedes any newly received HELLO. */
#endif
    uart4_rx_event_t event;
    for (unsigned n = 0U; n < COMMS_RX_CAPACITY && UART4Transport_PopRx(&event); ++n) {
#ifdef MOTOR_BACKEND_SIM
        if(current_generation!=event.generation) SimParser_Reset(&parser);
        current_generation=event.generation;
        SimParser_Feed(&parser,event.byte,event.ms,event.epoch,frame_received,NULL);
#else
        if(sync_handshake_generation()) service_lifecycle();
        if(event.handshake_generation!=current_handshake_generation) continue;
        if(current_generation!=event.generation) CommsParser_Reset(&parser);
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
