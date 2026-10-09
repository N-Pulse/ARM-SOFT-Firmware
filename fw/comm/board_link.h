#ifndef BOARD_LINK_H
#define BOARD_LINK_H

#include <stdint.h>
#include <stdbool.h>
#include "motor_control.h"

/*
 * Compatibility API retained while callers are migrated away from the
 * historical two-board routing layer. There is one controller board now, so
 * every motor is local and no inter-board transport is performed.
 */
void BoardLink_Init(void);

/* Kept for source compatibility with application code being migrated. */
bool          BoardLink_IsLocalMotor(motor_id_t id);
void          BoardLink_SendMotorCmd(motor_id_t id,
                                     float       target_rad,
                                     uint8_t     speed_percent);
void          BoardLink_TaskCreate(void);

#endif /* BOARD_LINK_H */
