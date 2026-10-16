#if N_PULSE_DUAL_BOARD_LINK && !defined(MOTOR_BACKEND_SIM)
#include "board_link.h"
#include "main.h"
#include "motor_lock.h"
#include "motor_safety.h"
#include "motor_backend.h"
#include "motor_backend_state.h"
#include "hw_test_config.h"
#include "l298n_config.h"
#include "hw_commission.h"
#include "comms_session.h"
#include "encoder_bsp.h"
#include "drv8848.h"
#include "FreeRTOS.h"
#include "task.h"
#include "comms_config.h"
#include "board_commission.h"
#include "hw_telemetry.h"
#include <math.h>
#if COMMS_UART_IRQ_PRIORITY < configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY
#error USART3 priority must be below the FreeRTOS syscall ceiling
#endif
_Static_assert(BOARD_ROLE_MASTER==BL_MASTER && BOARD_ROLE_SLAVE==BL_SLAVE,"Wire role mapping changed");

extern UART_HandleTypeDef huart3;
static board_protocol_t link;
typedef struct { uint8_t byte; uint32_t ms; } rx_item_t;
static rx_item_t rx[256];
static unsigned head,tail;
static uint8_t rx_byte,tx[BL_WIRE_MAX];
static bool started,tx_active,quarantined;
static volatile bool recover_rx,tx_complete;
static uint32_t tx_ms,feedback_ms,last_request;
static uint32_t rx_overflow_count,transport_errors;
static uint8_t last_transport_cause,last_uart_error;
enum { TRANSPORT_RX_ERROR=1,TRANSPORT_RX_REARM,TRANSPORT_RX_OVERFLOW,
       TRANSPORT_TX_START,TRANSPORT_TX_TIMEOUT,TRANSPORT_RX_ABORT,TRANSPORT_ENTROPY };
static uint8_t feedback_motor;
static StaticTask_t task_control;
static StackType_t task_stack[768];
static bool relay_active,commission_pending;
static uint32_t relay_ms,commission_ms,commission_request,commission_epoch;
static uint64_t commission_peer;
static bool selection_wait;
static hw_command_t selection_command;

