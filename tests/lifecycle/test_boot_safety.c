/* Exercise the real pre-RTOS homing implementation, not the app homing stub. */
#include "../comms/stream_fixture.h"
#include "boot_homing.h"
#include "force_sensor.h"
static unsigned force_reads;
bool Encoder_BSP_SetCalibration(motor_id_t id,const encoder_calibration_t *cal) {
    if(Motor_IsEnabled(id) || !cal->counts_per_revolution) return false;
    calibrated[id]=true; return true;
}
bool ForceSensor_ReadVoltage(motor_id_t id,float *v,uint32_t *ms) {
    assert(!primask);
    if(id!=MOTOR_INDEX) return false;
    ++force_reads; *v=0.2f; *ms=clock_ms; return true;
}
int main(void) {
    fixture(); (void)hello; (void)close_hand; (void)inject; drain();
    ForceSensor_Init();
    assert(!BootHoming_RunOnce() && !BootHoming_Done());
    assert(force_reads>0 && (MotorSafety_GetStopCauses()&MOTOR_STOP_FORCE));
    assert(MotorSafety_GetStopCauses()&MOTOR_STOP_INIT);
    assert_stopped(); unsigned before=force_reads;
    assert(!BootHoming_RunOnce() && force_reads==before);
    puts("PASS real boot homing polls force inputs before RTOS, aborts on hard voltage limit, and never retries failed homing");
    return 0;
}
