#ifndef RX_H
#define RX_H

#include <stdint.h>
#include <stdbool.h>

#include "stm32g4xx_hal.h"
#include "stm32g4xx_nucleo.h"

#define RX_BUFFER_SIZE 128
#define SYNC_BYTE 0xAA

typedef enum {
    RX_TYPE_NONE = 0,
    RX_TYPE_ACTION,
    RX_TYPE_HELLO,
    RX_TYPE_CONFIG,
    RX_TYPE_ERROR
} rx_msg_type_t;

typedef struct {
    rx_msg_type_t type;
    union {
        uint32_t action_id;
        uint32_t device_id;
        uint32_t error_code;
        uint32_t mode;
    } data;
} rx_result_t;

#ifdef __cplusplus
extern "C" {
#endif

// 1. The Packetizer: Finds [0xAA][Len][Payload] in the UART stream
//    `length` is an out-pointer (was a C++ reference; changed so this header
//    can be included from C translation units like comm.c).
bool ReceiveMessage(uint8_t* buffer, uint8_t* length);

// 2. The Parsers: Turns raw bytes into the C struct
rx_result_t HandleDeviceMessage(uint8_t* buffer, uint8_t length);
rx_result_t HandleProsthesisMessage(uint8_t* buffer, uint8_t length);

#ifdef __cplusplus
}
#endif

#endif
