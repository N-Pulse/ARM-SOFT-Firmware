#ifndef BATTERY_H
#define BATTERY_H
#include <stdbool.h>
#include <stdint.h>
typedef enum { BATTERY_UNKNOWN, BATTERY_UNCONFIGURED, BATTERY_NORMAL,
    BATTERY_LOW, BATTERY_STALE, BATTERY_INVALID } battery_state_t;
typedef struct {
    float pack_voltage_v;
    uint32_t timestamp_ms,sequence,warning_revision;
    battery_state_t state;
    bool voltage_valid,low_warning;
} battery_snapshot_t;
void Battery_Init(void);
/* Actual whole-pack volts, supplied by a board driver/fuel gauge. */
bool Battery_PublishVoltage(float pack_voltage_v,uint32_t timestamp_ms);
/* Optional 12-bit Batt_lvl ADC input; requires configured VDDA/divider. */
bool Battery_PublishRaw(uint16_t raw,uint32_t timestamp_ms);
/* Board hook: whole-pack volts and original acquisition time, no blocking.
 * Default reads existing ADC4 rank 2 when DMA acquisition is already enabled;
 * it never starts/reconfigures shared current ADCs for battery monitoring. */
bool Battery_ReadPackVoltage(float *voltage_v,uint32_t *timestamp_ms);
void Battery_Service(void);
void Battery_GetSnapshot(battery_snapshot_t *out);
#endif
