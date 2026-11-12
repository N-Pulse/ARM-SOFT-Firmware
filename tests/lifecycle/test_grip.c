#define TEST_STREAMING_FIXTURE
#include "test_handshake.c"
#include "motor_grip_config.h"
#include "intent_config.h"

static unsigned current_action;
static uint32_t selected(void) { uint32_t s; Motor_GetGripMasks(&s,NULL); return s; }
static uint32_t contacted(void) { uint32_t s; Motor_GetGripMasks(NULL,&s); return s; }
static void action(unsigned id) {
    uint8_t f[]={0x10,1,10,0,0,0x12,4,0x0a,2,8,(uint8_t)id,0,0};
    uint16_t crc=Handshake_Crc16(f,sizeof(f)-2); f[11]=(uint8_t)crc; f[12]=(uint8_t)(crc>>8);
    inject(f,sizeof(f)); drain(); tx_all(); current_action=id;
}
static void work(void) {
    boot_to_handshake(); uint8_t h[9]; make_hello(h,1,4); inject(h,9); drain(); tx_all();
    assert(CommsSession_GetState()==APP_WORK); current_action=ACTION_OPEN_HAND;
}
static void tick(unsigned n,uint32_t moving) {
    for(unsigned j=0;j<n;++j) {
        for(unsigned i=0;i<MOTOR_COUNT;++i) if(moving&(1U<<i)) raw[i]+=MOTOR_GUARD_EPS_COUNTS;
        step(10,true); MotorGuard_Poll(); App_Task();
        if(j%4==0) action(current_action); /* Fixed-rate repeated classification. */
    }
}
int main(void) {
    for(unsigned id=0;id<ACTION_COUNT;++id) {
        uint8_t p[]={0x12,4,0x0a,2,8,(uint8_t)id};
        rx_result_t r=HandleDeviceMessage(p,sizeof(p)); assert(r.type==RX_TYPE_ACTION && r.data.action_id==id);
    }
    uint8_t bad[]={0x12,4,0x0a,2,8,ACTION_COUNT}; assert(HandleDeviceMessage(bad,sizeof(bad)).type==RX_TYPE_NONE);
    work(); assert(SendClassificationData(ACTION_GRASPING)); tx_all(); assert(wire[wire_size-1]==17);
    assert(SendClassificationData(ACTION_PINCHING)); tx_all(); assert(wire[wire_size-1]==18);
    assert(!SendClassificationData(ACTION_COUNT));
    work(); Motor_Disable(MOTOR_THUMB); action(ACTION_PINCHING);
    assert(selected()==0);
    comms_diagnostics_t rejected; CommsSession_GetDiagnostics(&rejected); assert(rejected.applied==0);
    assert(Motor_Enable(MOTOR_THUMB)); action(ACTION_PINCHING); assert(selected()==3);
    puts("PASS a rejected grip is not marked current; retrying the same intent can start it");
    work(); action(ACTION_GRASPING); assert(selected()==31 && contacted()==0);
    tick(5,30); assert(Motor_DRV8848_DrivingDir(MOTOR_THUMB)>0);
    for(unsigned i=1;i<5;++i) assert(Motor_DRV8848_DrivingDir((motor_id_t)i)>0);
    assert(Motor_DRV8848_DrivingDir(MOTOR_WRIST_X)==0 && Motor_DRV8848_DrivingDir(MOTOR_PALM)==0);
    tick(16,30); assert(contacted()==1 && Motor_DRV8848_DrivingDir(MOTOR_THUMB)==0);
    assert(!MotorSafety_IsFaultActive());
    tick(20,0); assert(contacted()==31 && !MotorSafety_IsFaultActive());
    for(unsigned i=0;i<5;++i) assert(Motor_DRV8848_DrivingDir((motor_id_t)i)==0);
    assert(Motor_DRV8848_IsArmed());
    tick(30,0); assert(contacted()==31 && !MotorSafety_IsFaultActive());
    assert(Motor_BeginGrip(31,epoch()) && contacted()==31);
    puts("PASS IDs 0..18 decode, grasping selects five fingers, constant closing PWM stops each independently on stall; repeated intents never restart contact motors");

    action(ACTION_OPEN_HAND); assert(selected()==0 && contacted()==0);
    tick(6,0); action(ACTION_CLOSE_HAND); tick(25,0);
    assert(MotorSafety_GetStopCauses()&MOTOR_STOP_GUARD); assert_stopped();
    puts("PASS a different ordinary intent cancels grip mode and restores global stall protection");

    work(); action(ACTION_PINCHING); assert(selected()==3);
    assert(!Motor_BeginGrip(1U<<MOTOR_WRIST_X,epoch()) && selected()==3);
    tick(2,3);
    assert(Motor_DRV8848_DrivingDir(MOTOR_THUMB)>0 && Motor_DRV8848_DrivingDir(MOTOR_INDEX)>0);
    for(unsigned i=2;i<MOTOR_COUNT;++i) assert(Motor_DRV8848_DrivingDir((motor_id_t)i)==0);
    tick(22,0); assert(contacted()==3 && !MotorSafety_IsFaultActive());
    puts("PASS pinching selects only thumb/index, stops on contact without disabling protection for other motors");

    work(); action(ACTION_CLOSE_HAND); tick(6,31);
    assert(Motor_DRV8848_DrivingDir(MOTOR_MIDDLE)>0);
    action(ACTION_PINCHING); assert(selected()==3);
    assert(Motor_DRV8848_DrivingDir(MOTOR_THUMB)==0 && Motor_DRV8848_DrivingDir(MOTOR_INDEX)==0);
    for(unsigned i=2;i<MOTOR_COUNT;++i) assert(Motor_DRV8848_DrivingDir((motor_id_t)i)==0);
    tick(5,3);
    for(unsigned i=2;i<MOTOR_COUNT;++i) assert(Motor_DRV8848_DrivingDir((motor_id_t)i)==0);
    puts("PASS entering pinching terminates previous nonparticipant motion instead of continuing an older full-hand close");

    work(); action(ACTION_GRASPING); tick(12,0);
    action(ACTION_PINCHING); assert(selected()==3 && contacted()==0);
    for(unsigned i=2;i<5;++i) assert(Motor_DRV8848_DrivingDir((motor_id_t)i)==0);
    tick(10,0); assert(contacted()==0); /* New mode has its own stall window. */
    tick(12,0); assert(contacted()==3 && !MotorSafety_IsFaultActive());
    intent_t stale={ACTION_OPEN_HAND,epoch()-1}; IntentRouter_Handle(&stale); assert(selected()==3);
    puts("PASS grasp-to-pinch stops nonparticipants, resets contact timers, and stale intents cannot cancel current grip");

    work(); action(ACTION_PINCHING);
    assert(Motor_SetTargetAtEpoch(MOTOR_WRIST_X,0.5f,100,epoch()));
    tick(24,3); assert(MotorSafety_GetStopCauses()&MOTOR_STOP_GUARD); assert_stopped();
    assert(selected()==0 && contacted()==0);
    puts("PASS a nonparticipating wrist stall still stops all motors while pinching");

    work(); action(ACTION_GRASPING); tick(2,31);
    motor_feedback_t force={.id=MOTOR_INDEX,.force_voltage_v=MOTOR_FORCE_HARD_LIMIT_V,.valid_fields=MOTOR_FB_FORCE_VOLTAGE};
    MotorBackend_OnFeedback(&force); assert_stopped(); assert(MotorSafety_GetStopCauses()&MOTOR_STOP_FORCE);
    assert(selected()==0 && contacted()==0);
    work(); action(ACTION_GRASPING); local[MOTOR_INDEX]=false; MotorGuard_Poll();
    assert(MotorSafety_IsFaultActive()); assert_stopped();
    puts("PASS force hard limit and missing encoder are never treated as contact; both latch global safety stop");

    work(); action(ACTION_PINCHING); tick(2,3);
    intent_t ordinary={ACTION_ROTATE_WRIST_R,epoch()}; IntentRouter_Handle(&ordinary);
    assert(selected()==0 && Motor_DRV8848_DrivingDir(MOTOR_THUMB)==0);
    assert(Motor_DRV8848_DrivingDir(MOTOR_INDEX)==0);
    puts("PASS partial wrist intent also cancels residual finger closing outputs");

    work(); action(ACTION_GRASPING);
    positions[MOTOR_INDEX]=2.0f; tick(1,31);
    assert(MotorSafety_GetStopCauses()&MOTOR_STOP_POSITION); assert_stopped();
    puts("PASS contact-seek cannot bypass the configured closing travel limit");
    return 0;
}
