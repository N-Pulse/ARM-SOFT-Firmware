#ifndef HW_COMMISSION_H
#define HW_COMMISSION_H
#include "motor_control.h"
typedef struct {
    uint32_t request_id, epoch, operation, motor_id, cpr, quadrature, pwm_percent;
    int32_t direction;
    int64_t zero_offset;
    float gear_ratio, minimum_rad, maximum_rad, maximum_step_rad, target_rad, reference_rate_rad_s;
    bool stationary_reference_verified;
} hw_command_t;
enum { HW_OK, HW_PRODUCTION, HW_SESSION, HW_STALE, HW_WRONG_MOTOR,
       HW_PARAMETERS, HW_DISABLE_FIRST, HW_CALIBRATION, HW_FEEDBACK,
       HW_FAULT, HW_ARM_FIRST, HW_ENABLE_FIRST, HW_REJECTED };
bool HWCommission_Ready(void);
bool HWCommission_Configured(void);
#if N_PULSE_OLD_HW
/* Explicit operator preflight acknowledgement, scoped to the current STOP epoch. */
bool HWCommission_OutputPreflightReady(void);
#endif
bool HWCommission_MeasurementAllowed(float position);
float HWCommission_ReferenceRate(void);
float HWCommission_PwmCeiling(void);
bool HWCommission_FilterTarget(float position, uint8_t *pwm);
uint32_t HWCommission_Execute(const hw_command_t *, uint32_t received_ms, uint32_t epoch);
#endif
