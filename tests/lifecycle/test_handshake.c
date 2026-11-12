/* Actual app/parser/session/UART worker; homing, hardware and RTOS are mocked. */
#define TEST_LIFECYCLE_FIXTURE
#include "../comms/stream_fixture.h"
#include "app.h"
#include "boot_homing.h"
#include "handshake_config.h"
#include "handshake_protocol.h"

static bool home_ok=true;
static unsigned calibration_writes;
bool BootHoming_RunOnce(void) { return home_ok; }
bool BootHoming_Done(void) { return home_ok; }
bool BootHoming_OutputAllowedLocked(motor_id_t id,float pwm) {
    (void)id; (void)pwm; return false;
}
bool Encoder_BSP_SetCalibration(motor_id_t id,const encoder_calibration_t *cal) {
    (void)id; (void)cal; ++calibration_writes; return true;
}
static void tx_all(void) {
    current_task=1;
    while(queue_count) UART4Transport_TxOnce(0);
    current_task=2;
}
static void make_hello(uint8_t *f,uint8_t seq,unsigned mismatch) {
    f[0]=1; f[1]=seq; f[2]=6;
    f[3]=HANDSHAKE_PROTO_VER; f[4]=HANDSHAKE_STATUS_LEN;
    f[5]=HANDSHAKE_DATA_LEN; f[6]=HANDSHAKE_HB_PERIOD_MS;
    if(mismatch<4) f[3+mismatch]^=0x55;
    uint16_t crc=Handshake_Crc16(f,7); f[7]=(uint8_t)crc; f[8]=(uint8_t)(crc>>8);
}
static void boot_to_handshake(void) {
    fixture(); setup=true; home_ok=true; App_Init(); setup=false;
    Comms_TaskCreate(); assert(UART4Transport_StartRx((void *)2));
    calibration_writes=0;
    assert(CommsSession_GetState()==APP_INIT_MODULES);
    drain(); assert(queue_count==0); /* No SYNC during initialization. */
    uint8_t f[9]; make_hello(f,4,4); inject(f,sizeof(f)); drain();
    assert(queue_count==0 && !CommsSession_HasSession()); /* Early HELLO ignored. */
    for(unsigned n=0;n<40;++n) { App_Task(); step(10,true); }
    assert(CommsSession_GetState()==APP_INIT_HANDSHAKE);
    assert(calibration_writes==0); /* Retain boot homing zero offsets. */
    drain(); assert(queue_count==1); tx_all();
    const uint8_t sync[]={0,0,1,0xdc,0xbd};
    assert(wire_size==sizeof(sync) && !memcmp(wire,sync,sizeof(sync)));
}
static void reply_case(unsigned mismatch) {
    boot_to_handshake();
    uint8_t f[9]; make_hello(f,0xa7,mismatch);
    inject(f,4); drain(); assert(!CommsSession_HasSession());
    inject(f+4,5); drain();
    assert(queue_count==1 && !CommsSession_HasSession());
    App_Task(); assert(CommsSession_GetState()==APP_INIT_HANDSHAKE);
    tx_all(); assert(CommsSession_HasSession());
    assert(wire_size==9 && wire[0]==(mismatch<4 ? 3 : 2) && wire[1]==0xa7 && wire[2]==6);
    assert(wire[3]==HANDSHAKE_PROTO_VER && wire[4]==HANDSHAKE_STATUS_LEN &&
           wire[5]==HANDSHAKE_DATA_LEN && wire[6]==HANDSHAKE_HB_PERIOD_MS);
    assert(((uint16_t)wire[7]|((uint16_t)wire[8]<<8))==Handshake_Crc16(wire,7));
    App_Task(); assert(CommsSession_GetState()==APP_WORK);
    inject(close_hand,sizeof(close_hand)); drain();
    lifecycle_status_t s; CommsSession_GetStatus(&s); assert(s.queue_depth==0 && s.action_sequence==0);
}
int handshake_regression_suite(void) {
    const uint8_t check[]="123456789";
    assert(Handshake_Crc16(check,9)==0x29b1);
    for(unsigned mismatch=0;mismatch<=4;++mismatch) reply_case(mismatch);
    puts("PASS matching ACK and each of four NACK mismatches; local fields, echoed sequence, LE CRC, work only after TX completion");
    boot_to_handshake();
    for(unsigned n=0;n<1200;++n) {
        step(10,true); App_Task(); drain(); tx_all();
        assert(!CommsSession_HasSession() && !MotorSafety_IsFaultActive());
        assert(CommsSession_GetState()==APP_INIT_HANDSHAKE);
    }
    assert(uart_tx_calls>=24);
    puts("PASS continuous periodic SYNC beyond previous 10-second timeout; no work before HELLO");
    uint8_t f[9]; make_hello(f,7,4); f[8]^=1; inject(f,sizeof(f)); drain(); tx_all();
    assert(!CommsSession_HasSession());
    make_hello(f,8,4); f[2]=5; inject(f,sizeof(f)); drain(); tx_all();
    assert(!CommsSession_HasSession());
    inject(hello,sizeof(hello)); drain(); tx_all(); assert(!CommsSession_HasSession());
    make_hello(f,9,4); inject(f,4); drain();
    for(unsigned n=0;n<6;++n) step(10,true);
    inject(f+4,5); drain(); tx_all(); assert(!CommsSession_HasSession());
    inject(f,sizeof(f)); drain(); tx_all(); assert(CommsSession_HasSession());
    puts("PASS corrupt CRC, wrong length, legacy HELLO, partial-frame timeout rejected; parser recovers");
    boot_to_handshake(); make_hello(f,10,4); inject(f,sizeof(f)); drain();
    tx_auto_complete=false; tx_all();
    assert(!CommsSession_HasSession() && MotorSafety_IsFaultActive());
    App_Task(); assert(CommsSession_GetState()==APP_FAULT);
    puts("PASS TX timeout cannot complete handshake or enter work");
    boot_to_handshake(); make_hello(f,11,4); inject(f,sizeof(f)); drain();
    Motor_StopAll(); tx_all(); assert(!CommsSession_HasSession());
    puts("PASS STOP invalidates queued reply and prevents work");
    boot_to_handshake();
    for(unsigned n=0;n<COMMS_TX_CAPACITY;++n) assert(UART4Transport_QueueFrame(f,sizeof(f)));
    make_hello(f,12,4); inject(f,sizeof(f)); drain(); assert(!CommsSession_HasSession());
    tx_all(); assert(!CommsSession_HasSession());
    drain(); tx_all(); assert(CommsSession_HasSession());
    puts("PASS full TX queue retries stored reply without completing prematurely");
    App_Task(); assert(CommsSession_GetState()==APP_WORK);
    make_hello(f,0xff,4); inject(f,sizeof(f)); drain();
    bool duplicate_reply=false; current_task=1;
    while(queue_count) {
        assert(UART4Transport_TxOnce(0));
        if(wire_size==9 && wire[0]==2 && wire[1]==0xff) duplicate_reply=true;
    }
    current_task=2;
    assert(duplicate_reply && CommsSession_GetState()==APP_WORK);
    fixture(); setup=true; home_ok=false; App_Init(); setup=false;
    Comms_TaskCreate(); assert(UART4Transport_StartRx((void *)2));
    step(10,true); App_Task(); drain(); assert(queue_count==0 && MotorSafety_IsFaultActive());
    puts("PASS failed boot homing blocks SYNC and work");
    return 0;
}

#ifndef TEST_STREAMING_FIXTURE
int main(void) { return handshake_regression_suite(); }
#endif
