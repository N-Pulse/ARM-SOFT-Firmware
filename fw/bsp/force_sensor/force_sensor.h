#ifndef FORCE_SENSOR_H
#define FORCE_SENSOR_H
#include "motor_control.h"
#include <stdint.h>
#include "../../app/board_hardware_config.h"

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
/* Task-context adapter, outside Motor_Lock. Wiring/ADC IDs are defined in
 * board_hardware_config.h. Acquisition still requires a shared ADC scan owner;
 * do not reconfigure ADC1/2 concurrently with motor-current DMA. */
bool ForceSensor_ReadVoltage(motor_id_t id,float *voltage_v,uint32_t *timestamp_ms);
void ForceSensor_Service(void);
#endif
