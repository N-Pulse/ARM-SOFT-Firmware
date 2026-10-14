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
static motor_id_t selected = (motor_id_t)N_PULSE_HW_TEST_MOTOR;
void HWTelemetry_Select(uint32_t motor) {
    if (motor >= MOTOR_COUNT) return;
#if N_PULSE_HW_SINGLE_MOTOR_TEST
    if (motor != N_PULSE_HW_TEST_MOTOR) return;
#endif
    uint32_t key=Motor_Lock(); selected=(motor_id_t)motor; Motor_Unlock(key);
}
void HWTelemetry_Reply(uint32_t request, uint32_t result) {
    uint32_t key=Motor_Lock(), epoch=MotorSafety_EpochLocked(), causes=MotorSafety_GetStopCauses();
    Motor_Unlock(key);
    SendCommandReply(request,result,epoch,causes);
}
void HWTelemetry_Service(void) {
    hw_diagnostic_t d={0}; motor_state_t m; encoder_snapshot_t e;
    uint32_t key=Motor_Lock();
    d.timestamp_ms=HAL_GetTick(); d.motor_id=(uint32_t)selected;
    if (!CommsSession_IsFreshLocked(d.timestamp_ms)) { Motor_Unlock(key); return; }
    Motor_GetSnapshot(selected,&m);
    bool valid=Encoder_BSP_GetSnapshot(selected,&e);
    d.epoch=MotorSafety_EpochLocked(); d.faults=MotorSafety_GetStopCauses();
    d.flags=(m.enabled?1U:0U) | (Motor_DRV8848_IsArmed()?2U:0U) |
        ((valid && m.feedback_valid && (uint32_t)(d.timestamp_ms-m.feedback_ms)<=100U)?4U:0U) |
        (Encoder_BSP_IsCalibrated(selected)?8U:0U) | (N_PULSE_HW_SINGLE_MOTOR_TEST?16U:0U) |
        (HWCommission_Configured()?32U:0U);
    d.raw_count=e.raw_count; d.encoder_rad=valid?e.position_rad:NAN; d.velocity_rad_s=valid?e.velocity_rad_s:NAN;
    d.target_rad=m.target_position; d.reference_rad=m.reference_position; d.pwm_percent=m.output_pwm;
    Motor_Unlock(key);
    d.adc_voltage=d.current_a=d.kalman_rad=d.kalman_velocity_rad_s=NAN;
    d.adc_age_ms=UINT32_MAX;
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
