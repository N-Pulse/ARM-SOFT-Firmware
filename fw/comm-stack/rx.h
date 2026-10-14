#ifndef RX_H
#define RX_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "hw_commission.h"


#define RX_BUFFER_SIZE 128
#define SYNC_BYTE 0xAA

typedef enum {
    RX_TYPE_NONE = 0,
    RX_TYPE_ACTION,
    RX_TYPE_HELLO,
    RX_TYPE_CONFIG,
    RX_TYPE_ERROR,
    RX_TYPE_COMMISSION,
    RX_TYPE_STOP,
    RX_TYPE_DIAGNOSTIC
} rx_msg_type_t;

typedef struct {
    rx_msg_type_t type;
    uint32_t proto_version;
    union {
        uint32_t action_id;
        uint32_t device_id;
        uint32_t error_code;
        uint32_t mode;
    } data;
    hw_command_t command;
} rx_result_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Task-context protobuf decoders. Incremental packetizing is in comms_parser. */
rx_result_t HandleDeviceMessage(const uint8_t* buffer, size_t length);
rx_result_t HandleProsthesisMessage(const uint8_t* buffer, size_t length);

#ifdef __cplusplus
}
#endif

#endif
