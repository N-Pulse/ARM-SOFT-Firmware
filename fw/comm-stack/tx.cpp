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
    if (action > 16U) return false;
    ClassificationData classification;
    classification.set_action(static_cast<ClassificationData::Action>(action));
    Data data; data.set_instruction(classification);
    DeviceMessage msg; msg.set_data(data);
    return queue_message(msg);
}