static void stop_hook(void *context)
{ (void)context; MotorSafety_RequestStopCauseLocked(MOTOR_STOP_LINK,MOTOR_COUNT); }
static bool local_target(motor_id_t id,float target,uint8_t speed,uint32_t epoch)
{
    if(!BoardLink_IsLocalMotor(id) || !isfinite(target) || !speed || speed>100 ||
       !Motor_IsEnabled(id) || (!HW_DIAGNOSTIC_TIMED && (!Motor_HasFreshPositionFeedback(id) ||
       !MotorBackend_FeedbackReady(id))) || !Motor_OutputAllowedLocked(id)) return false;
    float filtered=target; uint8_t filtered_speed=speed;
    if(!MotorSafety_FilterCommand(id,&filtered,&filtered_speed) || filtered!=target || filtered_speed!=speed) return false;
    return Motor_SetTargetAtEpoch(id,target,speed,epoch);
}
static uint8_t target_hook(void *context,uint8_t id,int32_t target,uint8_t speed,uint32_t epoch)
{
    (void)context;
    /* Local single-motor commissioning has exclusive command authority. */
    if(N_PULSE_HW_SINGLE_MOTOR_TEST) return BL_FORBIDDEN;
    return local_target((motor_id_t)id,(float)target/1000.0f,speed,epoch) ? BL_ACCEPTED : BL_FORBIDDEN;
}
void BoardLink_RuntimeInit(void)
{
    head=tail=0; started=recover_rx=tx_active=tx_complete=quarantined=false;
    feedback_ms=last_request=0; feedback_motor=0;
    rx_overflow_count=transport_errors=0;
    last_transport_cause=last_uart_error=0;
    relay_active=commission_pending=selection_wait=false; relay_ms=0;
    /* A zero nonce is deliberately unusable until the worker obtains entropy. */
    BL_Init(&link,(uint8_t)BoardLink_Role(),0,HAL_GetTick(),(bl_hooks_t){stop_hook,target_hook,NULL});
}
bool BoardLink_HostAuthority(void) { return BoardLink_Role()==BOARD_ROLE_MASTER; }
void BoardLink_SafetyPollLocked(uint32_t now)
{
    if(!started) return;
    BL_SetLocalState(&link,MotorSafety_EpochLocked(),MotorSafety_GetStopCauses(),now);
    BL_Poll(&link,now);
}
bool BoardLink_CanRecoverLocked(uint32_t now)
{ return started && BL_Fresh(&link,now); }
bool BoardLink_ActuationAllowedLocked(uint32_t now)
{ return BoardLink_CanRecoverLocked(now) && !link.peer_faults; }
bool BoardLink_RouteTarget(motor_id_t id,float target,uint8_t speed,uint32_t epoch)
{
    uint32_t key=Motor_Lock(),now=HAL_GetTick();
    BoardLink_SafetyPollLocked(now);
    bool ok=BoardLink_HostAuthority() && !N_PULSE_HW_SINGLE_MOTOR_TEST &&
        epoch==MotorSafety_EpochLocked() && BL_Ready(&link,now) && isfinite(target) &&
        target>=-3.141593f && target<=3.141593f;
    if(ok) ok=BoardLink_IsLocalMotor(id) ? local_target(id,target,speed,epoch) :
        BL_Target(&link,(uint8_t)id,(int32_t)lroundf(target*1000.0f),speed,epoch,now);
    if(!ok) { ++g_old_hw_rejected_commands; MotorSafety_RequestStopCauseLocked(MOTOR_STOP_LINK,MOTOR_COUNT); }
    Motor_Unlock(key); return ok;
}
bool BoardLink_GetRemote(motor_id_t id,bl_feedback_t *out)
{
    uint32_t key=Motor_Lock(); bool ok=BL_GetFeedback(&link,(uint8_t)id,HAL_GetTick(),out); Motor_Unlock(key); return ok;
}
void BoardLink_GetDiagnostic(link_diagnostic_t *out)
{
    if(!out) return;
    uint32_t key=Motor_Lock(),now=HAL_GetTick();
    *out=(link_diagnostic_t){.timestamp_ms=now,.role=(uint32_t)BoardLink_Role(),
        .local_session=link.session,.remote_session=link.peer,.local_epoch=MotorSafety_EpochLocked(),
        .remote_epoch=link.peer_epoch,.local_stops=MotorSafety_GetStopCauses(),.remote_stops=link.peer_faults,
        .heartbeat_age_ms=link.have_heartbeat?now-link.rx_ms:UINT32_MAX,
        .echo_age_ms=link.have_heartbeat?now-link.ack_ms:UINT32_MAX,
        .flags=(BL_Fresh(&link,now)?1U:0U)|(link.established?2U:0U)|(link.authorized?4U:0U)|
               (link.peer_authorized?8U:0U)|(N_PULSE_OLD_HW_MOTION_OPT_IN?16U:0U)|
               (quarantined?32U:0U)|(link.have_peer_epoch?64U:0U)|(started?128U:0U)|
               ((uint32_t)last_uart_error<<8)|((uint32_t)last_transport_cause<<16),
        .rx_sequence=link.rx_seq,.tx_sequence=link.tx_seq,.malformed=link.malformed,
        .overflows=link.overflows>UINT32_MAX-rx_overflow_count ? UINT32_MAX : link.overflows+rx_overflow_count,
        .restarts=link.restarts,.rejected=link.rejected,.transport_errors=transport_errors};
    Motor_Unlock(key);
}
uint32_t BoardLink_Operator(uint32_t op,uint32_t request,uint32_t epoch,uint32_t ms)
{
    uint32_t key=Motor_Lock(),now=HAL_GetTick(); BoardLink_SafetyPollLocked(now);
    uint32_t result=HW_OK;
    if(!CommsSession_IsFreshLocked(now)) result=HW_SESSION;
    else if(epoch!=MotorSafety_EpochLocked() || now-ms>250U || !request || request<=last_request) result=HW_STALE;
    else if(!BL_Fresh(&link,now)) result=HW_SESSION;
    else if(MotorBackend_IsArmed() || MotorBackend_IsTransitioning()) result=HW_DISABLE_FIRST;
    if(result==HW_OK) {
        for(unsigned i=0;i<MOTOR_COUNT;++i) if(Motor_IsEnabled((motor_id_t)i)) result=HW_DISABLE_FIRST;
    }
    if(result!=HW_OK) { Motor_Unlock(key); return result; }
    last_request=request;
    if(op==12) result=BL_Authorize(&link,now) ? HW_OK : HW_FAULT;
    else if(op==13) {
        motor_recovery_ticket_t ticket=Motor_RecoveryTicketLocked();
        Motor_Unlock(key); return Motor_RecoverFromFaultWithTicket(ticket)?HW_OK:HW_FAULT;
    }
    else result=HW_PARAMETERS;
    Motor_Unlock(key); return result;
}
/* Setup/recovery must also traverse USART3. They use a fresh link, while
 * HWCommission retains calibration, single-motor, preflight and arm checks. */
