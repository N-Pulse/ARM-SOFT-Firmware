#include <cstring>
#include <ReadBufferFixedSize.h>
#include "rx.h"
#include "protocol_messages.h"

rx_result_t HandleDeviceMessage(const uint8_t *bytes, size_t length)
{
    rx_result_t result = {};
    if (!bytes || !length || length > RX_BUFFER_SIZE) return result;
    lifecycle_control_t control = {};
    int envelope = Lifecycle_DecodeDevice(bytes, length, &control);
    if (envelope < 0) return result;
    if (envelope == 1) { result.type = RX_TYPE_CONTROL; result.control = control; return result; }
    EmbeddedProto::ReadBufferFixedSize<RX_BUFFER_SIZE> in;
    memcpy(in.get_data(), bytes, length);
    in.set_bytes_written(length);
    DeviceMessage msg;
    uint8_t trailing;
    if (msg.deserialize(in) != EmbeddedProto::Error::NO_ERRORS || in.peek(trailing)) return result;
    if (msg.get_which_message() == DeviceMessage::FieldNumber::DATA &&
        msg.get_data().get_which_message() == Data::FieldNumber::INSTRUCTION) {
        uint32_t action = static_cast<uint32_t>(msg.get_data().get_instruction().get_action());
        if (action <= 16U) { result.type = RX_TYPE_ACTION; result.data.action_id = action; }
    } else if (msg.get_which_message() == DeviceMessage::FieldNumber::HELLO) {
        result.type = RX_TYPE_HELLO;
        result.proto_version = msg.get_hello().get_proto_ver();
        result.data.device_id = msg.get_hello().get_device_id();
    }
    return result;
}
rx_result_t HandleProsthesisMessage(const uint8_t *bytes, size_t length)
{
    rx_result_t result = {};
    if (!bytes || !length || length > RX_BUFFER_SIZE) return result;
    EmbeddedProto::ReadBufferFixedSize<RX_BUFFER_SIZE> in;
    memcpy(in.get_data(), bytes, length);
    in.set_bytes_written(length);
    ProsthesisMessage<RX_BUFFER_SIZE> msg;
    uint8_t trailing;
    if (msg.deserialize(in) != EmbeddedProto::Error::NO_ERRORS || in.peek(trailing)) return result;
    switch (msg.get_which_message()) {
    case ProsthesisMessage<RX_BUFFER_SIZE>::FieldNumber::CONFIG:
        result.type = RX_TYPE_CONFIG; result.data.mode = msg.get_config().get_selected_mode(); break;
    case ProsthesisMessage<RX_BUFFER_SIZE>::FieldNumber::ERROR:
        result.type = RX_TYPE_ERROR; result.data.error_code = msg.get_error().get_reason(); break;
    default: break;
    }
    return result;
}
