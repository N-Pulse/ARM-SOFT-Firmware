#include "hw_commission.h"
#include "hw_test_config.h"
#include "motor_safety.h"
#include "motor_lock.h"
#include "comms_session.h"
#include "encoder_bsp.h"
#include "drv8848.h"
#include "motor_estimation.h"
#include <math.h>
#include <stddef.h>
#if N_PULSE_OLD_HW
#include "board_link.h"
static bool output_preflight;
static uint32_t preflight_epoch;
bool HWCommission_OutputPreflightReady(void) {
    if(N_PULSE_HW_FINGER_DIAGNOSTIC && !N_PULSE_DIAG_CHECK_PREFLIGHT) return true;
    return output_preflight && preflight_epoch == MotorSafety_EpochLocked();
}
#endif
#if N_PULSE_HW_SELECTABLE_FINGER
static motor_id_t selected_motor=MOTOR_COUNT; /* No finger authorized at boot. */
#endif
motor_id_t HWCommission_SelectedMotor(void) {
#if N_PULSE_HW_SELECTABLE_FINGER
    uint32_t key=Motor_Lock(); motor_id_t id=selected_motor; Motor_Unlock(key); return id;
#else
    return (motor_id_t)N_PULSE_HW_TEST_MOTOR;
#endif
}
bool HWCommission_ConfiguredFor(motor_id_t id) {
    return id==HWCommission_SelectedMotor() && HWCommission_Configured();
}
#if N_PULSE_HW_SINGLE_MOTOR_TEST
typedef struct { bool configured; float minimum,maximum,step_limit,rate; uint8_t pwm_limit; } settings_t;
static settings_t settings[5];
static uint32_t last_request;
static settings_t *active_settings(void) {
    motor_id_t id=HWCommission_SelectedMotor(); return (unsigned)id<5U?&settings[id]:NULL;
}
bool HWCommission_Configured(void) { settings_t *s=active_settings(); return s && s->configured; }
bool HWCommission_MeasurementAllowed(float position) {
    settings_t *s=active_settings();
    return s && s->configured && isfinite(position) &&
        ((N_PULSE_HW_FINGER_DIAGNOSTIC && !N_PULSE_DIAG_CHECK_TRAVEL) || (position>=s->minimum && position<=s->maximum));
}
float HWCommission_ReferenceRate(void) { settings_t *s=active_settings(); return s?s->rate:0; }
float HWCommission_PwmCeiling(void) {
    settings_t *s=active_settings(); float ceiling=s && s->configured?s->pwm_limit:0;
    if (!N_PULSE_HW_FINGER_DIAGNOSTIC && N_PULSE_HW_SELECTABLE_FINGER && HWCommission_SelectedMotor()==MOTOR_THUMB && ceiling>5.0f)
        ceiling=5.0f;
    return ceiling;
}
bool HWCommission_Ready(void) {
    uint32_t key = Motor_Lock();
    motor_state_t m;
    motor_id_t id = HWCommission_SelectedMotor();
    settings_t *s=active_settings();
    if(!s) { Motor_Unlock(key); return false; }
#if N_PULSE_OLD_HW
    if (!BoardLink_IsLocalMotor(id)) { Motor_Unlock(key); return false; }
#endif
    Motor_GetSnapshot(id, &m);
    bool ok = s->configured && CommsSession_IsFreshLocked(HAL_GetTick()) &&
        (HW_DIAGNOSTIC_TIMED || (Encoder_BSP_IsCalibrated(id) && Motor_HasFreshPositionFeedback(id))) &&
        HWCommission_MeasurementAllowed(HW_DIAGNOSTIC_TIMED?Motor_DiagnosticPosition(id):m.measured_position);
    Motor_Unlock(key); return ok;
}
bool HWCommission_FilterTarget(float position, uint8_t *pwm) {
    settings_t *s=active_settings();
    motor_state_t m;
    if(!s || !Motor_GetSnapshot(HWCommission_SelectedMotor(),&m)) return false;
    if(HW_DIAGNOSTIC_TIMED) m.measured_position=m.reference_position;
    if (!pwm || !s->configured || !isfinite(position) ||
        ((!N_PULSE_HW_FINGER_DIAGNOSTIC || N_PULSE_DIAG_CHECK_TRAVEL) && (position<s->minimum || position>s->maximum)) ||
        ((!N_PULSE_HW_FINGER_DIAGNOSTIC || N_PULSE_DIAG_CHECK_STEP) &&
         (fabsf(position-m.measured_position)>s->step_limit || fabsf(position-m.reference_position)>s->step_limit)) ||
        !HWCommission_Ready()) return false;
    if (*pwm > s->pwm_limit) *pwm = s->pwm_limit;
    return *pwm > 0;
}
uint32_t HWCommission_Execute(const hw_command_t *c, uint32_t ms, uint32_t epoch) {
    if (!c) return HW_PARAMETERS;
    uint32_t key = Motor_Lock(), now = HAL_GetTick();
    uint32_t result = HW_OK;
    if (!CommsSession_IsFreshLocked(now)) result = HW_SESSION;
    else if (epoch != MotorSafety_EpochLocked() || c->epoch != epoch ||
             (uint32_t)(now-ms) > 250U || !c->request_id || c->request_id <= last_request) result = HW_STALE;
    else if (c->motor_id>4U ||
             (c->motor_id!=(uint32_t)HWCommission_SelectedMotor() &&
              !(N_PULSE_HW_SELECTABLE_FINGER && c->operation==HW_SELECT_FINGER))) result=HW_WRONG_MOTOR;
#if N_PULSE_OLD_HW
    else if (!BoardLink_IsLocalMotor((motor_id_t)c->motor_id) &&
             !(N_PULSE_HW_SELECTABLE_FINGER && c->operation==HW_SELECT_FINGER)) result = HW_WRONG_MOTOR;
#endif
    if (result != HW_OK) { Motor_Unlock(key); return result; }
    last_request = c->request_id; /* Every admissible request is consumed once. */
    motor_id_t id = (motor_id_t)c->motor_id;
    settings_t *s=&settings[id];
    bool enabled = Motor_IsEnabled(id), armed = Motor_DRV8848_IsArmed();
    switch (c->operation) {
    case 1: /* Physically verified, operator-provided limits. Never persisted. */
        if (enabled || armed) { result = HW_DISABLE_FIRST; break; }
        if (!isfinite(c->minimum_rad) || !isfinite(c->maximum_rad) ||
            !isfinite(c->maximum_step_rad) || !isfinite(c->reference_rate_rad_s) ||
            c->minimum_rad < 0 || c->maximum_rad > 1.5707963267948966f || c->minimum_rad >= c->maximum_rad ||
            c->maximum_step_rad <= 0 || c->maximum_step_rad > N_PULSE_HW_TEST_STEP_CAP_RAD ||
            c->reference_rate_rad_s <= 0 || c->reference_rate_rad_s > 3.14159265f ||
            !c->pwm_percent || c->pwm_percent > N_PULSE_HW_TEST_PWM_CAP ||
            (!N_PULSE_HW_FINGER_DIAGNOSTIC && N_PULSE_HW_SELECTABLE_FINGER && id==MOTOR_THUMB && c->pwm_percent>5U)) { result = HW_PARAMETERS; break; }
        s->minimum=c->minimum_rad; s->maximum=c->maximum_rad; s->step_limit=c->maximum_step_rad;
        s->rate=c->reference_rate_rad_s; s->pwm_limit=(uint8_t)c->pwm_percent; s->configured=true; break;
    case 2: {
        if (enabled || armed) { result=HW_DISABLE_FIRST; break; }
        if (c->quadrature > 4 || (c->direction != -1 && c->direction != 1)) { result=HW_PARAMETERS; break; }
        encoder_calibration_t cal = {c->cpr,(uint8_t)c->quadrature,c->gear_ratio,(int8_t)c->direction,c->zero_offset};
        if (!Encoder_BSP_SetCalibration(id,&cal)) result=HW_CALIBRATION;
        break;
    }
    case 3:
        if (enabled || armed) result=HW_DISABLE_FIRST;
        else if (!Encoder_BSP_Reset(id)) result=HW_REJECTED;
        break;
    case 4: {
        motor_recovery_ticket_t ticket=Motor_RecoveryTicketLocked();
        Motor_Unlock(key);
        return Motor_RecoverFromFaultWithTicket(ticket) ? HW_OK : HW_FAULT;
    }
    case 5:
        if (!s->configured) result=HW_PARAMETERS;
        else if (!HW_DIAGNOSTIC_TIMED && !Encoder_BSP_IsCalibrated(id)) result=HW_CALIBRATION;
        else if (!HWCommission_Ready()) result=HW_FEEDBACK;
        else if (!MotorSafety_CanArmLocked()) result=HW_FAULT;
        else {
            Motor_Unlock(key);
            return Motor_DRV8848_Arm() ? HW_OK : HW_REJECTED;
        }
        break;
    case 6:
        if (!HWCommission_Ready()) result=HW_FEEDBACK;
        else if (!armed) result=HW_ARM_FIRST;
        else if (!Motor_Enable(id)) result=HW_REJECTED;
        break;
    case 7: {
        uint8_t pwm=(uint8_t)c->pwm_percent;
        if (!enabled) result=HW_ENABLE_FIRST;
        else if (c->pwm_percent > s->pwm_limit || !HWCommission_FilterTarget(c->target_rad,&pwm)) result=HW_PARAMETERS;
        else if (!Motor_SetTargetAtEpoch(id,c->target_rad,pwm,epoch)) result=HW_STALE;
        break;
    }
    case 8: Motor_Disable(id); break;
    case 9:
        if(enabled || armed) result=HW_DISABLE_FIRST;
        else if(!MotorEstimation_Reset(id,c->stationary_reference_verified)) result=HW_REJECTED;
        break;
    case 10: MotorEstimation_Stop(id); break;
    case HW_DIAGNOSTIC_POSITION:
        if(enabled || armed) result=HW_DISABLE_FIRST;
        else if(!Motor_DiagnosticSetPosition(id,c->target_rad)) result=HW_REJECTED;
        break;
#if N_PULSE_OLD_HW
    case 11: /* Existing stationary_reference_verified bool is an explicit operator attestation here. */
        if (enabled || armed) result = HW_DISABLE_FIRST;
        else if (!c->stationary_reference_verified || !HWCommission_Ready()) result = HW_REJECTED;
        else { output_preflight = true; preflight_epoch = epoch; }
        break;
#endif
#if N_PULSE_HW_SELECTABLE_FINGER
    case HW_SELECT_FINGER:
        /* Selecting is a STOP on this board, never a recovery/arm operation.
         * MASTER relays the same operation and waits for the peer's reply. */
        MotorSafety_RequestStopCauseLocked(MOTOR_STOP_MANUAL,MOTOR_COUNT);
        selected_motor=id;
        output_preflight=false;
        break;
#endif
    default: result=HW_PARAMETERS; break;
    }
    Motor_Unlock(key); return result;
}
#else
bool HWCommission_Configured(void) { return false; }
bool HWCommission_MeasurementAllowed(float position) { (void)position; return false; }
bool HWCommission_Ready(void) { return false; }
float HWCommission_ReferenceRate(void) { return 0; }
float HWCommission_PwmCeiling(void) { return 0; }
bool HWCommission_FilterTarget(float position, uint8_t *pwm) { (void)position; (void)pwm; return false; }
uint32_t HWCommission_Execute(const hw_command_t *c, uint32_t ms, uint32_t epoch) {
    (void)c; (void)ms; (void)epoch; return HW_PRODUCTION;
}
#endif