uint32_t BoardLink_RouteCommission(const hw_command_t *c,uint32_t ms,uint32_t epoch)
{
    if(!c) return HW_PARAMETERS;
    uint32_t key=Motor_Lock(),now=HAL_GetTick();
    BoardLink_SafetyPollLocked(now);
    bool valid=BoardLink_HostAuthority() && CommsSession_IsFreshLocked(now) &&
        epoch==MotorSafety_EpochLocked() && c->epoch==epoch && now-ms<=250U &&
        c->request_id && !commission_pending;
    Motor_Unlock(key);
    if(!valid) return HW_STALE;
    if(N_PULSE_HW_SELECTABLE_FINGER && c->operation!=HW_SELECT_FINGER &&
       c->operation!=12 && c->operation!=13 && c->motor_id!=(uint32_t)HWCommission_SelectedMotor()) return HW_WRONG_MOTOR;
    if(N_PULSE_HW_SELECTABLE_FINGER && c->operation==HW_SELECT_FINGER) {
        uint32_t result=HWCommission_Execute(c,ms,epoch);
        if(result!=HW_OK) return result;
        /* First let the distributed STOP settle. Queuing SELECT immediately
         * would bind it to the peer's pre-STOP epoch and correctly discard it. */
        key=Motor_Lock();
        selection_command=*c; selection_wait=relay_active=commission_pending=true;
        commission_request=c->request_id; commission_epoch=MotorSafety_EpochLocked();
        commission_ms=HAL_GetTick(); commission_peer=link.peer;
        Motor_Unlock(key); return BOARD_LINK_PENDING;
    } else if(c->operation==12 || c->operation==13) {
        uint32_t result=BoardLink_Operator(c->operation,c->request_id,epoch,ms);
        if(result!=HW_OK) return result;
        /* Recovery advanced the master's epoch. Nothing queued at the old
         * epoch may survive; the peer is still addressed at its own epoch. */
        epoch=MotorSafety_EpochLocked();
    } else if(BoardLink_IsLocalMotor((motor_id_t)c->motor_id)) {
        return HWCommission_Execute(c,ms,epoch);
    }
    uint8_t bytes[BL_PAYLOAD]; uint8_t n=BoardCommission_Encode(c,bytes);
    if(!n) return HW_PARAMETERS;
    key=Motor_Lock(); now=HAL_GetTick(); BoardLink_SafetyPollLocked(now);
    if(epoch!=MotorSafety_EpochLocked() || !BL_Fresh(&link,now)) { Motor_Unlock(key); return HW_SESSION; }
    /* No peer console is needed: forward the master's *live* host lease.
     * HELLO only renews communications; it never recovers, enables or arms. */
    const uint8_t hello[]={BC_HELLO};
    bool ok=BL_OfferApplication(&link,hello,sizeof hello,now) && BL_OfferApplication(&link,bytes,n,now);
    if(ok) {
        relay_active=commission_pending=true; relay_ms=commission_ms=now;
        commission_request=c->request_id; commission_epoch=epoch; commission_peer=link.peer;
    }
    Motor_Unlock(key); return ok?BOARD_LINK_PENDING:HW_SESSION;
}
static void application(void)
{
    bl_message_t m; uint32_t key=Motor_Lock();
    bool have=BL_TakeApplication(&link,&m); Motor_Unlock(key);
    if(!have) return;
    if(BoardLink_Role()==BOARD_ROLE_SLAVE && m.length==1 && m.data[0]==BC_HELLO) {
        rx_result_t hello={.type=RX_TYPE_HELLO,.proto_version=1,.data.device_id=1};
        CommsSession_OnMessage(&hello,m.offered_ms,m.epoch,NULL);
    } else if(BoardLink_Role()==BOARD_ROLE_SLAVE && m.data[0]==BC_COMMAND) {
        hw_command_t c; uint32_t result=HW_PARAMETERS,request=m.length>=5?BL_Get32(m.data+1):0;
        if(BoardCommission_Decode(m.data,m.length,&c)) {
            c.epoch=m.epoch;
            /* Called with interrupts enabled: arming is forbidden under the parser lock. */
            result=(c.operation==12 || c.operation==13) ?
                BoardLink_Operator(c.operation,c.request_id,m.epoch,m.offered_ms) :
                HWCommission_Execute(&c,m.offered_ms,m.epoch);
        }
        uint8_t reply[9]={BC_REPLY}; BL_Put32(reply+1,request); BL_Put32(reply+5,result);
        key=Motor_Lock(); BoardLink_SafetyPollLocked(HAL_GetTick());
        if(!BL_OfferApplication(&link,reply,sizeof reply,HAL_GetTick())) BL_Fail(&link,HAL_GetTick());
        Motor_Unlock(key);
    } else if(BoardLink_Role()==BOARD_ROLE_MASTER && m.length==9 && m.data[0]==BC_REPLY) {
        key=Motor_Lock();
        uint32_t result=BL_Get32(m.data+5);
        bool matched=commission_pending && BL_Get32(m.data+1)==commission_request &&
            m.epoch==commission_epoch && MotorSafety_EpochLocked()==commission_epoch &&
            m.peer==commission_peer && result<=HW_REJECTED;
        uint32_t request=commission_request;
        if(matched) commission_pending=false;
        Motor_Unlock(key);
        if(matched) HWTelemetry_Reply(request,result);
    } else {
        key=Motor_Lock(); BL_Fail(&link,HAL_GetTick()); Motor_Unlock(key);
    }
}
static void transport_fault(bool overflow,uint8_t cause)
{
    uint32_t key=Motor_Lock(); recover_rx=true; tail=head;
    last_transport_cause=cause;
    if(cause==TRANSPORT_RX_ERROR) last_uart_error=(uint8_t)huart3.ErrorCode;
    /* Quiesce this UART until the worker aborts/clears/rearms RX. Nonblocking
     * HAL errors can otherwise keep interrupting BUSY_RX before the worker
     * gets CPU time, flooding the error count and starving recovery. */
    HAL_NVIC_DisableIRQ(USART3_IRQn);
    if(overflow && rx_overflow_count!=UINT32_MAX) ++rx_overflow_count;
    if(transport_errors!=UINT32_MAX) ++transport_errors;
    BL_Fail(&link,HAL_GetTick()); Motor_Unlock(key);
}
bool BoardLink_RxCallback(void *uart)
{
    if(uart!=&huart3) return false;
    if(!started || recover_rx) return true;
    uint32_t key=Motor_Lock(); unsigned next=(head+1U)%256U;
    bool full=next==tail;
    if(!full) { rx[head]=(rx_item_t){rx_byte,HAL_GetTick()}; head=next; }
    Motor_Unlock(key);
    if(full) transport_fault(true,TRANSPORT_RX_OVERFLOW);
    else if(HAL_UART_Receive_IT(&huart3,&rx_byte,1)!=HAL_OK) transport_fault(false,TRANSPORT_RX_REARM);
    return true;
}
bool BoardLink_TxCallback(void *uart)
{
    if(uart!=&huart3) return false;
    if(tx_active && huart3.gState==HAL_UART_STATE_READY && !huart3.TxXferCount) tx_complete=true;
    return true;
}
bool BoardLink_ErrorCallback(void *uart)
{
    if(uart!=&huart3) return false;
    transport_fault(false,TRANSPORT_RX_ERROR); return true; /* No HAL abort/polling inside this IRQ. */
}
/* Nonblocking RNG startup is confined to this lower-priority worker. No HAL
 * handles or RNG interrupt are needed. CMSIS/RCC define HSI48 as CLK48SEL=0.
 * Fail closed on oscillator/clock/seed errors, zero/repeated entropy or timeout. */
