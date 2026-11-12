#define TEST_STREAMING_FIXTURE
#include "test_handshake.c"
int main(void) {
    fixture(); setup=true; App_Init(); setup=false;
    for(unsigned i=0;i<5;++i) { step(10,true); App_Task(); }
    assert(!MotorSafety_CanArmLocked());
    assert(MotorSafety_GetStopCauses()&MOTOR_STOP_INIT);
    assert_stopped();
    puts("PASS invalid float force thresholds cannot arm drivers and latch initialization failure");
    return 0;
}
