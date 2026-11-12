#include "force_sensor.h"
#include "motor_safety.h"
#include "motor_lock.h"
#include "motor_backend.h"
#include "motor_safety_config.h"
#include <math.h>

static force_sensor_sample_t samples[3];
static bool sensor_index(motor_id_t id,unsigned *index) {
    switch(id) {
    case MOTOR_THUMB: *index=0; return true;
    case MOTOR_INDEX: *index=1; return true;
    case MOTOR_MIDDLE: *index=2; return true;
    default: return false;
    }
}
void ForceSensor_Init(void) {
    uint32_t key=Motor_Lock();
    for(unsigned i=0;i<3;++i) samples[i]=(force_sensor_sample_t){.voltage_v=NAN};
    Motor_Unlock(key);
}
bool ForceSensor_PublishVoltage(motor_id_t id,float voltage,uint32_t ms) {
    unsigned index;
    if(!sensor_index(id,&index)) return false;
    uint32_t key=Motor_Lock(),now=HAL_GetTick();
    force_sensor_sample_t *s=&samples[index];
    /* Do not let an older completion replace the latest measurement. */
    if((int32_t)(now-ms)<0 || (s->sequence && (int32_t)(ms-s->timestamp_ms)<0)) {
        Motor_Unlock(key); return false;
    }
    bool valid=isfinite(voltage) && voltage>=0.0f && (uint32_t)(now-ms)<=MOTOR_FEEDBACK_TIMEOUT_MS;
    uint32_t sequence=s->sequence+1U;
    if(!sequence) sequence=1;
    *s=(force_sensor_sample_t){valid ? voltage : NAN,ms,sequence,valid};
    motor_feedback_t feedback={.id=id,.force_voltage_v=valid ? voltage : NAN,
        .valid_fields=MOTOR_FB_FORCE_VOLTAGE};
    MotorSafety_OnFeedbackAtTime(&feedback,ms);
    Motor_Unlock(key); return valid;
}
bool ForceSensor_GetLatest(motor_id_t id,force_sensor_sample_t *out) {
    unsigned index;
    if(!out || !sensor_index(id,&index)) return false;
    uint32_t key=Motor_Lock(); *out=samples[index]; Motor_Unlock(key);
    if(!out->sequence || (uint32_t)(HAL_GetTick()-out->timestamp_ms)>MOTOR_FEEDBACK_TIMEOUT_MS)
        out->valid=false;
    return out->valid;
}
__attribute__((weak)) bool ForceSensor_ReadVoltage(motor_id_t id,float *voltage,uint32_t *ms) {
    (void)id; (void)voltage; (void)ms;
    return false; /* No force channel is assigned to the existing current ADCs. */
}
void ForceSensor_Service(void) {
    static const motor_id_t ids[]={MOTOR_THUMB,MOTOR_INDEX,MOTOR_MIDDLE};
    for(unsigned i=0;i<3;++i) {
        float voltage; uint32_t ms;
        if(ForceSensor_ReadVoltage(ids[i],&voltage,&ms))
            ForceSensor_PublishVoltage(ids[i],voltage,ms);
    }
}
