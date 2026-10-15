#ifdef MOTOR_BACKEND_SIM
#include "sim_session.h"
#include "motor_backend.h"
#include "motor_safety.h"
#include "motor_sim_config.h"
#include "motor_safety_config.h"
#include "motor_lock.h"
#include "uart4_transport.h"
#include "intent_router.h"
#include <string.h>
#include <math.h>
static uint64_t session, high_water;
static uint32_t rx_seq, tx_seq, fresh_ms, run, last_output_ms, last_status_ms;
static bool live, configured, armed, stop_pending, reply_pending;
static uint8_t force_mask, reply_type, reply_request, reply_result;
static uint32_t reply_seq;
static float output[MOTOR_COUNT], measured[MOTOR_COUNT], applied[MOTOR_COUNT];
static uint32_t position_ms[MOTOR_COUNT], applied_ms[MOTOR_COUNT], force_ms[MOTOR_COUNT];
static uint8_t position_seen, applied_seen, force_seen;
static sim_diagnostics_t diag;
static uint8_t enabled_mask(void) {
    uint8_t mask=0U;
    for(unsigned i=0;i<MOTOR_COUNT;++i) if(Motor_IsEnabled((motor_id_t)i)) mask|=(uint8_t)(1U<<i);
    return mask;
}
static bool fresh(uint32_t now) { return live && (uint32_t)(now-fresh_ms)<=SIM_LINK_TIMEOUT_MS; }
static void invalidate(void) {
    position_seen=applied_seen=force_seen=0U;
    for(unsigned i=0;i<MOTOR_COUNT;++i) Motor_InvalidatePositionFeedback((motor_id_t)i);
}
void SimSession_Init(void) {
    session=high_water=0U; rx_seq=tx_seq=fresh_ms=run=last_output_ms=last_status_ms=0U;
    live=configured=armed=stop_pending=reply_pending=false; force_mask=0U;
    position_seen=applied_seen=force_seen=0U;
    memset(output,0,sizeof(output)); memset(&diag,0,sizeof(diag));
}
void SimSession_InhibitLocked(void) {
    armed=false; if(run!=UINT32_MAX) ++run; stop_pending=session!=0U;
    UART4Transport_DiscardSimOutputLocked();
    memset(output,0,sizeof(output)); ++diag.stops;
}
void SimSession_PollLocked(uint32_t now) {
    if(live && !fresh(now)) { live=false; MotorSafety_RequestStopCauseLocked(MOTOR_STOP_LINK,MOTOR_COUNT); }
}
void SimSession_TransportErrorLocked(void) {
    if(session) { live=false; MotorSafety_RequestStopCauseLocked(MOTOR_STOP_LINK,MOTOR_COUNT); }
}
bool SimSession_CanRecoverLocked(uint32_t now) { return configured && fresh(now); }
bool SimSession_IsArmedLocked(void) { return armed && configured && fresh(HAL_GetTick()); }
bool SimSession_FeedbackReadyLocked(motor_id_t id) {
    if((unsigned)id>=MOTOR_COUNT || !configured || !fresh(HAL_GetTick())) return false;
    uint8_t bit=(uint8_t)(1U<<id); uint32_t now=HAL_GetTick();
    return (position_seen&bit) && (applied_seen&bit) &&
        (uint32_t)(now-position_ms[id])<=MOTOR_FEEDBACK_TIMEOUT_MS &&
        (uint32_t)(now-applied_ms[id])<=MOTOR_FEEDBACK_TIMEOUT_MS &&
        (!(force_mask&bit) || ((force_seen&bit) && (uint32_t)(now-force_ms[id])<=MOTOR_FEEDBACK_TIMEOUT_MS &&
                              MotorSafety_HasFreshForceLocked(id,now)));
}
void SimSession_StageOutputLocked(motor_id_t id,float value) { if((unsigned)id<MOTOR_COUNT) output[id]=value; }
bool SimSession_GuardSampleLocked(motor_id_t id,float *position,float *drive) {
    if(!SimSession_FeedbackReadyLocked(id)) return false;
    *position=measured[id]; *drive=applied[id]; return true;
}
static void reply(uint8_t type,uint8_t request,uint32_t sequence,bool success) {
    reply_pending=true; reply_type=type; reply_request=request; reply_seq=sequence; reply_result=success?1U:0U;
}
static bool malformed(void) { ++diag.malformed; ++diag.rejected; return false; }
/* All fields of a packet are validated before any entry is delivered. */
static bool validate(uint8_t type,const uint8_t *p,size_t n) {
    size_t k=SIM_COMMON_SIZE;
    if(n<k || !p) return false;
    switch(type) {
    case SIM_HELLO: case SIM_HEARTBEAT: case SIM_RECOVER: case SIM_ARM: case SIM_STOP: return n==k;
    case SIM_CONFIG: return n==k+4U && Sim_Read16(p+k)==SIM_MODEL_ROTOR_V1 && p[k+2U]==0xFFU;
    case SIM_ENABLE: return n==k+1U && p[k]!=0U;
    case SIM_INTENT: return n==k+1U && p[k]<=ACTION_ROTATE_WRIST_L;
    case SIM_TARGET: {
        if(n<k+1U || !p[k] || p[k]>MOTOR_COUNT || n!=k+1U+(size_t)p[k]*6U) return false;
        uint8_t seen=0U;
        for(unsigned i=0;i<p[k];++i) {
            size_t o=k+1U+i*6U; unsigned id=p[o]; float value=Sim_ReadFloat(p+o+1U);
            if(id>=MOTOR_COUNT || (seen&(1U<<id)) || !isfinite(value) || fpclassify(value)==FP_SUBNORMAL || fabsf(value)>SIM_POSITION_LIMIT_RAD || p[o+5U]>100U) return false;
            seen|=(uint8_t)(1U<<id);
        }
        return true;
    }
    case SIM_FEEDBACK: {
        if(n<k+7U || Sim_Read16(p+k+4U)>SIM_SAMPLE_MAX_AGE_MS || !p[k+6U] || p[k+6U]>MOTOR_COUNT ||
           n!=k+7U+(size_t)p[k+6U]*14U) return false;
        uint8_t seen=0U;
        for(unsigned i=0;i<p[k+6U];++i) {
            size_t o=k+7U+i*14U; unsigned id=p[o]; uint8_t fields=p[o+1U];
            float pos=Sim_ReadFloat(p+o+2U),vel=Sim_ReadFloat(p+o+6U);
            int16_t drive=(int16_t)Sim_Read16(p+o+12U);
            if(id>=MOTOR_COUNT || (seen&(1U<<id)) || !fields || (fields&~15U) ||
               !isfinite(pos) || !isfinite(vel) || fpclassify(pos)==FP_SUBNORMAL || fpclassify(vel)==FP_SUBNORMAL || fabsf(pos)>1000.0f || fabsf(vel)>1000.0f ||
               (!(fields&MOTOR_FB_POSITION) && Sim_Read32(p+o+2U)!=0U) ||
               (!(fields&MOTOR_FB_VELOCITY) && Sim_Read32(p+o+6U)!=0U) ||
               (!(fields&MOTOR_FB_FORCE) && Sim_Read16(p+o+10U)!=0U) ||
               (!(fields&8U) && drive!=0) || drive < -10000 || drive > 10000) return false;
            seen|=(uint8_t)(1U<<id);
        }
        return true;
    }
    default: return false;
    }
}
bool SimSession_OnFrame(uint8_t type,const uint8_t *p,size_t n,uint32_t ms,uint32_t epoch,void *unused) {
    uint32_t key=Motor_Lock(); uint32_t now=HAL_GetTick();
    if(unused && !UART4Transport_RxGenerationMatches(*(const uint32_t *)unused)) {
        ++diag.old; Motor_Unlock(key); return false;
    }
    if(!validate(type,p,n)) {
        /* Malformed numerical feedback must not leave an active old sample usable. */
        if(type==SIM_FEEDBACK && armed) MotorSafety_RequestStopCauseLocked(MOTOR_STOP_FEEDBACK,MOTOR_COUNT);
        bool result=malformed(); Motor_Unlock(key); return result;
    }
    uint64_t sid=Sim_Read64(p); uint32_t sequence=Sim_Read32(p+8U),stamp=Sim_Read32(p+12U);
    if(type==SIM_HELLO) {
        if(sequence!=0U || stamp!=0U || !sid || (uint32_t)(now-ms)>SIM_LEASE_MAX_MS || epoch!=MotorSafety_EpochLocked()) {
            ++diag.old; Motor_Unlock(key); return false;
        }
        if(sid==session && live) { reply(SIM_HELLO_ACK,type,0U,true); Motor_Unlock(key); return true; }
        if(sid<=high_water) { ++diag.old; Motor_Unlock(key); return false; }
        if(session) MotorSafety_RequestStopCauseLocked(MOTOR_STOP_LINK,MOTOR_COUNT);
        session=high_water=sid; run=0U; live=true; configured=armed=false; fresh_ms=ms; rx_seq=tx_seq=0U;
        force_mask=0U; invalidate(); memset(output,0,sizeof(output));
        reply(SIM_HELLO_ACK,type,0U,true); ++diag.accepted;
        Motor_Unlock(key); return true;
    }
    SimSession_PollLocked(now);
    if(!fresh(now) || sid!=session || epoch!=MotorSafety_EpochLocked() ||
       !Sim_SequenceNewer(sequence,rx_seq) || (uint32_t)(now-ms)>SIM_LEASE_MAX_MS ||
       (uint32_t)(now-stamp)>SIM_LEASE_MAX_MS) { ++diag.old; Motor_Unlock(key); return false; }
    /* Fields incompatible with this explicit configuration are rejected atomically. */
    if(type==SIM_FEEDBACK) {
        if(!configured || Sim_Read32(p+16U)!=run) { ++diag.rejected; Motor_Unlock(key); return false; }
        for(unsigned i=0;i<p[22U];++i) {
            size_t o=23U+i*14U;
            if((p[o+1U]&MOTOR_FB_FORCE) && !(force_mask&(1U<<p[o]))) {
                ++diag.rejected; Motor_Unlock(key); return false;
            }
        }
    }
    rx_seq=sequence; fresh_ms=ms; ++diag.accepted;
    bool success=false;
    switch(type) {
    case SIM_HEARTBEAT: success=true; break;
    case SIM_STOP: MotorSafety_RequestStopCauseLocked(MOTOR_STOP_MANUAL,MOTOR_COUNT); success=true; break;
    case SIM_CONFIG:
        if(!configured && !armed && !enabled_mask()) { configured=true; force_mask=p[19U]; invalidate(); success=true; }
        break;
    case SIM_RECOVER:
        Motor_Unlock(key); success=Motor_RecoverFromFault(); key=Motor_Lock(); break;
    case SIM_ARM:
        success=configured && !armed && run!=UINT32_MAX && MotorSafety_CanArmLocked();
        for(unsigned i=0;i<MOTOR_COUNT && success;++i) success=SimSession_FeedbackReadyLocked((motor_id_t)i) && Motor_HasFreshPositionFeedback((motor_id_t)i);
        if(success) { ++run; armed=true; invalidate(); }
        break;
    case SIM_ENABLE:
        success=SimSession_IsArmedLocked() && MotorSafety_CanArmLocked() && !(enabled_mask()&p[16U]);
        for(unsigned i=0;i<MOTOR_COUNT && success;++i)
            if(p[16U]&(1U<<i)) success=SimSession_FeedbackReadyLocked((motor_id_t)i) && Motor_HasFreshPositionFeedback((motor_id_t)i);
        if(success) for(unsigned i=0;i<MOTOR_COUNT;++i)
            if((p[16U]&(1U<<i)) && !Motor_Enable((motor_id_t)i)) { Motor_StopAll(); success=false; break; }
        break;
    case SIM_TARGET:
        success=SimSession_IsArmedLocked() && MotorSafety_CanArmLocked();
        for(unsigned i=0;i<p[16U] && success;++i) success=Motor_IsEnabled((motor_id_t)p[17U+i*6U]);
        if(success) for(unsigned i=0;i<p[16U];++i) {
            size_t o=17U+i*6U; Motor_SetTargetAtEpoch((motor_id_t)p[o],Sim_ReadFloat(p+o+1U),p[o+5U],MotorSafety_EpochLocked());
        }
        break;
    case SIM_INTENT:
        /* Full activation required: no partly applied high-level movement. */
        success=SimSession_IsArmedLocked() && MotorSafety_CanArmLocked() && enabled_mask()==0xFFU;
        if(success) {
            intent_t intention={.id=(intent_id_t)p[16U],.epoch=MotorSafety_EpochLocked()};
            IntentRouter_Handle(&intention);
        }
        break;
    case SIM_FEEDBACK: {
        uint32_t sample_ms=stamp-Sim_Read16(p+20U);
        for(unsigned i=0;i<p[22U];++i) {
            size_t o=23U+i*14U; motor_id_t id=(motor_id_t)p[o]; uint8_t fields=p[o+1U],bit=(uint8_t)(1U<<id);
            motor_feedback_t fb={ .id=id, .position=Sim_ReadFloat(p+o+2U), .velocity=Sim_ReadFloat(p+o+6U),
                .force_mN=Sim_Read16(p+o+10U), .valid_fields=(uint8_t)(fields&7U) };
            if(fields&MOTOR_FB_POSITION) { measured[id]=fb.position; position_ms[id]=sample_ms; position_seen|=bit; }
            if(fields&8U) { applied[id]=(float)(int16_t)Sim_Read16(p+o+12U)/100.0f; applied_ms[id]=sample_ms; applied_seen|=bit; }
            if(fields&MOTOR_FB_FORCE) { force_ms[id]=sample_ms; force_seen|=bit; }
            MotorBackend_OnFeedbackAtTime(&fb,sample_ms);
        }
        success=true; break;
    }
    default: break;
    }
    if(type!=SIM_HEARTBEAT && type!=SIM_FEEDBACK) reply(SIM_STATUS,type,sequence,success);
    if(!success) ++diag.rejected;
    Motor_Unlock(key); return success;
}
static void common(uint8_t *p,uint32_t now) { Sim_Write64(p,session); Sim_Write32(p+8U,++tx_seq); Sim_Write32(p+12U,now); }
void SimSession_Service(void) {
    uint8_t p[SIM_PAYLOAD_MAX],frame[SIM_FRAME_MAX]; size_t length=0U; uint8_t type=0U,slot=0U;
    uint32_t key=Motor_Lock(),now=HAL_GetTick();
    SimSession_PollLocked(now);
    uint32_t epoch=MotorSafety_EpochLocked();
    if(session && stop_pending) {
        stop_pending=false; common(p,now); Sim_Write32(p+16U,run); Sim_Write32(p+20U,MotorSafety_GetStopCauses());
        type=SIM_STOP; length=24U; slot=2U;
    } else if(session && (reply_pending || (uint32_t)(now-last_status_ms)>=SIM_STATUS_PERIOD_MS)) {
        common(p,now); Sim_Write32(p+16U,run); Sim_Write32(p+20U,epoch); Sim_Write32(p+24U,MotorSafety_GetStopCauses());
        p[28U]=enabled_mask(); p[29U]=armed?1U:0U; p[30U]=configured?1U:0U;
        p[31U]=reply_pending?reply_request:0U; Sim_Write32(p+32U,reply_pending?reply_seq:0U); p[36U]=reply_pending?reply_result:0U;
        type=reply_pending?reply_type:SIM_STATUS; reply_pending=false; length=37U; slot=1U; last_status_ms=now;
    } else if(SimSession_IsArmedLocked() && (uint32_t)(now-last_output_ms)>=SIM_OUTPUT_PERIOD_MS) {
        common(p,now); Sim_Write32(p+16U,run); Sim_Write32(p+20U,epoch); p[24U]=enabled_mask();
        for(unsigned i=0;i<MOTOR_COUNT;++i) Sim_Write16(p+25U+i*2U,(uint16_t)(int16_t)lroundf(output[i]*100.0f));
        type=SIM_OUTPUT; length=41U; slot=0U; last_output_ms=now;
    }
    Motor_Unlock(key);
    if(length) {
        size_t n=Sim_Encode(type,p,length,frame,sizeof(frame));
        UART4Transport_QueueSimFrame(frame,n,epoch,now,slot);
    }
}
void SimSession_GetDiagnostics(sim_diagnostics_t *out) {
    if(!out) return;
    uint32_t key=Motor_Lock(); *out=diag; out->session=session; out->run=run;
    out->live=live; out->configured=configured; out->armed=armed; Motor_Unlock(key);
}
#endif
