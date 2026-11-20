#ifndef BATTERY_H
#define BATTERY_H
#include <stdbool.h>
#include <stdint.h>
typedef enum { BATTERY_UNKNOWN, BATTERY_UNCONFIGURED, BATTERY_NORMAL,
    BATTERY_LOW, BATTERY_STALE, BATTERY_INVALID } battery_state_t;
typedef enum { BATTERY_SHUTDOWN_IDLE, BATTERY_SHUTDOWN_COUNTDOWN,
    BATTERY_SHUTDOWN_STOPPED } battery_shutdown_state_t;
typedef struct {
    float pack_voltage_v;
    uint32_t timestamp_ms,sequence,warning_revision;
    battery_state_t state;
    bool voltage_valid,low_warning;
    float state_of_charge_percent;
    uint32_t soc_timestamp_ms,soc_sequence;
    bool soc_valid;
    battery_shutdown_state_t shutdown_state;
    uint32_t shutdown_started_ms;
    bool fast_warning,power_off_accepted;
} battery_snapshot_t;
void Battery_Init(void);
/* Actual whole-pack volts, supplied by a board driver/fuel gauge. */
bool Battery_PublishVoltage(float pack_voltage_v,uint32_t timestamp_ms);
/* Optional 12-bit Batt_lvl ADC input; requires configured VDDA/divider. */
bool Battery_PublishRaw(uint16_t raw,uint32_t timestamp_ms);
/* Measured/calibrated state of charge, 0..100 percent. Never inferred from
 * nominal pack voltage. The source must preserve the acquisition timestamp. */
bool Battery_PublishStateOfCharge(float percent,uint32_t timestamp_ms);
bool Battery_ReadStateOfCharge(float *percent,uint32_t *timestamp_ms);
/* Called once, in task context, after the critical shutdown motor stop has
 * completed. Return true only if a real power-off request was accepted.
 * Default false: no verified STM32 power-latch/eFuse control pin is assigned. */
bool Battery_BoardRequestPowerOff(void);
/* Board hook: whole-pack volts and original acquisition time, no blocking.
 * Default reads existing ADC4 rank 2 when DMA acquisition is already enabled;
 * it never starts/reconfigures shared current ADCs for battery monitoring. */
bool Battery_ReadPackVoltage(float *voltage_v,uint32_t *timestamp_ms);
void Battery_Service(void);
void Battery_GetSnapshot(battery_snapshot_t *out);
#endif
