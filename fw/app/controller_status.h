#ifndef CONTROLLER_STATUS_H
#define CONTROLLER_STATUS_H
#include "battery.h"
#include "motor_safety.h"
enum { CONTROLLER_STATUS_SAFETY_FAULT=1U, CONTROLLER_STATUS_BATTERY_LOW=2U,
       CONTROLLER_STATUS_BATTERY_UNAVAILABLE=4U };
/* Shared wire/LED convention: zero is healthy, nonzero describes a fault or
 * unavailable battery monitoring. No motor policy is applied here. */
static inline uint8_t ControllerStatus_Read(void) {
    battery_snapshot_t battery; Battery_GetSnapshot(&battery);
    uint8_t flags=MotorSafety_IsFaultActive() ? CONTROLLER_STATUS_SAFETY_FAULT : 0U;
    if(battery.low_warning) flags|=CONTROLLER_STATUS_BATTERY_LOW;
    if((!battery.voltage_valid && !battery.soc_valid) || (battery.state!=BATTERY_NORMAL && battery.state!=BATTERY_LOW))
        flags|=CONTROLLER_STATUS_BATTERY_UNAVAILABLE;
    return flags;
}
#endif
