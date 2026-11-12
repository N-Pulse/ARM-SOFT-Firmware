/* Dedicated commissioning still must not initialize or arm the full mechanism. */
#define TEST_STREAMING_FIXTURE
#include "test_handshake.c"
bool Encoder_BSP_Reset(motor_id_t id) {
    if(Motor_IsEnabled(id)) return false;
    raw[id]=0; positions[id]=0; Motor_InvalidatePositionFeedback(id); return true;
}
int main(void) {
    fixture(); setup=true; App_Init(); setup=false;
    Comms_TaskCreate(); assert(UART4Transport_StartRx((void *)2));
    for(unsigned i=0;i<1100;++i) { step(10,true); App_Task(); }
    assert(!MotorSafety_IsFaultActive()); assert_stopped();
    assert(CommsSession_GetState()==APP_INIT_HANDSHAKE);
    const uint8_t remote_stop[]={0xaa,2,0x22,0};
    inject(remote_stop,sizeof(remote_stop)); drain(); tx_all(); assert_stopped();
    assert(!MotorSafety_IsFaultActive());
    puts("PASS commissioning App: no automatic homing/arm/return, no initial timeout, removed remote STOP has no side effects");
    return 0;
}
