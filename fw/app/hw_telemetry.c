#include "hw_telemetry.h"
#ifndef MOTOR_BACKEND_SIM
#include "hw_commission.h"
#include "hw_test_config.h"
#include "encoder_bsp.h"
#include "motor_safety.h"
#include "motor_lock.h"
#include "comms_session.h"
#include "drv8848.h"
#include "tx.h"
#include "adc_dma_bsp.h"
#include "motor_estimation.h"
#include <math.h>
#if N_PULSE_OLD_HW
#include "old_hw.h"
#include "l298n_config.h"
#if N_PULSE_DUAL_BOARD_LINK
#include "board_link.h"
#endif
#endif
static motor_id_t selected = (motor_id_t)N_PULSE_HW_TEST_MOTOR;
static bool link_selected;
void HWTelemetry_Select(uint32_t motor) {
#if N_PULSE_DUAL_BOARD_LINK
    if (motor==MOTOR_COUNT) { uint32_t key=Motor_Lock(); link_selected=true; Motor_Unlock(key); return; }
#endif
    if (motor >= MOTOR_COUNT) return;
#if N_PULSE_HW_SINGLE_MOTOR_TEST
    if (motor != N_PULSE_HW_TEST_MOTOR) return;
#endif
    uint32_t key=Motor_Lock(); link_selected=false; selected=(motor_id_t)motor; Motor_Unlock(key);
}
void HWTelemetry_Reply(uint32_t request, uint32_t result) {
    uint32_t key=Motor_Lock(), epoch=MotorSafety_EpochLocked(), causes=MotorSafety_GetStopCauses();
    Motor_Unlock(key);
    SendCommandReply(request,result,epoch,causes);
}
void HWTelemetry_Service(void) {
    hw_diagnostic_t d={0}; motor_state_t m;
    encoder_snapshot_t e={.position_rad=NAN,.velocity_rad_s=NAN};
    uint32_t key=Motor_Lock();
    d.timestamp_ms=HAL_GetTick(); d.motor_id=(uint32_t)selected;
    if (!CommsSession_IsFreshLocked(d.timestamp_ms)) { Motor_Unlock(key); return; }
#if N_PULSE_DUAL_BOARD_LINK
    if (link_selected) {
        link_diagnostic_t status; BoardLink_GetDiagnostic(&status); Motor_Unlock(key);
        SendLinkDiagnostic(&status); return;
    }
#endif
    Motor_GetSnapshot(selected,&m);
    bool valid=Encoder_BSP_GetSnapshot(selected,&e);
    if (Encoder_BSP_RawAvailable(selected)) d.flags |= 128U;
    d.epoch=MotorSafety_EpochLocked(); d.faults=MotorSafety_GetStopCauses();
    d.flags|=(m.enabled?1U:0U) | (Motor_DRV8848_IsArmed()?2U:0U) |
        ((valid && m.feedback_valid && (uint32_t)(d.timestamp_ms-m.feedback_ms)<=100U)?4U:0U) |
        (Encoder_BSP_IsCalibrated(selected)?8U:0U) | (N_PULSE_HW_SINGLE_MOTOR_TEST?16U:0U) |
        (HWCommission_Configured()?32U:0U);
#if N_PULSE_OLD_HW
    /* Additive diagnostic flags; existing protobuf fields and low bits unchanged. */
    d.flags |= g_old_hw_role==BOARD_ROLE_MASTER ? 1U<<16 : 1U<<17;
    if (!N_PULSE_OLD_HW_MOTION_OPT_IN) d.flags |= 1U<<18;
    if (selected==MOTOR_THUMB && !N_PULSE_THUMB_EXTI_MOTION_QUALIFIED) d.flags |= 1U<<22;
    if (HWCommission_OutputPreflightReady()) d.flags |= 1U<<20;
    if (!Encoder_BSP_IsCalibrated(selected)) d.flags |= 1U<<19;
#endif
    d.raw_count=e.raw_count; d.encoder_rad=valid?e.position_rad:NAN; d.velocity_rad_s=valid?e.velocity_rad_s:NAN;
    d.target_rad=m.target_position; d.reference_rad=m.reference_position; d.pwm_percent=m.output_pwm;
#if N_PULSE_OLD_HW
    d.pwm_percent=Motor_L298N_AppliedPWM(selected);
#endif
    Motor_Unlock(key);
    d.adc_voltage=d.current_a=d.kalman_rad=d.kalman_velocity_rad_s=NAN;
    d.adc_age_ms=UINT32_MAX;
#if N_PULSE_DUAL_BOARD_LINK
    if (!BoardLink_IsLocalMotor((motor_id_t)d.motor_id)) {
        bl_feedback_t remote;
        d.flags &= ~(15U|32U|128U|(1U<<20));
        d.flags |= (1U<<21) | (255U<<24); /* Remote, age unavailable initially. */
        d.encoder_rad=d.velocity_rad_s=d.target_rad=d.reference_rad=d.pwm_percent=NAN;
        d.raw_count=0;
        if (BoardLink_GetRemote((motor_id_t)d.motor_id,&remote)) {
            d.flags |= (remote.flags&BL_ENABLED?1U:0U) | (remote.flags&BL_ARMED?2U:0U) |
                (remote.flags&BL_VALID?4U:0U) | (remote.flags&BL_CALIBRATED?8U:0U) |
                (remote.flags&BL_RAW_VALID?128U:0U);
            d.flags = (d.flags & 0x00FFFFFFU) | ((remote.age_ms<255U?remote.age_ms:255U)<<24);
            d.flags &= ~(1U<<19);
            if (!(remote.flags&BL_VALID)) d.flags |= 1U<<19;
            d.raw_count=remote.raw_count;
            if (remote.flags&BL_VALID) { d.encoder_rad=remote.position_mrad/1000.0f; d.velocity_rad_s=remote.velocity_mrad_s/1000.0f; }
            d.target_rad=remote.target_mrad/1000.0f; d.pwm_percent=remote.pwm_milli/1000.0f;
            d.faults |= remote.faults;
            /* timestamp_ms stays in the reporting board's clock domain.
             * flags[31:24] carry saturated remote age in ms, 255=old/unknown. */
        }
        SendMotorDiagnostic(&d); return;
    }
#endif
    adc_current_sample_t current;
    if(ADC_DMA_BSP_GetSample((motor_id_t)d.motor_id,&current)) {
        d.adc_raw=current.raw; d.adc_voltage=current.voltage_v; d.current_a=current.current_a;
        d.adc_state=current.state; d.adc_error=current.error; d.adc_age_ms=current.age_ms;
        if(current.active) d.flags|=64U;
    }
    motor_kalman_t estimate;
    if(MotorEstimation_GetSnapshot((motor_id_t)d.motor_id,&estimate)) {
        d.kalman_status=estimate.status;
        if(MotorKalman_IsValid(&estimate)) {
            d.kalman_rad=(float)estimate.position; d.kalman_velocity_rad_s=(float)estimate.velocity;
        }
    }
    SendMotorDiagnostic(&d); /* Serialization and zero-wait offer outside lock. */
}
#else
void HWTelemetry_Select(uint32_t motor) { (void)motor; }
void HWTelemetry_Service(void) {}
void HWTelemetry_Reply(uint32_t request, uint32_t result) { (void)request; (void)result; }
#endif
