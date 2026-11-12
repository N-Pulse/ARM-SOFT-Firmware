#include "comms_session.h"
#include "comms_config.h"
#include "motor_safety.h"
#include "motor_lock.h"
#include "hw_test_config.h"
#include "handshake_config.h"
#include "intent_config.h"
#include <string.h>

static bool enabled, session, lost, link_loss_pending, have_intent;
static uint32_t fresh_ms, peer_id, intent_started_ms, handshake_generation;
static intent_id_t current_intent;
static lifecycle_status_t status;
static comms_diagnostics_t diag;
static comms_peer_status_t peer_status;
static uint32_t heartbeat_ms(void) {
    return N_PULSE_HW_SINGLE_MOTOR_TEST ? COMMS_LINK_TIMEOUT_MS : HANDSHAKE_HB_PERIOD_MS;
}
app_state_t CommsSession_StateLocked(void) { return (app_state_t)status.state; }
void CommsSession_StateLockedSet(app_state_t s) {
    if(status.state!=(uint32_t)s) { status.state=s; ++status.revision; }
}
static void report_locked(app_result_t r,uint32_t id,uint32_t detail) {
    status.result=r; status.request_id=id; status.detail=detail; ++status.revision;
}
void CommsSession_Report(app_result_t r,uint32_t id,uint32_t detail) {
    uint32_t k=Motor_Lock(); report_locked(r,id,detail); Motor_Unlock(k);
}
void CommsSession_SetState(app_state_t s) {
    uint32_t k=Motor_Lock(); CommsSession_StateLockedSet(s); Motor_Unlock(k);
}
app_state_t CommsSession_GetState(void) {
    uint32_t k=Motor_Lock(); app_state_t s=CommsSession_StateLocked(); Motor_Unlock(k); return s;
}
void CommsSession_Init(bool hw) {
    uint32_t k=Motor_Lock(); enabled=hw;
    session=lost=link_loss_pending=have_intent=false;
    fresh_ms=peer_id=intent_started_ms=handshake_generation=0;
    current_intent=ACTION_UNKNOWN;
    memset(&diag,0,sizeof(diag)); memset(&status,0,sizeof(status));
    memset(&peer_status,0,sizeof(peer_status));
    status.state=APP_INIT_HANDSHAKE; status.revision=1; Motor_Unlock(k);
}
void CommsSession_OnStopLocked(void) {
    have_intent=false;
}
uint32_t CommsSession_HandshakeGenerationLocked(void) { return handshake_generation; }
static bool fresh(uint32_t now) {
    return enabled && session && !lost && (uint32_t)(now-fresh_ms)<heartbeat_ms();
}
bool CommsSession_IsFreshLocked(uint32_t now) { return fresh(now); }
bool CommsSession_HasSession(void) {
    uint32_t k=Motor_Lock(); bool result=fresh(HAL_GetTick()); Motor_Unlock(k); return result;
}
bool CommsSession_RecordActivityLocked(uint32_t ms,uint32_t epoch) {
    uint32_t now=HAL_GetTick();
    if(!enabled || !session || lost || epoch!=MotorSafety_EpochLocked() ||
       (uint32_t)(now-ms)>=heartbeat_ms() || (uint32_t)(ms-fresh_ms)>=heartbeat_ms()) return false;
    fresh_ms=ms; return true;
}
bool CommsSession_RecordPeerStatusLocked(const uint8_t *bytes,size_t length,uint8_t sequence,
    uint32_t ms,uint32_t epoch,uint32_t generation) {
    if(!bytes || !length || length>sizeof(peer_status.bytes) ||
       generation!=handshake_generation || !CommsSession_RecordActivityLocked(ms,epoch)) return false;
    memcpy(peer_status.bytes,bytes,length);
    peer_status.state=bytes[0]; peer_status.sequence=sequence;
    peer_status.length=(uint8_t)length; peer_status.timestamp_ms=ms; peer_status.valid=true;
    return true;
}
void CommsSession_GetPeerStatus(comms_peer_status_t *out) {
    if(!out) return;
    uint32_t key=Motor_Lock(); *out=peer_status;
    out->valid=out->valid && fresh(HAL_GetTick()) &&
        (uint32_t)(HAL_GetTick()-out->timestamp_ms)<heartbeat_ms();
    Motor_Unlock(key);
}
bool CommsSession_CanReceiveHelloLocked(void) {
    if(!enabled || N_PULSE_HW_SINGLE_MOTOR_TEST) return false;
    return MotorSafety_CanArmLocked() &&
        (status.state==APP_INIT_HANDSHAKE || fresh(HAL_GetTick()));
}
void CommsSession_HandshakeReplySentLocked(uint32_t epoch,uint32_t generation,uint32_t ms,bool matches) {
    if(!CommsSession_CanReceiveHelloLocked() || generation!=handshake_generation ||
       epoch!=MotorSafety_EpochLocked() || (session && !lost)) return;
    CommsSession_OnStopLocked(); session=true; lost=false; fresh_ms=ms;
    /* End the handshake RX/TX generation as well: DATA buffered before the
     * reply completed must not become executable merely because WORK starts. */
    ++handshake_generation;
    peer_status.valid=false;
    ++diag.hello; ++diag.accepted;
    report_locked(matches ? APP_RESULT_OK : APP_RESULT_BAD_VERSION,0,0);
    if(status.state==APP_INIT_HANDSHAKE) CommsSession_StateLockedSet(APP_WORK);
}
void CommsSession_PollLocked(uint32_t now) {
    if(!enabled || !session || lost || fresh(now) || !MotorSafety_CanArmLocked()) return;
    lost=true; have_intent=false; ++diag.link_timeouts;
    peer_status.valid=false;
    report_locked(APP_RESULT_LINK_TIMEOUT,0,0);
    if(N_PULSE_HW_SINGLE_MOTOR_TEST) {
        MotorSafety_RequestStopCauseLocked(MOTOR_STOP_LINK,MOTOR_COUNT);
    } else {
        /* Reconnect only: retain homing and motor targets. Old RX/TX cannot
         * establish the new session or execute after its HELLO. */
        session=false; ++handshake_generation;
        CommsSession_StateLockedSet(APP_INIT_HANDSHAKE);
        link_loss_pending=true;
    }
}
bool CommsSession_TakeLinkLoss(void) {
    uint32_t k=Motor_Lock(); bool pending=link_loss_pending;
    link_loss_pending=false; Motor_Unlock(k); return pending;
}
void CommsSession_TransportErrorLocked(void) {
    ++diag.transport_errors;
    if(enabled) {
        lost=true; report_locked(APP_RESULT_TRANSPORT,0,0);
        peer_status.valid=false;
        MotorSafety_RequestStopCauseLocked(MOTOR_STOP_LINK,MOTOR_COUNT);
    }
    CommsSession_OnStopLocked();
}
bool CommsSession_CanRecoverLocked(uint32_t now) {
    return !enabled || (N_PULSE_HW_SINGLE_MOTOR_TEST && !session) || fresh(now);
}
bool CommsSession_OnMessage(const rx_result_t *msg,uint32_t ms,uint32_t epoch,bool *started) {
    if(started) *started=false;
    uint32_t k=Motor_Lock(); ++diag.received;
    if(!enabled || !msg || msg->type==RX_TYPE_NONE) { ++diag.rejected; Motor_Unlock(k); return false; }
    if(msg->type==RX_TYPE_CONTROL || msg->type==RX_TYPE_STOP || msg->type==RX_TYPE_COMMISSION) {
        ++diag.rejected; Motor_Unlock(k); return false;
    }
    ++diag.valid;
    if((epoch!=MotorSafety_EpochLocked()) || (uint32_t)(HAL_GetTick()-ms)>=heartbeat_ms()) {
        ++diag.stale; Motor_Unlock(k); return false;
    }
    if(msg->type==RX_TYPE_HELLO && N_PULSE_HW_SINGLE_MOTOR_TEST) {
        if(msg->proto_version!=1 || !msg->data.device_id || (peer_id && peer_id!=msg->data.device_id)) {
            ++diag.rejected; Motor_Unlock(k); return false;
        }
        bool begin=!session || lost;
        session=true; lost=false; peer_id=msg->data.device_id; fresh_ms=ms;
        ++diag.hello; ++diag.accepted; if(started) *started=begin;
        Motor_Unlock(k); return true;
    }
    /* Valid repeated and protected-period intents still count as link activity. */
    CommsSession_RecordActivityLocked(ms,epoch);
    CommsSession_PollLocked(HAL_GetTick());
    bool result=false;
    if(!N_PULSE_HW_SINGLE_MOTOR_TEST && msg->type==RX_TYPE_ACTION &&
            msg->data.action_id<ACTION_COUNT && fresh(HAL_GetTick()) &&
            MotorSafety_CanArmLocked() && status.state==APP_WORK) {
        intent_id_t id=(intent_id_t)msg->data.action_id;
        if(have_intent && id==current_intent) {
            ++diag.repeated; ++diag.accepted; result=true;
        } else if(have_intent && ((int32_t)(ms-intent_started_ms)<0 ||
                   (uint32_t)(ms-intent_started_ms)<INTENT_MIN_RUN_MS)) {
            ++diag.guard_dropped; ++diag.discarded;
        } else {
            intent_t intent={id,epoch};
            if(!IntentRouter_Handle(&intent)) {
                ++diag.rejected; Motor_Unlock(k); return false;
            }
            have_intent=true; current_intent=id; intent_started_ms=HAL_GetTick();
            ++diag.applied; ++diag.accepted; ++status.action_sequence; ++status.revision;
            result=true;
        }
    } else { ++diag.rejected; report_locked(APP_RESULT_WRONG_STATE,0,msg->type); }
    Motor_Unlock(k); return result;
}
void CommsSession_GetStatus(lifecycle_status_t *out) {
    if(!out) return;
    uint32_t k=Motor_Lock(); *out=status;
    out->queue_depth=out->dropped=out->action_done=0;
    out->causes=MotorSafety_GetStopCauses(); Motor_Unlock(k);
}
void CommsSession_GetDiagnostics(comms_diagnostics_t *out) {
    if(!out) return;
    uint32_t k=Motor_Lock(); *out=diag; Motor_Unlock(k);
}
