#include "hw_commission.h"
#include "hw_test_config.h"
#include "motor_safety.h"
#include "motor_lock.h"
#include "comms_session.h"
#include "encoder_bsp.h"
#include "drv8848.h"
#include "motor_estimation.h"
#include <math.h>
#if N_PULSE_OLD_HW
#include "board_link.h"
static bool output_preflight;
static uint32_t preflight_epoch;
bool HWCommission_OutputPreflightReady(void) {
    return output_preflight && preflight_epoch == MotorSafety_EpochLocked();
}
#endif
#if N_PULSE_HW_SINGLE_MOTOR_TEST
static bool configured;
static float minimum, maximum, step_limit, rate;
static uint8_t pwm_limit;
static uint32_t last_request;
bool HWCommission_Configured(void) { return configured; }
bool HWCommission_MeasurementAllowed(float position) {
    return configured && isfinite(position) && position>=minimum && position<=maximum;
}
float HWCommission_ReferenceRate(void) { return rate; }
float HWCommission_PwmCeiling(void) { return configured ? pwm_limit : 0; }
bool HWCommission_Ready(void) {
    uint32_t key = Motor_Lock();
    motor_state_t m;
    motor_id_t id = (motor_id_t)N_PULSE_HW_TEST_MOTOR;
#if N_PULSE_OLD_HW
    if (!BoardLink_IsLocalMotor(id)) { Motor_Unlock(key); return false; }
#endif
    Motor_GetSnapshot(id, &m);
    bool ok = configured && CommsSession_IsFreshLocked(HAL_GetTick()) &&
        Encoder_BSP_IsCalibrated(id) && Motor_HasFreshPositionFeedback(id) &&
        m.measured_position >= minimum && m.measured_position <= maximum;
    Motor_Unlock(key); return ok;
}
bool HWCommission_FilterTarget(float position, uint8_t *pwm) {
    motor_state_t m; Motor_GetSnapshot((motor_id_t)N_PULSE_HW_TEST_MOTOR, &m);
    if (!pwm || !configured || !isfinite(position) || position < minimum || position > maximum ||
        fabsf(position - m.measured_position) > step_limit ||
        fabsf(position - m.reference_position) > step_limit || !HWCommission_Ready()) return false;
    if (*pwm > pwm_limit) *pwm = pwm_limit;
    return *pwm > 0;
}
uint32_t HWCommission_Execute(const hw_command_t *c, uint32_t ms, uint32_t epoch) {
    if (!c) return HW_PARAMETERS;
    uint32_t key = Motor_Lock(), now = HAL_GetTick();
    uint32_t result = HW_OK;
    if (!CommsSession_IsFreshLocked(now)) result = HW_SESSION;
    else if (epoch != MotorSafety_EpochLocked() || c->epoch != epoch ||
             (uint32_t)(now-ms) > 250U || !c->request_id || c->request_id <= last_request) result = HW_STALE;
    else if (c->motor_id != N_PULSE_HW_TEST_MOTOR) result = HW_WRONG_MOTOR;
#if N_PULSE_OLD_HW
    else if (!BoardLink_IsLocalMotor((motor_id_t)c->motor_id)) result = HW_WRONG_MOTOR;
#endif
    if (result != HW_OK) { Motor_Unlock(key); return result; }
    last_request = c->request_id; /* Every admissible request is consumed once. */
    motor_id_t id = (motor_id_t)c->motor_id;
    bool enabled = Motor_IsEnabled(id), armed = Motor_DRV8848_IsArmed();
    switch (c->operation) {
    case 1: /* Physically verified, operator-provided limits. Never persisted. */
        if (enabled || armed) { result = HW_DISABLE_FIRST; break; }
        if (!isfinite(c->minimum_rad) || !isfinite(c->maximum_rad) ||
            !isfinite(c->maximum_step_rad) || !isfinite(c->reference_rate_rad_s) ||
            c->minimum_rad < 0 || c->maximum_rad > 1.5707963f || c->minimum_rad >= c->maximum_rad ||
            c->maximum_step_rad <= 0 || c->maximum_step_rad > N_PULSE_HW_TEST_STEP_CAP_RAD ||
            c->reference_rate_rad_s <= 0 || c->reference_rate_rad_s > 3.14159265f ||
            !c->pwm_percent || c->pwm_percent > N_PULSE_HW_TEST_PWM_CAP) { result = HW_PARAMETERS; break; }
        minimum=c->minimum_rad; maximum=c->maximum_rad; step_limit=c->maximum_step_rad;
        rate=c->reference_rate_rad_s; pwm_limit=(uint8_t)c->pwm_percent; configured=true; break;
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
        if (!configured) result=HW_PARAMETERS;
        else if (!Encoder_BSP_IsCalibrated(id)) result=HW_CALIBRATION;
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
        else if (c->pwm_percent > pwm_limit || !HWCommission_FilterTarget(c->target_rad,&pwm)) result=HW_PARAMETERS;
        else if (!Motor_SetTargetAtEpoch(id,c->target_rad,pwm,epoch)) result=HW_STALE;
        break;
    }
    case 8: Motor_Disable(id); break;
    case 9:
        if(enabled || armed) result=HW_DISABLE_FIRST;
        else if(!MotorEstimation_Reset(id,c->stationary_reference_verified)) result=HW_REJECTED;
        break;
    case 10: MotorEstimation_Stop(id); break;
#if N_PULSE_OLD_HW
    case 11: /* Existing stationary_reference_verified bool is an explicit operator attestation here. */
        if (enabled || armed) result = HW_DISABLE_FIRST;
        else if (!c->stationary_reference_verified || !HWCommission_Ready()) result = HW_REJECTED;
        else { output_preflight = true; preflight_epoch = epoch; }
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
