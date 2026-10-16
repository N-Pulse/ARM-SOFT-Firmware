#include "board_commission.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    uint8_t bytes[BL_PAYLOAD]; hw_command_t decoded;
    hw_command_t command={.request_id=0x12345678,.motor_id=3,.operation=7,.target_rad=.5f,.pwm_percent=5};
    const uint8_t golden[]={2,0x78,0x56,0x34,0x12,7,3,0,0,0,0x3f,5,0,0,0};
    assert(BoardCommission_Encode(&command,bytes)==sizeof golden && !memcmp(bytes,golden,sizeof golden));
    command=(hw_command_t){.request_id=UINT32_MAX,.motor_id=1,.minimum_rad=.1f,.maximum_rad=1.2f,
        .maximum_step_rad=.05f,.reference_rate_rad_s=.25f,.pwm_percent=UINT32_MAX,
        .cpr=UINT32_MAX,.quadrature=4,.gear_ratio=61,.direction=-1,.zero_offset=INT64_MIN,
        .target_rad=.75f,.stationary_reference_verified=true};
    for(unsigned operation=1;operation<=15;++operation) {
        command.operation=operation;
        uint8_t n=BoardCommission_Encode(&command,bytes);
        assert(n && n<=BL_PAYLOAD && BoardCommission_Decode(bytes,n,&decoded));
        assert(decoded.operation==operation && decoded.motor_id==1 && decoded.request_id==UINT32_MAX);
        if(operation==1) assert(!memcmp(&decoded.minimum_rad,&command.minimum_rad,sizeof(float)) &&
            !memcmp(&decoded.maximum_rad,&command.maximum_rad,sizeof(float)) &&
            !memcmp(&decoded.maximum_step_rad,&command.maximum_step_rad,sizeof(float)) &&
            !memcmp(&decoded.reference_rate_rad_s,&command.reference_rate_rad_s,sizeof(float)) && decoded.pwm_percent==UINT32_MAX);
        if(operation==2) assert(decoded.cpr==UINT32_MAX && decoded.quadrature==4 && decoded.direction==-1 &&
            decoded.gear_ratio==61 && decoded.zero_offset==INT64_MIN);
        if(operation==7) assert(decoded.target_rad==.75f && decoded.pwm_percent==UINT32_MAX);
        if(operation==15) assert(decoded.target_rad==.75f);
        if(operation==9 || operation==11) assert(decoded.stationary_reference_verified);
        for(uint8_t length=0;length<BL_PAYLOAD;++length) if(length!=n)
            assert(!BoardCommission_Decode(bytes,length,&decoded));
    }
    command.operation=16; assert(!BoardCommission_Encode(&command,bytes));
    command.operation=1; command.motor_id=5; assert(!BoardCommission_Encode(&command,bytes));
    command.motor_id=1; command.request_id=0; assert(!BoardCommission_Encode(&command,bytes));
    puts("PASS routed commissioning codec: golden target, all operations, binary32, signed 64-bit offset, full PWM width, malformed lengths/IDs");
    return 0;
}
