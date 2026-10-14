#include <WriteBufferFixedSize.h>
#include "tx.h"
#include "protocol_messages.h"
extern "C" {
#include "uart4_transport.h"
}
static bool queue_message(const DeviceMessage &msg)
{
    EmbeddedProto::WriteBufferFixedSize<TX_BUFFER_SIZE> out;
    return msg.serialize(out) == EmbeddedProto::Error::NO_ERRORS &&
           UART4Transport_QueuePayload(out.get_data(), out.get_size());
}
bool SendHelloMessage(void)
{
    Hello hello;
    hello.set_device_id(1); hello.set_proto_ver(1); hello.set_supported_modes(1);
    DeviceMessage msg; msg.set_hello(hello);
    return queue_message(msg);
}
bool SendClassificationData(uint8_t action)
{
    if (action > 4U) return false;
    ClassificationData classification;
    classification.set_action(static_cast<ClassificationData::Action>(action));
    Data data; data.set_instruction(classification);
    DeviceMessage msg; msg.set_data(data);
    return queue_message(msg);
}
static bool queue_response(const ProsthesisMessage<TX_BUFFER_SIZE> &msg) {
    EmbeddedProto::WriteBufferFixedSize<TX_BUFFER_SIZE> out;
    return msg.serialize(out) == EmbeddedProto::Error::NO_ERRORS &&
        UART4Transport_QueuePayload(out.get_data(),out.get_size());
}
bool SendCommandReply(uint32_t request,uint32_t result,uint32_t epoch,uint32_t faults) {
    CommandReply reply; reply.set_request_id(request); reply.set_result(static_cast<CommandReply::Result>(result));
    reply.set_epoch(epoch); reply.set_faults(faults);
    ProsthesisMessage<TX_BUFFER_SIZE> msg; msg.set_reply(reply); return queue_response(msg);
}
bool SendMotorDiagnostic(const hw_diagnostic_t *d) {
    if (!d) return false;
    MotorDiagnostic diagnostic;
#define SET(field) diagnostic.set_##field(d->field)
    SET(timestamp_ms); SET(motor_id); SET(epoch); SET(flags); SET(faults);
    SET(target_rad); SET(reference_rad); SET(raw_count); SET(encoder_rad); SET(velocity_rad_s);
    SET(pwm_percent); SET(adc_raw); SET(adc_voltage); SET(current_a); SET(adc_state);
    SET(adc_error); SET(adc_age_ms); SET(kalman_rad); SET(kalman_velocity_rad_s); SET(kalman_status);
#undef SET
    ProsthesisMessage<TX_BUFFER_SIZE> msg; msg.set_diagnostic(diagnostic); return queue_response(msg);
}