static bool nonce(uint64_t *out)
{
    RCC->CRRCR|=RCC_CRRCR_HSI48ON;
    uint32_t deadline=HAL_GetTick();
    while(!(RCC->CRRCR&RCC_CRRCR_HSI48RDY)) {
        if((uint32_t)(HAL_GetTick()-deadline)>50U) return false;
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    RCC->CCIPR&=~RCC_CCIPR_CLK48SEL;
    __HAL_RCC_RNG_CLK_ENABLE(); __HAL_RCC_RNG_FORCE_RESET(); __HAL_RCC_RNG_RELEASE_RESET();
    RNG->CR=RNG_CR_RNGEN; uint32_t words[3];
    for(unsigned i=0;i<3;++i) {
        deadline=HAL_GetTick();
        for(;;) {
            uint32_t status=RNG->SR;
            if(status&(RNG_SR_CECS|RNG_SR_SECS|RNG_SR_CEIS|RNG_SR_SEIS)) { RNG->CR=0; return false; }
            if(status&RNG_SR_DRDY) { words[i]=RNG->DR; break; }
            if((uint32_t)(HAL_GetTick()-deadline)>50U) { RNG->CR=0; return false; }
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
    uint32_t final_status=RNG->SR; RNG->CR=0;
    if(final_status&(RNG_SR_CECS|RNG_SR_SECS|RNG_SR_CEIS|RNG_SR_SEIS)) return false;
    if(!words[1] || !words[2] || words[0]==words[1] || words[1]==words[2]) return false;
    *out=words[1]|(uint64_t)words[2]<<32; return *out!=UINT64_MAX;
}
static int32_t fixed(float v)
{ return isfinite(v) && v>=-2147483.0f && v<=2147483.0f ? (int32_t)lroundf(v*1000.0f) : 0; }
static void feedback(uint32_t now)
{
    motor_id_t active=HWCommission_SelectedMotor();
    bool focused=N_PULSE_HW_SELECTABLE_FINGER && (unsigned)active<5U && BoardLink_IsLocalMotor(active);
    if((uint32_t)(now-feedback_ms)<(focused?20U:10U)) return;
    feedback_ms=now;
    motor_id_t id=focused?active:(motor_id_t)feedback_motor;
    if(!focused) feedback_motor=(feedback_motor+1U)%MOTOR_COUNT;
    if(!BoardLink_IsLocalMotor(id)) return;
    motor_state_t m; encoder_snapshot_t e={0}; Motor_GetSnapshot(id,&m);
    bool valid=Encoder_BSP_GetSnapshot(id,&e) && m.feedback_valid && now-m.feedback_ms<=100U;
    bool raw_valid=Encoder_BSP_RawAvailable(id);
    bl_feedback_t f={.id=(uint8_t)id,.flags=(valid?BL_VALID:0) |
        (raw_valid?BL_RAW_VALID:0) |
        (Encoder_BSP_IsCalibrated(id)?BL_CALIBRATED:0) | (m.enabled?BL_ENABLED:0) |
        (MotorBackend_IsArmed()?BL_ARMED:0) | (HWCommission_ConfiguredFor(id)?BL_CONFIGURED:0) |
        (id==HWCommission_SelectedMotor() && HWCommission_OutputPreflightReady()?BL_PREFLIGHT:0) |
        (id==MOTOR_THUMB && !HWTest_ThumbMotionAllowed()?BL_THUMB_BLOCKED:0),.age_ms=raw_valid?0:UINT32_MAX,
        .position_mrad=fixed(e.position_rad),.velocity_mrad_s=fixed(e.velocity_rad_s),
        .target_mrad=fixed(m.target_position),.pwm_milli=fixed(Motor_L298N_AppliedPWM(id)),
        .raw_count=e.raw_count,.faults=MotorSafety_GetStopCauses(),.reference_mrad=fixed(m.reference_position)};
    BL_OfferFeedback(&link,&f,now);
}
static void service(void)
{
#if N_PULSE_TIMING_DIAGNOSTICS
    uint32_t timing=OldHW_TimingBegin(OLD_TIME_BOARD);
#endif
    uint32_t key;
    if(recover_rx) {
        HAL_NVIC_DisableIRQ(USART3_IRQn);
        if(HAL_UART_AbortReceive(&huart3)!=HAL_OK) { quarantined=true; last_transport_cause=TRANSPORT_RX_ABORT; }
        __HAL_UART_CLEAR_FLAG(&huart3,UART_CLEAR_OREF|UART_CLEAR_NEF|UART_CLEAR_FEF|UART_CLEAR_PEF);
        __HAL_UART_SEND_REQ(&huart3,UART_RXDATA_FLUSH_REQUEST);
        key=Motor_Lock(); tail=head; link.encoded_length=0; recover_rx=false; Motor_Unlock(key);
        if(!quarantined && HAL_UART_Receive_IT(&huart3,&rx_byte,1)!=HAL_OK) transport_fault(false,TRANSPORT_RX_REARM);
        HAL_NVIC_ClearPendingIRQ(USART3_IRQn);
        if(!quarantined && !recover_rx) HAL_NVIC_EnableIRQ(USART3_IRQn);
    }
    for(unsigned i=0;i<256;++i) {
        key=Motor_Lock();
        if(recover_rx || tail==head) { Motor_Unlock(key); break; }
        rx_item_t item=rx[tail]; tail=(tail+1U)%256U;
        BoardLink_SafetyPollLocked(HAL_GetTick());
        BL_Feed(&link,item.byte,item.ms,HAL_GetTick()); Motor_Unlock(key);
        application();
    }
    key=Motor_Lock(); uint32_t now=HAL_GetTick(); BoardLink_SafetyPollLocked(now);
    if(selection_wait && commission_pending && BL_Fresh(&link,now) && link.peer_faults &&
       (MotorSafety_GetStopCauses()&MOTOR_STOP_LINK)) {
        uint8_t bytes[BL_PAYLOAD]; uint8_t n=BoardCommission_Encode(&selection_command,bytes);
        const uint8_t hello[]={BC_HELLO};
        if(n && BL_OfferApplication(&link,hello,sizeof hello,now) && BL_OfferApplication(&link,bytes,n,now)) {
            selection_wait=false; relay_ms=commission_ms=now; commission_epoch=MotorSafety_EpochLocked();
        }
    }
    if(relay_active && BoardLink_HostAuthority() && CommsSession_IsFreshLocked(now) &&
       BL_Fresh(&link,now) && now-relay_ms>=200U) {
        const uint8_t hello[]={BC_HELLO};
        if(BL_OfferApplication(&link,hello,sizeof hello,now)) relay_ms=now;
    }
    bool expired=commission_pending && (now-commission_ms>250U ||
        (!selection_wait && commission_epoch!=MotorSafety_EpochLocked()) || commission_peer!=link.peer || !BL_Fresh(&link,now));
    uint32_t expired_request=commission_request;
    if(expired) { commission_pending=selection_wait=false; BL_Fail(&link,now); }
    feedback(now);
    bool timed_out=tx_active && !tx_complete && (uint32_t)(now-tx_ms)>15U;
    if(tx_complete) { tx_active=false; tx_complete=false; }
    if(timed_out) { last_transport_cause=TRANSPORT_TX_TIMEOUT; if(transport_errors!=UINT32_MAX) ++transport_errors; BL_Fail(&link,now); tx_active=false; }
    Motor_Unlock(key);
    if(expired) HWTelemetry_Reply(expired_request,HW_STALE);
    if(timed_out && HAL_UART_AbortTransmit(&huart3)!=HAL_OK) quarantined=true;
    key=Motor_Lock();
    if(!quarantined && !tx_active) {
        size_t size=BL_Take(&link,HAL_GetTick(),tx);
        if(size) {
            /* Same bounded HAL IT commitment audited for LPUART1. */
            tx_complete=false; tx_active=HAL_UART_Transmit_IT(&huart3,tx,(uint16_t)size)==HAL_OK;
            tx_ms=HAL_GetTick(); if(!tx_active) {
                if(transport_errors!=UINT32_MAX) ++transport_errors;
                last_transport_cause=TRANSPORT_TX_START;
                quarantined=true; BL_Fail(&link,tx_ms);
            }
        }
    }
    Motor_Unlock(key);
#if N_PULSE_TIMING_DIAGNOSTICS
    OldHW_TimingEnd(OLD_TIME_BOARD,timing);
#endif
}
static void worker(void *unused)
{
    (void)unused; uint64_t session;
    if(!nonce(&session)) {
        uint32_t key=Motor_Lock(); last_transport_cause=TRANSPORT_ENTROPY; if(transport_errors!=UINT32_MAX) ++transport_errors; Motor_Unlock(key);
        MotorSafety_RequestStopCause(MOTOR_STOP_LINK,MOTOR_COUNT); vTaskDelete(NULL); return;
    }
    uint32_t key=Motor_Lock();
    BL_Init(&link,(uint8_t)BoardLink_Role(),session,HAL_GetTick(),(bl_hooks_t){stop_hook,target_hook,NULL});
    started=true; BoardLink_SafetyPollLocked(HAL_GetTick()); Motor_Unlock(key);
    HAL_NVIC_ClearPendingIRQ(USART3_IRQn); HAL_NVIC_EnableIRQ(USART3_IRQn);
    if(HAL_UART_Receive_IT(&huart3,&rx_byte,1)!=HAL_OK) transport_fault(false,TRANSPORT_RX_REARM);
    for(;;) {
        service();
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}
void BoardLink_StartTask(void)
{
    TaskHandle_t task=xTaskCreateStatic(worker,"boardlink",768,NULL,tskIDLE_PRIORITY+2,
                                       task_stack,&task_control);
    configASSERT(task!=NULL);
}
#endif
