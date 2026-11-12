#include "app.h"
#include "motor_control.h"
#include "comm.h"
#include "motor_estimation.h"
#include "boot_homing.h"
#include "hw_test_config.h"
#include "force_sensor.h"
#include "haptics.h"
#include "status_leds.h"

#if !defined(MOTOR_BACKEND_SIM) && !N_PULSE_HW_SINGLE_MOTOR_TEST
#include "comms_session.h"
#include "motor_map.h"
#include "motor_safety.h"
#include "motor_lock.h"
#include "lifecycle_config.h"
#include <math.h>

/* Boot homing owns encoder zero offsets. Reconnection retains them. */
static bool calibrated, settle_started;
static uint32_t handled_fault_masks[MOTOR_STOP_CAUSE_COUNT];
static uint32_t battery_warning_revision;
static uint32_t init_ms, motion_ms, settle_ms;
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
static bool initial_pose_settled(void) {
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
            in_range=false;
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
    if(ok) { settle_started=false; motion_ms=HAL_GetTick(); }
    Motor_Unlock(k); return ok;
}

#endif
__attribute__((weak)) void App_OnCommunicationInterrupted(void) { }
__attribute__((weak)) void App_OnBatteryWarning(bool active,const battery_snapshot_t *snapshot) {
    (void)active; (void)snapshot; /* Indication/telemetry policy to be assigned. */
}

__attribute__((weak)) void App_OnSafetyFault(motor_stop_cause_t cause,uint32_t motor_mask) {
    (void)motor_mask;
    /* Every branch is entered AFTER the common all-motor stop. Follow-up
     * handling remains deliberately undefined; none automatically recovers. */
    switch(cause) {
    case MOTOR_STOP_GUARD: break;         /* Stall / mechanical obstruction. */
    case MOTOR_STOP_FORCE: break;         /* Force hard limit. */
    case MOTOR_STOP_DRIVER: break;        /* DRV8848 physical fault. */
    case MOTOR_STOP_FEEDBACK: break;      /* Missing/invalid sensor feedback. */
    case MOTOR_STOP_CONTROL_DELAY: break; /* Control loop timing fault. */
    case MOTOR_STOP_PID: break;           /* Invalid control calculation. */
    case MOTOR_STOP_INIT: break;          /* Initialization / homing failure. */
    case MOTOR_STOP_LINK: break;          /* UART transport fault, not heartbeat. */
    case MOTOR_STOP_MOTION_TIMEOUT: break;
    case MOTOR_STOP_POSITION: break;      /* Grip exceeds configured closing travel. */
    case MOTOR_STOP_MANUAL: break;        /* Internal stop request only. */
    default: break;
    }
}

void App_Init(void) {
    /* Safe output/feedback/UART plumbing must exist before handshake or tasks. */
    Motor_InitAll();
    MotorEstimation_Init();
    ForceSensor_Init();
    Haptics_Init();
    Battery_Init();
    (void)BootHoming_RunOnce();
    Comms_Init();
    StatusLeds_Init();
#if !defined(MOTOR_BACKEND_SIM) && !N_PULSE_HW_SINGLE_MOTOR_TEST
    for(unsigned i=0;i<MOTOR_STOP_CAUSE_COUNT;++i) handled_fault_masks[i]=0;
    battery_warning_revision=0;
    calibrated=BootHoming_Done();
    CommsSession_SetState(APP_INIT_MODULES);
    settle_started=false;
    init_ms=HAL_GetTick();
    if(!isfinite(APP_POSITION_TOLERANCE_RAD) || APP_POSITION_TOLERANCE_RAD<=0)
        fault(APP_RESULT_INIT_FAILED,MOTOR_STOP_INIT);
#endif
}
void App_Task(void) {
#if N_PULSE_HW_SINGLE_MOTOR_TEST
    /* Dedicated commissioning owns targets; never initialize/return all axes. */
    return;
#elif defined(MOTOR_BACKEND_SIM)
    /* SIM v2 dispatches its own targets directly from SimSession. */
    return;
#else
    battery_snapshot_t battery; Battery_GetSnapshot(&battery);
    if(battery.warning_revision!=battery_warning_revision) {
        battery_warning_revision=battery.warning_revision;
        App_OnBatteryWarning(battery.low_warning,&battery);
    }
    if(CommsSession_TakeLinkLoss()) App_OnCommunicationInterrupted();
    app_state_t s=CommsSession_GetState();
    if(MotorSafety_IsFaultActive()) {
        CommsSession_SetState(APP_FAULT);
        /* Immediate all-output inhibition is done by MotorSafety at detection;
         * task context completes HAL cleanup and dispatches deferred branches. */
        Motor_ServiceStop();
        uint32_t causes=MotorSafety_GetStopCauses();
        for(unsigned i=0;i<MOTOR_STOP_CAUSE_COUNT;++i) {
            motor_stop_cause_t cause=(motor_stop_cause_t)(1U<<i);
            if(!(causes&(uint32_t)cause)) continue;
            uint32_t mask=MotorSafety_GetCauseMotorMask(cause);
            uint32_t newly_affected=mask&~handled_fault_masks[i];
            handled_fault_masks[i]|=mask;
            if(newly_affected) App_OnSafetyFault(cause,newly_affected);
        }
        return;
    }
    if(s==APP_INIT_MODULES) {
        if(!calibrated) {
            fault(APP_RESULT_INIT_FAILED,MOTOR_STOP_INIT); return;
        }
        if(all_feedback_ready() && Motor_ControlReady()) {
            if(!Motor_DRV8848_IsArmed() && !Motor_DRV8848_Arm()) {
                fault(APP_RESULT_INIT_FAILED,MOTOR_STOP_INIT); return;
            }
            if(set_default(APP_INIT_MODULES)) transition(APP_INIT_MODULES,APP_INIT_RETURN);
            else fault(APP_RESULT_INIT_FAILED,MOTOR_STOP_INIT);
        } else if((uint32_t)(HAL_GetTick()-init_ms)>APP_INIT_TIMEOUT_MS)
            fault(APP_RESULT_INIT_FAILED,MOTOR_STOP_INIT);
    } else if(s==APP_INIT_RETURN) {
        /* Only startup return uses arrival/settling checks. Work never does. */
        if(initial_pose_settled()) transition(APP_INIT_RETURN,APP_INIT_HANDSHAKE);
        else if((uint32_t)(HAL_GetTick()-motion_ms)>APP_MOTION_TIMEOUT_MS)
            fault(APP_RESULT_INIT_FAILED,MOTOR_STOP_INIT);
    }
#endif
}
