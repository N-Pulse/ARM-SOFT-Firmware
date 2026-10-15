#include <cstring>
#include <ReadBufferFixedSize.h>
#include "rx.h"
#include "protocol_messages.h"

rx_result_t HandleDeviceMessage(const uint8_t *bytes, size_t length)
{
    rx_result_t result = {};
    if (!bytes || !length || length > RX_BUFFER_SIZE) return result;
    lifecycle_control_t control = {};
    /* Classify the envelope first. Legacy Commission/Stop/Diagnostic fields
     * keep their original tags and are decoded by DeviceMessage below. */
    int envelope = Lifecycle_DecodeDevice(bytes, length, &control);
    if (envelope < 0) return result;
    if (envelope == 1) { result.type = RX_TYPE_CONTROL; result.control = control; return result; }
    EmbeddedProto::ReadBufferFixedSize<RX_BUFFER_SIZE> in;
    memcpy(in.get_data(), bytes, length);
    in.set_bytes_written(length);
    DeviceMessage msg;
    uint8_t trailing;
    if (msg.deserialize(in) != EmbeddedProto::Error::NO_ERRORS || in.peek(trailing)) return result;
    if (msg.get_which_message() == DeviceMessage::FieldNumber::STOP) { result.type=RX_TYPE_STOP; return result; }
    if (msg.get_which_message() == DeviceMessage::FieldNumber::DIAGNOSTIC) {
        result.type=RX_TYPE_DIAGNOSTIC; result.data.mode=msg.get_diagnostic().get_motor_id(); return result;
    }
    if (msg.get_which_message() == DeviceMessage::FieldNumber::COMMISSION) {
        const auto &c=msg.get_commission(); result.type=RX_TYPE_COMMISSION;
        result.command={c.get_request_id(),c.get_epoch(),static_cast<uint32_t>(c.get_operation()),
            c.get_motor_id(),c.get_cpr(),c.get_quadrature(),c.get_pwm_percent(),c.get_direction(),
            c.get_zero_offset(),c.get_gear_ratio(),c.get_minimum_rad(),c.get_maximum_rad(),
            c.get_maximum_step_rad(),c.get_target_rad(),c.get_reference_rate_rad_s(),c.get_stationary_reference_verified()};
        return result;
    }
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
