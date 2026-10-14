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

bool SendLinkDiagnostic(const link_diagnostic_t *d) {
    if (!d) return false;
    /* Small bounded varint-only writer for the additive LinkDiagnostic schema.
     * Existing generated host command/motor classes remain unchanged. The
     * native schema test decodes this exact output with Google's protobuf. */
    uint8_t payload[TX_BUFFER_SIZE]; size_t size=2;
    auto put=[&](uint64_t value) {
        do { if(size==sizeof(payload)) return false;
             payload[size++]=static_cast<uint8_t>((value&127U)|(value>127U?128U:0U)); value>>=7;
        } while(value);
        return true;
    };
    const uint64_t fields[]={d->timestamp_ms,d->role,d->local_session,d->remote_session,
        d->local_epoch,d->remote_epoch,d->local_stops,d->remote_stops,d->heartbeat_age_ms,
        d->echo_age_ms,d->flags,d->rx_sequence,d->tx_sequence,d->malformed,d->overflows,d->restarts,
        d->rejected,d->transport_errors};
    for(unsigned i=0;i<18;++i) if(fields[i] && (!put((i+1U)<<3) || !put(fields[i]))) return false;
    payload[0]=0x2AU; payload[1]=static_cast<uint8_t>(size-2U); /* ProsthesisMessage.link=5. */
    return UART4Transport_QueuePayload(payload,size);
}
