#ifndef BOOT_HOMING_H
#define BOOT_HOMING_H
#include "motor_control.h"
bool BootHoming_RunOnce(void);
bool BootHoming_Done(void);
bool BootHoming_OutputAllowedLocked(motor_id_t id, float pwm);
#endif
