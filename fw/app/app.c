#include "app.h"
#include "motor_control.h"
#include "comm.h"
#include "intent_router.h"
#include "motor_estimation.h"
#include "hw_test_config.h"

#if !defined(MOTOR_BACKEND_SIM) && !N_PULSE_HW_SINGLE_MOTOR_TEST
#include "comms_session.h"
#include "encoder_bsp.h"
#include "motor_map.h"
#include "motor_safety.h"
#include "motor_lock.h"
#include "lifecycle_config.h"
#include <math.h>

/* Confirm each axis on the actual board, particularly LPTIM quadrature.
 * Zero offsets are relative to the boot pose. Never reset them on RESTART. */
static const encoder_calibration_t calibration[MOTOR_COUNT] = {
#define CAL {APP_ENCODER_PPR,APP_ENCODER_QUADRATURE,APP_ENCODER_GEAR_RATIO,APP_ENCODER_DIRECTION,0}
    CAL,CAL,CAL,CAL,CAL,CAL,CAL,CAL
#undef CAL
};
static bool calibrated, return_started, settle_started;
static uint32_t init_ms, motion_ms, settle_ms, affected_mask;
static float previous_targets[MOTOR_COUNT];
static void transition(app_state_t expected, app_state_t next) {
    uint32_t k=Motor_Lock();
    if(CommsSession_StateLocked()==expected && MotorSafety_CanArmLocked())
        CommsSession_StateLockedSet(next);
    Motor_Unlock(k);
}
static void fault(app_result_t reason, motor_stop_cause_t cause) {
    MotorSafety_RequestStopCause(cause,MOTOR_COUNT);
    CommsSession_Report(reason,0,0);
}
static bool all_feedback_ready(void) {
    for(unsigned i=0;i<MOTOR_COUNT;++i)
        if(!Motor_HasFreshPositionFeedback((motor_id_t)i)) return false;
    return true;
}
static void begin_motion(bool all) {
    affected_mask=0; settle_started=false; motion_ms=HAL_GetTick();
    for(unsigned i=0;i<MOTOR_COUNT;++i) {
        motor_state_t s;
        if(!Motor_GetSnapshot((motor_id_t)i,&s)) continue;
        if(all || s.target_position!=previous_targets[i] ||
           !s.feedback_valid || !isfinite(s.measured_position) ||
           fabsf(s.measured_position-s.target_position)>APP_POSITION_TOLERANCE_RAD)
            affected_mask|=1U<<i;
        previous_targets[i]=s.target_position;
    }
}
static bool motion_complete(void) {
    /* Recheck unchanged held joints too: drift must not fabricate completion. */
    bool in_range=true;
    for(unsigned i=0;i<MOTOR_COUNT;++i) {
        motor_state_t s;
        if(!Motor_GetSnapshot((motor_id_t)i,&s) || !s.enabled ||
           !Motor_HasFreshPositionFeedback((motor_id_t)i) ||
           !isfinite(s.measured_position) || !isfinite(s.target_position)) {
            in_range=false; continue;
        }
        if(fabsf(s.measured_position-s.target_position)>APP_POSITION_TOLERANCE_RAD ||
           !isfinite(s.reference_position) || fabsf(s.reference_position-s.target_position)>APP_POSITION_TOLERANCE_RAD) {
            affected_mask|=1U<<i; in_range=false;
        }
    }
    if(!in_range) { settle_started=false; return false; }
    uint32_t now=HAL_GetTick();
    if(!settle_started) { settle_started=true; settle_ms=now; }
    return (uint32_t)(now-settle_ms)>=APP_SETTLE_MS;
}
static bool set_default(app_state_t expected) {
    uint32_t k=Motor_Lock(); bool ok=CommsSession_StateLocked()==expected && MotorSafety_CanArmLocked();
    uint32_t epoch=MotorSafety_EpochLocked();
    for(unsigned i=0;ok && i<MOTOR_COUNT;++i) {
        if(!Motor_IsEnabled((motor_id_t)i)) ok=Motor_Enable((motor_id_t)i);
        if(ok) ok=Motor_SetTargetAtEpoch((motor_id_t)i,
            MotorMap_GetPose(MOTOR_POSE_OPEN,(motor_id_t)i),APP_HOME_SPEED_PERCENT,epoch);
    }
    if(ok) begin_motion(true);
    Motor_Unlock(k); return ok;
}
static void handle_control(void) {
    lifecycle_control_t c; uint32_t epoch;
    if(!CommsSession_TakeControl(&c,&epoch)) return;
    uint32_t k=Motor_Lock();
    bool current=epoch==MotorSafety_EpochLocked(); app_state_t s=CommsSession_StateLocked();
    if(current && c.command==CONTROL_ALLOW_START && s==APP_READY)
        CommsSession_StateLockedSet(APP_WORK_WAIT);
    if(current && c.command==CONTROL_STOP && s==APP_STOPPING) return_started=false;
    Motor_Unlock(k);
    if(!current) { CommsSession_Report(APP_RESULT_STALE,c.request_id,0); return; }
    if(c.command==CONTROL_RESTART && (s==APP_STOPPED || s==APP_FAULT)) {
        /* No zero reset: fault/ESTOP can leave the mechanism away from boot pose. */
        if(!Motor_RecoverFromFault()) {
            CommsSession_Report(APP_RESULT_RECOVERY_DENIED,c.request_id,MotorSafety_GetStopCauses()); return;
        }
        k=Motor_Lock();
        if(MotorSafety_CanArmLocked() && CommsSession_StateLocked()==s) {
            MotorSafety_CancelCommandsLocked();
            CommsSession_BeginRestartLocked();
            CommsSession_StateLockedSet(APP_INIT_HANDSHAKE);
            init_ms=HAL_GetTick(); return_started=settle_started=false;
        }
        Motor_Unlock(k);
    }
    /* DENY_START deliberately leaves READY holding the initial pose. */
}
#endif
void App_Init(void) {
    /* Safe output/feedback/UART plumbing must exist before handshake or tasks. */
    Motor_InitAll();
    MotorEstimation_Init();
    Comms_Init();
#if !defined(MOTOR_BACKEND_SIM) && !N_PULSE_HW_SINGLE_MOTOR_TEST
    calibrated=return_started=settle_started=false; affected_mask=0;
    init_ms=HAL_GetTick();
    for(unsigned i=0;i<MOTOR_COUNT;++i) previous_targets[i]=0;
    if(!isfinite(APP_POSITION_TOLERANCE_RAD) || APP_POSITION_TOLERANCE_RAD<=0)
        fault(APP_RESULT_INIT_FAILED,MOTOR_STOP_INIT);
#endif
}
void App_Task(void) {
#if N_PULSE_HW_SINGLE_MOTOR_TEST
    /* Dedicated commissioning owns targets; never initialize/return all axes. */
    return;
#elif defined(MOTOR_BACKEND_SIM)
    /* SIM v2 retains its independent CONFIG/ARM/TARGET protocol. */
    intent_t intent;
    if(Comms_GetNextIntent(&intent)) IntentRouter_Handle(&intent);
#else
    handle_control();
    app_state_t s=CommsSession_GetState();
    if(MotorSafety_IsFaultActive() && s!=APP_STOPPED) {
        CommsSession_SetState(APP_FAULT); return;
    }
    if(s==APP_INIT_HANDSHAKE) {
        if(CommsSession_HasSession()) { transition(APP_INIT_HANDSHAKE,APP_INIT_MODULES); init_ms=HAL_GetTick(); }
    } else if(s==APP_INIT_MODULES) {
        if(!calibrated) {
            for(unsigned i=0;i<MOTOR_COUNT;++i) {
                if(!Encoder_BSP_SetCalibration((motor_id_t)i,&calibration[i])) {
                    fault(APP_RESULT_INIT_FAILED,MOTOR_STOP_INIT); return;
                }
            }
            calibrated=true;
        }
        if(all_feedback_ready()) {
            if(!Motor_DRV8848_IsArmed() && !Motor_DRV8848_Arm()) {
                fault(APP_RESULT_INIT_FAILED,MOTOR_STOP_INIT); return;
            }
            if(set_default(APP_INIT_MODULES)) transition(APP_INIT_MODULES,APP_INIT_RETURN);
            else fault(APP_RESULT_INIT_FAILED,MOTOR_STOP_INIT);
        } else if((uint32_t)(HAL_GetTick()-init_ms)>APP_INIT_TIMEOUT_MS)
            fault(APP_RESULT_INIT_FAILED,MOTOR_STOP_INIT);
    } else if(s==APP_INIT_RETURN || s==APP_WORK_EXECUTING || s==APP_STOPPING) {
        if(s==APP_STOPPING && !return_started) {
            if(!set_default(APP_STOPPING)) { fault(APP_RESULT_INIT_FAILED,MOTOR_STOP_INIT); return; }
            return_started=true;
        }
        if(motion_complete()) {
            uint32_t k=Motor_Lock();
            if(CommsSession_StateLocked()==s && MotorSafety_CanArmLocked()) {
                if(s==APP_INIT_RETURN) {
                    CommsSession_StateLockedSet(APP_READY);
                    CommsSession_Report(APP_RESULT_OK,0,0);
                }
                else if(s==APP_WORK_EXECUTING) {
                    CommsSession_ActionCompleted(); CommsSession_StateLockedSet(APP_WORK_WAIT);
                } else {
                    Motor_StopAll(); /* Global inhibit and PID reset; retains reference. */
                    if(MotorSafety_GetStopCauses()==MOTOR_STOP_MANUAL)
                        CommsSession_StateLockedSet(APP_STOPPED);
                }
            }
            Motor_Unlock(k);
        } else if((uint32_t)(HAL_GetTick()-motion_ms)>APP_MOTION_TIMEOUT_MS)
            fault(APP_RESULT_MOTION_TIMEOUT,MOTOR_STOP_MOTION_TIMEOUT);
    }
    if(CommsSession_GetState()==APP_WORK_WAIT) {
        intent_t intent;
        if(Comms_GetNextIntent(&intent)) {
            uint32_t k=Motor_Lock();
            if(CommsSession_StateLocked()==APP_WORK_WAIT && intent.epoch==MotorSafety_EpochLocked() && MotorSafety_CanArmLocked()) {
                IntentRouter_Handle(&intent); begin_motion(false);
                CommsSession_ActionStarted(); CommsSession_StateLockedSet(APP_WORK_EXECUTING);
            }
            Motor_Unlock(k);
        }
    }
#endif
}
