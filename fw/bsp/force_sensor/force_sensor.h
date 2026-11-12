#ifndef FORCE_SENSOR_H
#define FORCE_SENSOR_H
#include "motor_control.h"
#include <stdint.h>

typedef struct {
    float voltage_v;
    uint32_t timestamp_ms, sequence;
    bool valid;
} force_sensor_sample_t;

void ForceSensor_Init(void);
/* ADC completion/driver input: publish actual volts, never a force estimate.
 * Storage contains one sample per sensor; each publication replaces the old one.
 * High-force protection is evaluated on every sample, even if telemetry skips it. */
bool ForceSensor_PublishVoltage(motor_id_t id,float voltage_v,uint32_t timestamp_ms);
bool ForceSensor_GetLatest(motor_id_t id,force_sensor_sample_t *out);
/* Task-context adapter, outside Motor_Lock. Bind this weak board hook to the
 * actual force ADC channel after wiring/reference voltage have been confirmed. */
bool ForceSensor_ReadVoltage(motor_id_t id,float *voltage_v,uint32_t *timestamp_ms);
void ForceSensor_Service(void);
#endif
