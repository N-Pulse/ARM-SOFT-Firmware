#include "motor_estimation.h"
#include "motor_kalman_config.h"
#include "motor_lock.h"
#include "motor_backend_state.h"
#include "adc_dma_bsp.h"
static motor_kalman_t states[MOTOR_COUNT];
static uint32_t revision[MOTOR_COUNT];
void MotorEstimation_Init(void) {
    for(unsigned i=0;i<MOTOR_COUNT;++i) { MotorKalman_Init(&states[i],&g_motor_kalman_config[i]); revision[i]=0; }
}
bool MotorEstimation_GetSnapshot(motor_id_t id,motor_kalman_t *out) {
    if((unsigned)id>=MOTOR_COUNT || !out) return false;
    uint32_t key=Motor_Lock(); *out=states[id]; Motor_Unlock(key); return true;
}
bool MotorEstimation_Configure(motor_id_t id,const motor_kalman_config_t *config) {
    if((unsigned)id>=MOTOR_COUNT || !MotorKalman_Validate(config)) return false;
    uint32_t key=Motor_Lock();
    if(Motor_IsEnabled(id) || MotorBackend_IsArmed()) { Motor_Unlock(key); return false; }
    MotorKalman_Init(&states[id],config); ++revision[id]; Motor_Unlock(key); return true;
}
bool MotorEstimation_Reset(motor_id_t id,bool verified) {
    if((unsigned)id>=MOTOR_COUNT) return false;
    motor_state_t m; uint32_t key=Motor_Lock(); Motor_GetSnapshot(id,&m);
    bool ok=!MotorBackend_IsArmed() && Motor_HasFreshPositionFeedback(id) &&
        MotorKalman_Reset(&states[id],m.measured_position*states[id].config.encoder_to_motor_ratio,m.feedback_ms,!m.enabled,verified,verified);
    if(ok) ++revision[id];
    Motor_Unlock(key); return ok;
}
void MotorEstimation_Stop(motor_id_t id) {
    if((unsigned)id>=MOTOR_COUNT) return;
    uint32_t key=Motor_Lock(); states[id].initialized=false; states[id].status=KALMAN_NEEDS_REFERENCE;
    ++revision[id]; Motor_Unlock(key);
}
void MotorEstimation_Service(void) {
    for(unsigned i=0;i<MOTOR_COUNT;++i) {
        motor_kalman_t prepared; motor_state_t m; adc_current_sample_t current;
        uint32_t key=Motor_Lock(),version=revision[i]; prepared=states[i];
        Motor_GetSnapshot((motor_id_t)i,&m); Motor_Unlock(key);
        if(!prepared.initialized) continue;
        ADC_DMA_BSP_GetSample((motor_id_t)i,&current);
        /* Predict to the encoder sample's timestamp, never re-label an older
         * observation as now. No fabricated SIM current or force conversion. */
        uint32_t now=HAL_GetTick();
        uint32_t sample=m.feedback_valid && (uint32_t)(now-m.feedback_ms)<=100U ? m.feedback_ms : now;
        bool input=current.state==ADC_CURRENT_CALIBRATED && (int32_t)(sample-current.timestamp_ms)>=0;
        if(MotorKalman_Predict(&prepared,sample,current.current_a,input,sample-current.timestamp_ms))
            MotorKalman_Correct(&prepared,m.measured_position*prepared.config.encoder_to_motor_ratio,m.feedback_valid,m.feedback_ms);
        key=Motor_Lock();
        if(version==revision[i]) states[i]=prepared;
        Motor_Unlock(key);
    }
}
