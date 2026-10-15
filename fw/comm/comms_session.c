#include "comms_session.h"
#include "comms_config.h"
#include "motor_safety.h"
#include "motor_lock.h"
#include <string.h>

static intent_t intents[COMMS_INTENT_CAPACITY];
static unsigned head, count;
static bool enabled, session, lost, probe_pending, probe_offer, control_pending;
static uint32_t fresh_ms, peer_id, boot_ms, probe_ms, probe_id, last_probe_ms;
static uint32_t control_high_water, control_epoch;
static lifecycle_control_t pending_control;
static lifecycle_status_t status;
static comms_diagnostics_t diag;
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
    uint32_t k=Motor_Lock();
    head=count=0; enabled=hw; session=lost=probe_pending=probe_offer=control_pending=false;
    fresh_ms=peer_id=probe_ms=probe_id=last_probe_ms=control_high_water=control_epoch=0;
    boot_ms=HAL_GetTick(); memset(&diag,0,sizeof(diag)); memset(&status,0,sizeof(status));
    status.state=APP_INIT_HANDSHAKE; status.revision=1;
    Motor_Unlock(k);
}
void CommsSession_OnStopLocked(void) {
    diag.discarded+=count; head=count=0; control_pending=false;
}
void CommsSession_BeginRestartLocked(void) {
    CommsSession_OnStopLocked(); session=lost=probe_pending=probe_offer=false;
    boot_ms=HAL_GetTick(); fresh_ms=boot_ms;
    status.result=APP_RESULT_OK; status.action_done=0; ++status.revision;
}
static bool fresh(uint32_t now) {
    return enabled && session && !lost && (uint32_t)(now-fresh_ms)<=COMMS_LINK_TIMEOUT_MS;
}
bool CommsSession_HasSession(void) {
    uint32_t k=Motor_Lock(); bool result=fresh(HAL_GetTick()); Motor_Unlock(k); return result;
}
void CommsSession_PollLocked(uint32_t now) {
    if(!enabled || lost) return;
    bool timeout=(!session && (uint32_t)(now-boot_ms)>COMMS_INITIAL_HANDSHAKE_MS) ||
        (session && (!fresh(now) || (probe_pending && (uint32_t)(now-probe_ms)>COMMS_LINK_TIMEOUT_MS)));
    if(timeout) {
        lost=true; ++diag.link_timeouts; report_locked(APP_RESULT_LINK_TIMEOUT,0,probe_id);
        MotorSafety_RequestStopCauseLocked(MOTOR_STOP_LINK,MOTOR_COUNT);
    }
}
void CommsSession_TransportErrorLocked(void) {
    ++diag.transport_errors;
    if(enabled) {
        lost=true; report_locked(APP_RESULT_TRANSPORT,0,0);
        MotorSafety_RequestStopCauseLocked(MOTOR_STOP_LINK,MOTOR_COUNT);
    }
    CommsSession_OnStopLocked();
}
bool CommsSession_CanRecoverLocked(uint32_t now) { return !enabled || fresh(now); }
static bool on_control(const lifecycle_control_t *c,uint32_t ms,uint32_t epoch) {
    app_state_t s=CommsSession_StateLocked();
    if(!fresh(HAL_GetTick()) || c->peer_id!=peer_id) {
        ++diag.rejected; report_locked(APP_RESULT_BAD_SESSION,c->request_id,0); return false;
    }
    if(c->command==CONTROL_PONG) {
        if(!probe_pending || c->request_id!=probe_id || (uint32_t)(ms-probe_ms)>COMMS_LINK_TIMEOUT_MS) {
            ++diag.rejected; report_locked(APP_RESULT_STALE,c->request_id,0); return false;
        }
        probe_pending=probe_offer=false; fresh_ms=ms; ++diag.accepted; return true;
    }
    if(c->request_id<=control_high_water) {
        ++diag.stale; report_locked(APP_RESULT_STALE,c->request_id,0); return false;
    }
    control_high_water=c->request_id;
    if(c->command==CONTROL_QUERY) {
        fresh_ms=ms; ++diag.accepted; report_locked(APP_RESULT_OK,c->request_id,0); return true;
    }
    if(c->command==CONTROL_ESTOP) {
        MotorSafety_RequestStopCauseLocked(MOTOR_STOP_MANUAL,MOTOR_COUNT);
        CommsSession_StateLockedSet(APP_FAULT); report_locked(APP_RESULT_OK,c->request_id,0);
        fresh_ms=ms; ++diag.accepted; return true;
    }
    bool allowed=(c->command==CONTROL_ALLOW_START || c->command==CONTROL_DENY_START) ? s==APP_READY :
        c->command==CONTROL_RESTART ? (s==APP_STOPPED || s==APP_FAULT) :
        c->command==CONTROL_STOP ? (s==APP_INIT_MODULES || s==APP_INIT_RETURN || s==APP_READY || s==APP_WORK_WAIT || s==APP_WORK_EXECUTING || s==APP_STOPPING || s==APP_STOPPED) : false;
    if(!allowed || (control_pending && c->command!=CONTROL_STOP)) {
        ++diag.rejected; report_locked(APP_RESULT_WRONG_STATE,c->request_id,0); return false;
    }
    if(c->command==CONTROL_STOP) {
        if(s==APP_INIT_MODULES) {
            MotorSafety_RequestStopCauseLocked(MOTOR_STOP_MANUAL,MOTOR_COUNT);
            CommsSession_StateLockedSet(APP_STOPPED);
        } else if(s!=APP_STOPPING && s!=APP_STOPPED) {
            /* Epoch cancellation rejects old RX backlog and already-popped intents.
             * PID/output zero is immediate; return-to-default starts in ControlTask. */
            MotorSafety_CancelCommandsLocked();
            CommsSession_StateLockedSet(APP_STOPPING);
            control_epoch=MotorSafety_EpochLocked(); pending_control=*c; control_pending=true;
        }
    } else { pending_control=*c; control_epoch=epoch; control_pending=true; }
    fresh_ms=ms; ++diag.accepted; report_locked(APP_RESULT_OK,c->request_id,0); return true;
}
bool CommsSession_OnMessage(const rx_result_t *msg,uint32_t ms,uint32_t epoch,bool *started) {
    if(started) *started=false;
    uint32_t k=Motor_Lock(); ++diag.received; uint32_t now=HAL_GetTick(); CommsSession_PollLocked(now);
    if(!enabled || !msg || msg->type==RX_TYPE_NONE) { ++diag.rejected; Motor_Unlock(k); return false; }
    ++diag.valid;
    bool urgent=msg->type==RX_TYPE_CONTROL && msg->control.command==CONTROL_ESTOP;
    if((!urgent && epoch!=MotorSafety_EpochLocked()) || (uint32_t)(now-ms)>COMMS_LINK_TIMEOUT_MS) {
        ++diag.stale; report_locked(APP_RESULT_STALE,0,0); Motor_Unlock(k); return false;
    }
    if(msg->type==RX_TYPE_HELLO) {
        if(msg->proto_version!=1 || !msg->data.device_id) {
            ++diag.rejected; report_locked(APP_RESULT_BAD_VERSION,0,msg->proto_version); Motor_Unlock(k); return false;
        }
        if(peer_id && msg->data.device_id!=peer_id) {
            ++diag.rejected; report_locked(APP_RESULT_PEER_CHANGED,0,msg->data.device_id);
            MotorSafety_RequestStopCauseLocked(MOTOR_STOP_LINK,MOTOR_COUNT); lost=true;
            Motor_Unlock(k); return false;
        }
        bool begin=!session || lost;
        if(begin) { CommsSession_OnStopLocked(); probe_pending=probe_offer=false; }
        session=true; lost=false; peer_id=msg->data.device_id; fresh_ms=ms;
        if(begin) last_probe_ms=ms;
        ++diag.hello; ++diag.accepted; if(started) *started=begin;
        Motor_Unlock(k); return true;
    }
    bool result=false;
    if(msg->type==RX_TYPE_CONTROL) result=on_control(&msg->control,ms,epoch);
    else if(msg->type==RX_TYPE_ACTION && msg->data.action_id<=ACTION_WRIST_DORSI && fresh(now) &&
            MotorSafety_CanArmLocked() && (status.state==APP_WORK_WAIT || status.state==APP_WORK_EXECUTING)) {
        if(count==COMMS_INTENT_CAPACITY) {
            ++diag.queue_full; report_locked(APP_RESULT_QUEUE_FULL,0,msg->data.action_id);
        } else {
            intents[(head+count)%COMMS_INTENT_CAPACITY]=(intent_t){(intent_id_t)msg->data.action_id,epoch};
            ++count; ++diag.queued; ++diag.accepted; fresh_ms=ms; result=true;
        }
    } else { ++diag.rejected; report_locked(APP_RESULT_WRONG_STATE,0,msg->type); }
    Motor_Unlock(k); return result;
}
bool CommsSession_GetIntent(intent_t *out) {
    if(!out) return false;
    uint32_t k=Motor_Lock(); CommsSession_PollLocked(HAL_GetTick()); bool result=false;
    if(count && status.state==APP_WORK_WAIT) {
        intent_t item=intents[head]; head=(head+1)%COMMS_INTENT_CAPACITY; --count;
        if(item.epoch==MotorSafety_EpochLocked() && MotorSafety_CanArmLocked() && fresh(HAL_GetTick())) {
            *out=item; result=true;
        } else ++diag.stale;
    }
    Motor_Unlock(k); return result;
}
bool CommsSession_TakeControl(lifecycle_control_t *out,uint32_t *epoch) {
    uint32_t k=Motor_Lock(); bool ok=out && epoch && control_pending;
    if(ok) { *out=pending_control; *epoch=control_epoch; control_pending=false; }
    Motor_Unlock(k); return ok;
}
bool CommsSession_GetProbe(lifecycle_control_t *out) {
    uint32_t k=Motor_Lock(); uint32_t now=HAL_GetTick(); CommsSession_PollLocked(now);
    if(enabled && fresh(now) && !probe_pending && ((uint32_t)(now-fresh_ms)>=COMMS_PROBE_IDLE_MS ||
       (uint32_t)(now-last_probe_ms)>=COMMS_PROBE_PERIOD_MS)) {
        if(++probe_id==0) ++probe_id;
        probe_pending=probe_offer=true; probe_ms=last_probe_ms=now;
    }
    bool ok=out && probe_offer && !lost;
    if(ok) { *out=(lifecycle_control_t){CONTROL_PING,probe_id,peer_id}; probe_offer=false; }
    Motor_Unlock(k); return ok;
}
void CommsSession_GetStatus(lifecycle_status_t *out) {
    if(!out) return;
    uint32_t k=Motor_Lock(); *out=status; out->queue_depth=count; out->dropped=diag.queue_full;
    out->causes=MotorSafety_GetStopCauses(); Motor_Unlock(k);
}
void CommsSession_ActionStarted(void) {
    uint32_t k=Motor_Lock(); ++status.action_sequence; status.action_done=0; ++status.revision; Motor_Unlock(k);
}
void CommsSession_ActionCompleted(void) {
    uint32_t k=Motor_Lock(); status.action_done=1; ++status.revision; Motor_Unlock(k);
}
void CommsSession_GetDiagnostics(comms_diagnostics_t *out) {
    if(!out) return;
    uint32_t k=Motor_Lock(); *out=diag; Motor_Unlock(k);
}
