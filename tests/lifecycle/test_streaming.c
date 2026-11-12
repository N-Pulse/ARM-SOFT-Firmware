#define TEST_STREAMING_FIXTURE
#include "test_handshake.c"
#include "data_protocol.h"
#include "intent_config.h"
#include "motor_map.h"

static unsigned interruptions;
static unsigned safety_calls[MOTOR_STOP_CAUSE_COUNT];
static uint32_t safety_masks[MOTOR_STOP_CAUSE_COUNT];
void App_OnSafetyFault(motor_stop_cause_t cause,uint32_t mask) {
    assert_stopped();
    unsigned index=0; uint32_t bits=(uint32_t)cause;
    while((bits>>=1U)!=0) ++index;
    assert(index<MOTOR_STOP_CAUSE_COUNT); ++safety_calls[index]; safety_masks[index]|=mask;
}
void App_OnCommunicationInterrupted(void) { ++interruptions; }
static void advance(unsigned ms) {
    while(ms) { unsigned dt=ms>10 ? 10 : ms; step(dt,true); App_Task(); ms-=dt; }
}
static size_t make_data(uint8_t *f,unsigned action) {
    const size_t n=HANDSHAKE_STATUS_LEN+HANDSHAKE_DATA_LEN+5U;
    f[0]=DATA_PACKET_TYPE; f[1]=0x7b; f[2]=(uint8_t)(n-3U);
    memset(f+3,0xaa,HANDSHAKE_STATUS_LEN); /* Deliberately not protobuf. */
    memcpy(f+3+HANDSHAKE_STATUS_LEN,close_hand+2,HANDSHAKE_DATA_LEN);
    f[3+HANDSHAKE_STATUS_LEN+HANDSHAKE_DATA_LEN-1]=(uint8_t)action;
    uint16_t crc=Handshake_Crc16(f,n-2); f[n-2]=(uint8_t)crc; f[n-1]=(uint8_t)(crc>>8);
    return n;
}
static void send_action(unsigned action) {
    uint8_t f[COMMS_PAYLOAD_MAX+5]; size_t n=make_data(f,action);
    inject(f,n); drain(); tx_all();
}
static float target(void) {
    motor_state_t m; assert(Motor_GetSnapshot(MOTOR_INDEX,&m)); return m.target_position;
}
static comms_diagnostics_t diagnostics(void) {
    comms_diagnostics_t d; CommsSession_GetDiagnostics(&d); return d;
}
static void work(void) {
    boot_to_handshake(); uint8_t h[9]; make_hello(h,1,4);
    inject(h,sizeof(h)); drain(); tx_all();
    assert(CommsSession_GetState()==APP_WORK);
    interruptions=0;
    memset(safety_calls,0,sizeof(safety_calls)); memset(safety_masks,0,sizeof(safety_masks));
}
static void send_control(unsigned command) {
    lifecycle_control_t c={command,command,1}; uint8_t payload[32],frame[34];
    size_t n=Lifecycle_EncodeControl(payload,sizeof(payload),&c);
    frame[0]=0xaa; frame[1]=(uint8_t)n; memcpy(frame+2,payload,n);
    inject(frame,n+2); drain(); tx_all();
}
static void reconnect_and_safety_tests(void) {
    for(unsigned mismatch=0;mismatch<5;++mismatch) {
        work(); send_action(ACTION_CLOSE_HAND); float previous=target();
        uint32_t old_generation=CommsSession_HandshakeGenerationLocked();
        advance(HANDSHAKE_HB_PERIOD_MS);
        assert(CommsSession_GetState()==APP_INIT_HANDSHAKE && !CommsSession_HasSession());
        assert(CommsSession_HandshakeGenerationLocked()==old_generation+1);
        assert(target()==previous && calibration_writes==0 && !MotorSafety_IsFaultActive());
        drain(); tx_all();
        const uint8_t sync[]={0,0,1,0xdc,0xbd};
        assert(wire_size==5 && !memcmp(wire,sync,5));
        uint8_t h[9]; make_hello(h,42,mismatch);
        inject(h,sizeof(h)); drain(); assert(!CommsSession_HasSession());
        tx_all(); assert(wire_size==9 && wire[0]==(mismatch<4 ? 3 : 2));
        assert(CommsSession_GetState()==APP_WORK && CommsSession_HasSession());
        send_action(ACTION_OPEN_HAND); assert(target()==0); /* Fresh intent guard. */
    }
    puts("PASS heartbeat restarts exact SYNC/HELLO/ACK-or-NACK/work path without homing, target loss, or old intent guard");

    work(); advance(HANDSHAKE_HB_PERIOD_MS-1);
    uint8_t h[9]; make_hello(h,20,4); inject(h,sizeof(h)); /* Old RX backlog. */
    advance(1); drain(); tx_all(); assert(wire_size==5 && wire[0]==0);
    assert(!CommsSession_HasSession());
    uint8_t old_reply[9]; Handshake_EncodeReply(old_reply,sizeof(old_reply),20,true);
    assert(!UART4Transport_QueueHandshakeReply(old_reply,sizeof(old_reply),epoch(),0,true));
    uint32_t key=Motor_Lock();
    CommsSession_HandshakeReplySentLocked(epoch(),0,clock_ms,true);
    Motor_Unlock(key); assert(!CommsSession_HasSession());
    inject(h,sizeof(h)); drain(); tx_all(); assert(CommsSession_HasSession());
    puts("PASS old-session RX and delayed/queued ACK cannot bypass reconnection handshake");

    work();
    const uint8_t old_tx[]={0xaa,2,0x22,0};
    for(unsigned i=0;i<COMMS_TX_CAPACITY;++i) assert(UART4Transport_QueueFrame(old_tx,sizeof(old_tx)));
    advance(HANDSHAKE_HB_PERIOD_MS);
    make_hello(h,23,4); inject(h,sizeof(h)); drain(); tx_all();
    assert(!CommsSession_HasSession());
    drain(); tx_all(); assert(wire_size==5 && wire[0]==0);
    inject(h,sizeof(h)); drain(); tx_all(); assert(CommsSession_HasSession());
    puts("PASS saturated old TX queue cannot put HELLO reply before new-session SYNC");

    work(); send_action(ACTION_CLOSE_HAND); float previous=target();
    for(unsigned cmd=1;cmd<=8;++cmd) send_control(cmd);
    const uint8_t stop[]={0xaa,2,0x22,0}; inject(stop,sizeof(stop)); drain(); tx_all();
    assert(target()==previous && !MotorSafety_IsFaultActive() && CommsSession_GetState()==APP_WORK);
    assert(diagnostics().applied==1);
    for(unsigned i=0;i<4;++i) { advance(20); send_control(CONTROL_QUERY); }
    advance(20); assert(CommsSession_GetState()==APP_INIT_HANDSHAKE);
    puts("PASS all remote Control and legacy Stop commands ignored; rejected controls do not keep heartbeat alive");

    for(unsigned i=0;i<MOTOR_STOP_CAUSE_COUNT;++i) {
        work(); send_action(ACTION_CLOSE_HAND);
        motor_stop_cause_t cause=(motor_stop_cause_t)(1U<<i);
        if(cause==MOTOR_STOP_FORCE) force(MOTOR_INDEX,65000);
        else if(cause==MOTOR_STOP_DRIVER) { hardware_fault=true; MotorSafety_Poll(); }
        else MotorSafety_RequestStopCause(cause,MOTOR_INDEX);
        assert_stopped(); App_Task(); assert(safety_calls[i]==1);
        assert(CommsSession_GetState()==APP_FAULT && MotorSafety_IsFaultActive());
        App_Task(); assert(safety_calls[i]==1);
        for(unsigned cmd=1;cmd<=8;++cmd) send_control(cmd);
        inject(stop,sizeof(stop)); drain(); tx_all();
        make_hello(h,21,4); inject(h,sizeof(h)); drain(); tx_all();
        assert_stopped(); assert(CommsSession_GetState()==APP_FAULT);
        assert(MotorSafety_GetStopCauses()&(uint32_t)cause);
    }
    puts("PASS every local safety cause stops all motors, dispatches its branch once and stays faulted despite remote commands or HELLO");

    work(); MotorSafety_RequestStopCause(MOTOR_STOP_FORCE,MOTOR_INDEX); App_Task();
    MotorSafety_RequestStopCause(MOTOR_STOP_FORCE,MOTOR_THUMB); App_Task();
    assert(safety_calls[4]==2 && safety_masks[4]==((1U<<MOTOR_INDEX)|(1U<<MOTOR_THUMB)));
    MotorSafety_RequestStopCause(MOTOR_STOP_DRIVER,MOTOR_RING); App_Task();
    assert(safety_calls[5]==1 && safety_masks[5]==(1U<<MOTOR_RING)); assert_stopped();
    puts("PASS additional affected motors and simultaneous causes retain separate safety branches");

    work(); advance(HANDSHAKE_HB_PERIOD_MS); force(MOTOR_INDEX,65000); App_Task();
    drain(); tx_all(); make_hello(h,24,4); inject(h,sizeof(h)); drain(); tx_all();
    assert_stopped(); assert(CommsSession_GetState()==APP_FAULT && safety_calls[4]==1);
    puts("PASS force fault during heartbeat reconnection takes priority and blocks handshake from restarting motors");
}
int main(void) {
    handshake_regression_suite();
    work(); float closed=MotorMap_GetPose(MOTOR_POSE_CLOSED,MOTOR_INDEX);
    send_action(ACTION_CLOSE_HAND); assert(target()==closed && diagnostics().applied==1);
    advance(10); send_action(ACTION_CLOSE_HAND);
    assert(diagnostics().applied==1 && diagnostics().repeated==1);
    advance(39); send_action(ACTION_OPEN_HAND);
    assert(target()==closed && diagnostics().guard_dropped==1);
    advance(1); send_action(ACTION_OPEN_HAND);
    assert(target()==0 && diagnostics().applied==2); /* Exact 50 ms boundary. */
    assert(CommsSession_GetState()==APP_WORK);
    advance(20); send_action(ACTION_PINCH); assert(target()==0);
    advance(40); assert(target()==0); /* Dropped means never replayed later. */
    send_action(ACTION_PINCH); assert(target()>0 && diagnostics().applied==3);
    lifecycle_status_t s; CommsSession_GetStatus(&s);
    assert(s.queue_depth==0 && s.action_done==0 && s.action_sequence==3);
    puts("PASS immediate DATA dispatch, skipped Status, repeated intent does not restart guard; 49 ms drops, 50 ms preempts unfinished motion; no replay/queue/completion state");

    work();
    for(unsigned i=0;i<40;++i) { send_action(ACTION_CLOSE_HAND); advance(40); }
    assert(diagnostics().applied==1 && diagnostics().repeated==39 && interruptions==0);
    assert(CommsSession_GetState()==APP_WORK);
    /* Last valid packet was 40 ms ago. */
    advance(HANDSHAKE_HB_PERIOD_MS-41); assert(interruptions==0);
    advance(1); assert(interruptions==1 && CommsSession_GetState()==APP_INIT_HANDSHAKE);
    assert(!(MotorSafety_GetStopCauses()&MOTOR_STOP_LINK));
    float held_target=target(); send_action(ACTION_OPEN_HAND); assert(target()==held_target);
    advance(200); assert(interruptions==1);
    puts("PASS repeated intents keep heartbeat alive; configured silence deadline restarts handshake and calls notification once; no late intent execution");

    work(); send_action(ACTION_CLOSE_HAND); advance(25); send_action(ACTION_OPEN_HAND);
    advance(75); assert(CommsSession_GetState()==APP_WORK && interruptions==0);
    advance(25); assert(interruptions==1); /* Dropped action refreshed RX activity. */
    puts("PASS protection-period discarded intents refresh heartbeat without extending the action guard");

    work(); send_action(ACTION_CLOSE_HAND); advance(20);
    uint8_t backlog[COMMS_PAYLOAD_MAX+5]; size_t backlog_size=make_data(backlog,ACTION_OPEN_HAND);
    inject(backlog,backlog_size); advance(40); drain(); tx_all();
    assert(target()==closed && diagnostics().guard_dropped==1);
    puts("PASS intent received inside protection interval stays discarded even when RX processing occurs after 50 ms");

    work(); uint8_t f[COMMS_PAYLOAD_MAX+5]; size_t n=make_data(f,ACTION_CLOSE_HAND);
    inject(f,4); drain(); assert(diagnostics().applied==0);
    inject(f+4,n-4); drain(); tx_all(); assert(diagnostics().applied==1);
    advance(50); n=make_data(f,ACTION_PINCH); f[n-1]^=1;
    inject(f,n); drain(); tx_all(); assert(diagnostics().applied==1);
    advance(50); assert(interruptions==1);
    puts("PASS fragmented DATA accepted only after full CRC; corrupt CRC cannot update target or heartbeat");

    work(); n=make_data(f,ACTION_COUNT); inject(f,n); drain(); tx_all();
    assert(diagnostics().applied==0); advance(HANDSHAKE_HB_PERIOD_MS); assert(interruptions==1);
    work(); n=make_data(f,ACTION_CLOSE_HAND); f[2]--;
    inject(f,n); drain(); tx_all(); assert(diagnostics().applied==0);
    puts("PASS unknown protobuf action and wrong DATA length rejected");

    boot_to_handshake(); send_action(ACTION_CLOSE_HAND); assert(diagnostics().applied==0);
    puts("PASS DATA before successful handshake cannot move motors");

    work(); inject(close_hand,sizeof(close_hand)); drain(); tx_all();
    assert(diagnostics().applied==0);
    send_action(ACTION_CLOSE_HAND); assert(diagnostics().applied==1);
    puts("PASS legacy AA/protobuf actions cannot bypass production DATA CRC verification");

    boot_to_handshake(); uint8_t early_hello[9]; make_hello(early_hello,25,4);
    inject(early_hello,sizeof(early_hello)); drain();
    n=make_data(f,ACTION_CLOSE_HAND); inject(f,n); /* RX arrived before ACK completion. */
    tx_all(); drain(); tx_all();
    assert(CommsSession_GetState()==APP_WORK && diagnostics().applied==0);
    send_action(ACTION_CLOSE_HAND); assert(diagnostics().applied==1);
    puts("PASS pre-ACK DATA backlog cannot execute after the TX worker enters WORK");

    work(); send_action(ACTION_CLOSE_HAND); advance(50);
    n=make_data(f,ACTION_OPEN_HAND); inject(f,n);
    Motor_StopAll(); drain(); tx_all(); App_Task();
    assert(CommsSession_GetState()==APP_FAULT && !Motor_IsEnabled(MOTOR_INDEX));
    assert(diagnostics().applied==1);
    puts("PASS RTOS safety stop invalidates already-received intents and leaves work via unexpected-exit state");

    work(); clock_ms=UINT32_MAX-25U;
    CommsSession_Init(true); CommsSession_HandshakeReplySentLocked(epoch(),CommsSession_HandshakeGenerationLocked(),clock_ms,true);
    send_action(ACTION_CLOSE_HAND); clock_ms+=49U; send_action(ACTION_OPEN_HAND);
    assert(target()==closed); clock_ms+=1U; send_action(ACTION_OPEN_HAND); assert(target()==0);
    puts("PASS action guard and heartbeat arithmetic across HAL tick wrap");
    reconnect_and_safety_tests();
    return 0;
}
