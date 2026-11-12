#ifndef DATA_PROTOCOL_H
#define DATA_PROTOCOL_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define DATA_PACKET_TYPE 0x10U
/* DATA uses the local HELLO lengths; Status is skipped, class vector is the
 * unchanged serialized DeviceMessage DATA envelope, without its old AA header. */
bool Data_Decode(const uint8_t *frame,size_t length,const uint8_t **data,size_t *data_length);
#endif
