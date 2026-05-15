/*
 * rx.cpp
 *
 *  Created on: 5 Apr 2026
 *      Author: Roman Danylovych
 */

#include <stdio.h>
#include <cstdint>
#include <cstring>

#include <WriteBufferFixedSize.h>
#include <ReadBufferFixedSize.h>

#include "rx.h"
#include "protocol_messages.h"


bool ReceiveMessage(uint8_t* buffer, uint8_t* length) {
    uint8_t byte;
    if (length == nullptr || buffer == nullptr) return false;

    // 1. Look for Sync Byte (Non-blocking check for 0xAA)
    if (HAL_UART_Receive(&hcom_uart[COM1], &byte, 1, 0) != HAL_OK || byte != SYNC_BYTE) {
        return false;
    }

    // 2. Get Length (Wait up to 10ms for the next byte)
    if (HAL_UART_Receive(&hcom_uart[COM1], length, 1, 10) != HAL_OK) return false;
    if (*length > RX_BUFFER_SIZE) return false;

    // 3. Get Payload (Wait up to 100ms for the full data)
    if (HAL_UART_Receive(&hcom_uart[COM1], buffer, *length, 100) != HAL_OK) return false;

    return true;
}

rx_result_t HandleDeviceMessage(uint8_t* buffer, uint8_t length) {
    rx_result_t result = { .type = RX_TYPE_NONE };
    EmbeddedProto::ReadBufferFixedSize<RX_BUFFER_SIZE> buffer_in;
    
    memcpy(buffer_in.get_data(), buffer, length);
    buffer_in.set_bytes_written(length);

    DeviceMessage msg;
    if (msg.deserialize(buffer_in) != EmbeddedProto::Error::NO_ERRORS) return result;

    switch (msg.get_which_message()) {
        case DeviceMessage::FieldNumber::DATA: {
            auto& data_layer = msg.get_data();
            if (data_layer.get_which_message() == Data::FieldNumber::INSTRUCTION) {
                result.type = RX_TYPE_ACTION;
                result.data.action_id = (uint32_t)data_layer.get_instruction().get_action();
            }
            break;
        }
        case DeviceMessage::FieldNumber::HELLO:
            result.type = RX_TYPE_HELLO;
            result.data.device_id = msg.get_hello().get_device_id();
            break;
        default: break;
    }
    return result;
}

rx_result_t HandleProsthesisMessage(uint8_t* buffer, uint8_t length) {
    rx_result_t result = { .type = RX_TYPE_NONE };
    EmbeddedProto::ReadBufferFixedSize<RX_BUFFER_SIZE> buffer_in;
    
    uint8_t* proto_data = buffer_in.get_data();
    memcpy(proto_data, buffer, (length > RX_BUFFER_SIZE) ? RX_BUFFER_SIZE : length);
    buffer_in.set_bytes_written(length);

    ProsthesisMessage<RX_BUFFER_SIZE> msg;
    if (msg.deserialize(buffer_in) != EmbeddedProto::Error::NO_ERRORS) return result;

    switch (msg.get_which_message()) {
        case ProsthesisMessage<RX_BUFFER_SIZE>::FieldNumber::CONFIG:
            result.type = RX_TYPE_CONFIG;
            result.data.mode = msg.get_config().get_selected_mode();
            break;
        case ProsthesisMessage<RX_BUFFER_SIZE>::FieldNumber::ERROR:
            result.type = RX_TYPE_ERROR;
            result.data.error_code = msg.get_error().get_reason();
            break;
        default: break;
    }
    return result;
}
