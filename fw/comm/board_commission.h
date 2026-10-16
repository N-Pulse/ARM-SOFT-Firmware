#ifndef BOARD_COMMISSION_H
#define BOARD_COMMISSION_H
#include "board_protocol.h"
#include "hw_commission.h"
enum { BC_HELLO=1, BC_COMMAND, BC_REPLY };
/* Explicit field encoding: no structure padding, pointer, or native endianness on wire. */
uint8_t BoardCommission_Encode(const hw_command_t *, uint8_t out[BL_PAYLOAD]);
bool BoardCommission_Decode(const uint8_t *, uint8_t length, hw_command_t *);
#endif
