#include "rx.h"
#include "protocol_messages.h"
#include <cassert>
#include <cstdio>
int main()
{
    static_assert(static_cast<unsigned>(CommissionCommand::Operation::OUTPUT_PREFLIGHT)==11,"wire enum");
    const unsigned char payload[]={26,11,8,14,16,42,24,11,32,1,128,1,1};
    const auto decoded=HandleDeviceMessage(payload,sizeof(payload));
    assert(decoded.type==RX_TYPE_COMMISSION && decoded.command.operation==11);
    assert(decoded.command.request_id==14 && decoded.command.epoch==42 && decoded.command.motor_id==1);
    assert(decoded.command.stationary_reference_verified);
    static_assert(static_cast<unsigned>(CommissionCommand::Operation::LINK_AUTHORIZE)==12,"wire enum");
    static_assert(static_cast<unsigned>(CommissionCommand::Operation::LINK_RECOVER)==13,"wire enum");
    for(unsigned op=12;op<=13;++op) {
        unsigned char link_payload[]={26,8,8,15,16,42,24,0,32,1}; link_payload[7]=static_cast<unsigned char>(op);
        const auto link=HandleDeviceMessage(link_payload,sizeof(link_payload));
        assert(link.type==RX_TYPE_COMMISSION && link.command.operation==op && link.command.epoch==42);
    }
    std::puts("PASS production C++ protobuf preflight decoding, existing fields/wire numbers preserved; exit=0");
}
