#ifndef APP_H
#define APP_H
#include "motor_safety.h"
#include "battery.h"

void App_Init(void);
void App_Task(void);
/* Notification after heartbeat timeout has restarted the SYNC handshake. */
void App_OnCommunicationInterrupted(void);
/* Low-battery warning/clear notification in control-task context; no auto stop. */
void App_OnBatteryWarning(bool active,const battery_snapshot_t *snapshot);
/* Outputs are already inhibited. One callback per newly observed cause/motor.
 * Fill the corresponding case in app.c when follow-up policies are defined. */
void App_OnSafetyFault(motor_stop_cause_t cause, uint32_t motor_mask);

#endif // APP_H
