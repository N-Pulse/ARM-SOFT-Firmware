/* These benches start after startup and do not simulate homing mechanics.
 * Disabled controllers must never acquire homing output permission here. */
#include "boot_homing.h"
bool BootHoming_OutputAllowedLocked(motor_id_t id,float pwm) {
    (void)id; (void)pwm; return false;
}
