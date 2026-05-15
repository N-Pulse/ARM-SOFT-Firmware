#ifndef BOARD_LINK_H
#define BOARD_LINK_H

#include <stdint.h>
#include <stdbool.h>
#include "motor_control.h"

/*
 * board_link — UART link between motherboard (master) and daughterboard
 * (slave). Wire-level layout:
 *
 *   [0xAA][LEN=4][motor_id:1][pos_q15_h:1][pos_q15_l:1][speed_pct:1]
 *
 * pos_q15 = round(position_rad * 32767 / pi), big-endian on the wire.
 *
 * The two boards run the same firmware binary; role is selected at boot
 * by the strap pin (PC0, internal pull-up — HIGH = master, GND = slave).
 *
 * Pins (USART3, AF7):
 *   PB10 = TX
 *   PB11 = RX
 */

typedef enum {
    BOARD_ROLE_MASTER = 0,
    BOARD_ROLE_SLAVE  = 1,
} board_role_t;

void          BoardLink_Init(void);
board_role_t  BoardLink_Role(void);

/* True if this board is the one physically wired to the given motor.
 * Master (motherboard) owns WRIST_X, WRIST_Y, LITTLE (poignet couplé groupé).
 * Slave  (daughterboard) owns INDEX, MIDDLE, RING, THUMB, PALM. */
bool          BoardLink_IsLocalMotor(motor_id_t id);

/* Master-only: encode + transmit a motor command frame to the slave.
 * Safe to call from any FreeRTOS task context (not ISR). */
void          BoardLink_SendMotorCmd(motor_id_t id,
                                     float       target_rad,
                                     uint8_t     speed_percent);

/* Slave-only: spawn the RX task that drains the USART3 ring buffer and
 * dispatches motor commands locally. No-op when called on master. */
void          BoardLink_TaskCreate(void);

#endif /* BOARD_LINK_H */
