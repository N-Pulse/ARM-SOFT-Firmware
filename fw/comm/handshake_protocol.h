#ifndef HANDSHAKE_PROTOCOL_H
#define HANDSHAKE_PROTOCOL_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { HANDSHAKE_SYNC=0x00, HANDSHAKE_HELLO=0x01,
       HANDSHAKE_ACK=0x02, HANDSHAKE_NACK=0x03,
       HANDSHAKE_SYNC_SIZE=5, HANDSHAKE_HELLO_SIZE=9 };
/* HELLO/reply Len=6 includes four parameters and two CRC bytes.
 * SYNC has the separately specified fixed Len=1. */
uint16_t Handshake_Crc16(const uint8_t *bytes, size_t length);
size_t Handshake_EncodeSync(uint8_t *out, size_t capacity);
bool Handshake_DecodeHello(const uint8_t *frame, size_t length, bool *matches);
size_t Handshake_EncodeReply(uint8_t *out, size_t capacity, uint8_t seq, bool matches);
#endif
