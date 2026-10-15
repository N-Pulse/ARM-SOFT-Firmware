/* Real app/session/parser/PID/interlock/driver; only hardware/RTOS are mocked. */
#define TEST_LIFECYCLE_FIXTURE
#include "../comms/test_comms.c"
#include "app.h"
#include "lifecycle_config.h"
static bool calibration_fail, force_stream;
static uint32_t host_request;
bool Encoder_BSP_SetCalibration(motor_id_t id,const encoder_calibration_t *cal) {
    if(calibration_fail || Motor_IsEnabled(id) || !cal->counts_per_revolution) return false;
    calibrated[id]=true; Motor_InvalidatePositionFeedback(id); return true;
}
static void tx_drain(void) {
    current_task=1; while(UART4Transport_TxOnce(0)) {} current_task=2;
}
static void control(uint32_t command) {
    lifecycle_control_t c={command,++host_request,1}; uint8_t p[32],f[34];
    size_t n=Lifecycle_EncodeControl(p,sizeof(p),&c);
    f[0]=0xaa; f[1]=(uint8_t)n; memcpy(f+2,p,n);
    inject(f,n+2); drain(); tx_drain();
}
static void tick(bool follow) {
    if(follow) for(unsigned i=0;i<MOTOR_COUNT;++i) {
        motor_state_t m; Motor_GetSnapshot((motor_id_t)i,&m); positions[i]=m.target_position;
    }
    if(force_stream) force(MOTOR_INDEX,100);
    step(10,true); App_Task(); MotorSafety_Poll(); Motor_ServiceStop();
}
static void ticks(unsigned n,bool follow) { for(unsigned i=0;i<n;++i) tick(follow); }
static void boot(void) {
    fixture(); calibration_fail=force_stream=false; host_request=0;
    setup=true; App_Init(); setup=false; Comms_TaskCreate();
    assert(UART4Transport_StartRx((void *)2));
    step(10,true); App_Task();
    assert(CommsSession_GetState()==APP_INIT_HANDSHAKE); assert_stopped();
}
static void ready(void) {
    boot(); session(); tx_drain(); ticks(30,true);
    assert(CommsSession_GetState()==APP_READY);
    for(unsigned i=0;i<MOTOR_COUNT;++i) assert(Motor_IsEnabled((motor_id_t)i));
}
static void decoder_tests(void) {
    uint8_t b[64]; lifecycle_control_t c={CONTROL_STOP,123,1},decoded;
    size_t n=Lifecycle_EncodeControl(b,sizeof(b),&c);
    assert(Lifecycle_DecodeDevice(b,n,&decoded)==1 && decoded.command==CONTROL_STOP);
    assert(Lifecycle_DecodeDevice(b,n-1,&decoded)==-1);
    b[n]=10; b[n+1]=0; assert(Lifecycle_DecodeDevice(b,n+2,&decoded)==-1);
    uint8_t duplicate[]={26,8,8,3,16,1,24,1,8,3};
    assert(Lifecycle_DecodeDevice(duplicate,sizeof(duplicate),&decoded)==-1);
    uint8_t omitted[]={26,4,8,3,24,1};
    assert(Lifecycle_DecodeDevice(omitted,sizeof(omitted),&decoded)==-1);
    report("control wire codec rejects truncation, mixed oneof, duplicate and missing identity fields");
}
static void workflow_tests(void) {
    ready(); control(CONTROL_DENY_START); tick(true);
    assert(CommsSession_GetState()==APP_READY);
    inject(close_hand,sizeof(close_hand)); drain(); tx_drain();
    lifecycle_status_t st; CommsSession_GetStatus(&st); assert(st.queue_depth==0);
    control(CONTROL_ALLOW_START); tick(true); assert(CommsSession_GetState()==APP_WORK_WAIT);
    control(CONTROL_RESTART); tick(true); assert(CommsSession_GetState()==APP_WORK_WAIT);
    inject(close_hand,sizeof(close_hand)); drain(); tx_drain(); tick(true);
    assert(CommsSession_GetState()==APP_WORK_EXECUTING);
    uint8_t open[]={0xaa,4,0x12,2,0x0a,0}; inject(open,sizeof(open)); drain(); tx_drain();
    ticks(10,true); CommsSession_GetStatus(&st);
    assert(st.queue_depth==1 && st.action_sequence==1);
    motor_state_t m; Motor_GetSnapshot(MOTOR_INDEX,&m); assert(m.target_position>0);
    ticks(50,true); CommsSession_GetStatus(&st);
    assert(st.queue_depth==0 && st.action_sequence==2 && st.state==APP_WORK_EXECUTING);
    ticks(60,true); assert(CommsSession_GetState()==APP_WORK_WAIT);
    for(unsigned i=0;i<MOTOR_COUNT;++i) assert(Motor_IsEnabled((motor_id_t)i));
    control(CONTROL_STOP); assert(CommsSession_GetState()==APP_STOPPING); ticks(30,true);
    assert(CommsSession_GetState()==APP_STOPPED); assert_stopped();
    control(CONTROL_ALLOW_START); tick(true); assert(CommsSession_GetState()==APP_STOPPED);
    control(CONTROL_RESTART); tick(true); assert(CommsSession_GetState()==APP_INIT_HANDSHAKE);
    session(); tx_drain(); ticks(30,true); assert(CommsSession_GetState()==APP_READY);
    report("READY approval/denial, serial FIFO actions, PID holding, return STOP, restart with fresh handshake");
}
static void stop_priority_tests(void) {
    ready(); control(CONTROL_ALLOW_START); tick(true);
    for(unsigned i=0;i<COMMS_INTENT_CAPACITY+2;++i) { inject(close_hand,sizeof(close_hand)); drain(); tx_drain(); }
    lifecycle_status_t st; CommsSession_GetStatus(&st);
    assert(st.queue_depth==8 && st.dropped==2);
    control(CONTROL_STOP); CommsSession_GetStatus(&st); assert(st.queue_depth==0 && st.state==APP_STOPPING);
    ticks(30,true); assert(CommsSession_GetState()==APP_STOPPED); assert_stopped();
    report("full FIFO drops newest intent and reports; STOP bypasses saturation and cancels queue");
}
static void fault_tests(void) {
    ready(); control(CONTROL_ALLOW_START); tick(true);
    control(CONTROL_ESTOP); tick(false); assert(CommsSession_GetState()==APP_FAULT); assert_stopped();
    control(CONTROL_RESTART); tick(true); session(); tx_drain(); ticks(30,true);
    assert(CommsSession_GetState()==APP_READY);
    MotorSafety_RequestStopCause(MOTOR_STOP_FORCE,MOTOR_INDEX); force(MOTOR_INDEX,1700);
    tick(false); control(CONTROL_RESTART); tick(false);
    assert(CommsSession_GetState()==APP_FAULT); assert_stopped();
    lifecycle_status_t st; CommsSession_GetStatus(&st); assert(st.result==APP_RESULT_RECOVERY_DENIED);
    force_stream=true; force(MOTOR_INDEX,100); control(CONTROL_RESTART); tick(true); session(); tx_drain(); ticks(30,true);
    assert(CommsSession_GetState()==APP_READY);
    report("ESTOP/force faults stop immediately; persistent cause rejects recovery");
    boot(); calibration_fail=true; session(); tx_drain(); tick(true); tick(true);
    assert(CommsSession_GetState()==APP_FAULT); assert(MotorSafety_GetStopCauses()&MOTOR_STOP_INIT);
    report("module calibration failure prevents READY and latches initialization fault");
}
static void watchdog_tests(void) {
    ready(); ticks(50,true); drain(); tx_drain();
    lifecycle_control_t p; /* Probe already offered by production comm task. */
    lifecycle_control_t response={CONTROL_PONG,1,1}; rx_result_t r={.type=RX_TYPE_CONTROL,.control=response};
    bool started;
    assert(CommsSession_OnMessage(&r,clock_ms,epoch(),&started));
    ticks(60,true); drain(); tx_drain();
    response.request_id=1; r.control=response;
    assert(!CommsSession_OnMessage(&r,clock_ms,epoch(),&started));
    response.request_id=2; r.control=response;
    assert(CommsSession_OnMessage(&r,clock_ms,epoch(),&started));
    assert(!CommsSession_GetProbe(&p));
    ticks(210,true); tick(true);
    assert(MotorSafety_GetStopCauses()&MOTOR_STOP_LINK); assert_stopped();
    report("periodic handshake in READY, matching PONG only, missed reply latches LINK");
    boot(); ticks(1002,true); assert(CommsSession_GetState()==APP_FAULT); assert_stopped();
    report("missing initial handshake times out before any motor is armed");
}
static void return_and_settle_tests(void) {
    ready(); control(CONTROL_ALLOW_START); tick(true);
    inject(close_hand,sizeof(close_hand)); drain(); tx_drain(); ticks(15,true);
    assert(CommsSession_GetState()==APP_WORK_EXECUTING);
    control(CONTROL_STOP); tick(false);
    assert(CommsSession_GetState()==APP_STOPPING && !MotorSafety_IsFaultActive());
    /* Real return takes time; don't mark STOPPED at receipt. */
    ticks(10,true); assert(CommsSession_GetState()==APP_STOPPING);
    ticks(50,true); assert(CommsSession_GetState()==APP_STOPPED); assert_stopped();
    for(unsigned i=0;i<MOTOR_COUNT;++i) { motor_state_t m; Motor_GetSnapshot((motor_id_t)i,&m); assert(fabsf(m.measured_position)<0.001f); }
    report("STOP during motion returns all final targets to OPEN before disabling controllers");
    ready(); control(CONTROL_ALLOW_START); tick(true);
    uint8_t open[]={0xaa,4,0x12,2,0x0a,0}; inject(open,sizeof(open)); drain(); tx_drain();
    ticks(20,true); assert(CommsSession_GetState()==APP_WORK_EXECUTING);
    positions[MOTOR_INDEX]=0.1f; tick(false);
    ticks(10,true); assert(CommsSession_GetState()==APP_WORK_EXECUTING);
    ticks(20,true); assert(CommsSession_GetState()==APP_WORK_WAIT);
    positions[MOTOR_INDEX]=0.01f; step(10,true); MotorGuard_Poll();
    for(unsigned i=0;i<30;++i) { step(10,true); MotorGuard_Poll(); }
    assert(!MotorSafety_IsFaultActive());
    report("identical targets still settle continuously; error excursion resets timer; tiny hold drive is not a stall");
}
static void additional_fault_tests(void) {
    ready(); control(CONTROL_ALLOW_START); tick(true);
    inject(close_hand,sizeof(close_hand)); drain(); tx_drain(); ticks(60,true);
    lifecycle_control_t probe; assert(CommsSession_GetProbe(&probe));
    for(unsigned i=0;i<220;++i) {
        tick(true);
        if(i%20==0) { rx_result_t h={.type=RX_TYPE_HELLO,.proto_version=1,.data.device_id=1}; bool begin;
            CommsSession_OnMessage(&h,clock_ms,epoch(),&begin); }
    }
    assert(CommsSession_GetState()==APP_FAULT); assert(MotorSafety_GetStopCauses()&MOTOR_STOP_LINK); assert_stopped();
    report("missing matching PONG faults even with repeated Hello during an active work session");
    ready(); control(CONTROL_ALLOW_START); tick(true);
    inject(close_hand,sizeof(close_hand)); drain(); tx_drain(); tick(false);
    for(unsigned i=0;i<APP_MOTION_TIMEOUT_MS/10+2;++i) {
        tick(false); rx_result_t h={.type=RX_TYPE_HELLO,.proto_version=1,.data.device_id=1}; bool begin;
        CommsSession_OnMessage(&h,clock_ms,epoch(),&begin);
        if(CommsSession_GetProbe(&probe)) { rx_result_t r={.type=RX_TYPE_CONTROL};
            r.control=probe; r.control.command=CONTROL_PONG;
            CommsSession_OnMessage(&r,clock_ms,epoch(),&begin); }
    }
    assert(MotorSafety_GetStopCauses()&MOTOR_STOP_MOTION_TIMEOUT); assert_stopped();
    report("motion maximum duration independently prevents permanent EXECUTING");
    ready(); rx_result_t bad={.type=RX_TYPE_HELLO,.proto_version=2,.data.device_id=1}; bool begin;
    assert(!CommsSession_OnMessage(&bad,clock_ms,epoch(),&begin));
    bad.proto_version=1; bad.data.device_id=2;
    assert(!CommsSession_OnMessage(&bad,clock_ms,epoch(),&begin)); assert_stopped();
    report("incompatible protocol rejected; changed peer identity immediately stops a bound session");
}
static void diagnostic_tests(void) {
    fixture(); current_task=1;
    uint8_t p[64]; size_t n=Lifecycle_EncodeSafety(p,sizeof(p),clock_ms,MOTOR_STOP_GUARD,2,epoch(),APP_WORK_EXECUTING);
    assert(UART4Transport_QueueSafetyPayload(p,n));
    Motor_StopAll();
    assert(UART4Transport_TxOnce(0)); assert(wire[2]==0x22);
    motor_safety_event_t e; assert(MotorSafety_PeekEvent(&e));
    assert(e.cause==MOTOR_STOP_MANUAL && e.motor_mask==255);
    MotorSafety_ConsumeEvent(e.sequence); assert(!MotorSafety_PeekEvent(&e));
    report("deferred safety records survive later stop epochs and preserve cause/motor context");
}
int main(void) {
    setbuf(stdout,NULL); scenarios=0; decoder_tests(); workflow_tests(); stop_priority_tests(); fault_tests(); watchdog_tests(); return_and_settle_tests(); additional_fault_tests(); diagnostic_tests();
    printf("Lifecycle: %u scenarios passed\n",scenarios); return 0;
}
