/* Each DLL has its own actual motor/PID/safety/BSP/driver globals and clock.
 * Reuse the lot-2 physical register model; no serial port or board is accessed.
 * Dedicated native opt-in allows ONLY its selected motor. The protocol hook
 * invokes the already-authorized local target gate, never arms or enables. */
#define main lot2_reference_main
#include "test_l298n.c"
#undef main
#include "board_protocol.h"
#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))
#endif
static board_protocol_t distributed;
static void distributed_stop(void *unused)
{ (void)unused; MotorSafety_RequestStopCause(MOTOR_STOP_LINK,MOTOR_COUNT); }
static uint8_t distributed_target(void *unused,uint8_t id,int32_t target,uint8_t speed,uint32_t epoch)
{
    (void)unused;
    if(id!=selected || !BoardLink_IsLocalMotor((motor_id_t)id) || !Motor_IsEnabled(selected) ||
       !Motor_HasFreshPositionFeedback(selected)) return BL_FORBIDDEN;
    uint8_t pwm=speed; float radians=target/1000.0f;
    if(!HWCommission_FilterTarget(radians,&pwm)) return BL_FORBIDDEN;
    return Motor_SetTargetAtEpoch(selected,radians,pwm,epoch)?BL_ACCEPTED:BL_STALE;
}
static void sync_state(void)
{ BL_SetLocalState(&distributed,MotorSafety_EpochLocked(),MotorSafety_GetStopCauses(),now); }
EXPORT void B3_Init(uint32_t clock,uint64_t session)
{
    now=clock; slave=selected!=MOTOR_LITTLE;
    BoardLink_Init(); OldHW_HoldOutputsLow(); OldHW_ClockConfig(); OldHW_Init();
    Motor_InitAll(); CommsSession_Init(true); configure(); stopped();
    if(selected==MOTOR_LITTLE) Motor_StopAll(); /* Deliberately independent epoch history. */
    assert(BL_Init(&distributed,(uint8_t)BoardLink_Role(),session,now,
                   (bl_hooks_t){distributed_stop,distributed_target,NULL})); sync_state();
}
EXPORT void B3_Step(uint32_t elapsed)
{ hello(); advance(elapsed); sync_state(); BL_Poll(&distributed,now); }
EXPORT uint32_t B3_Take(uint8_t *out)
{ sync_state(); return (uint32_t)BL_Take(&distributed,now,out); }
EXPORT void B3_Feed(const uint8_t *p,uint32_t length)
{ sync_state(); for(unsigned i=0;i<length;++i) BL_Feed(&distributed,p[i],now,now); }
EXPORT uint32_t B3_Recover(void)
{ hello(); uint32_t r=command((hw_command_t){.operation=4}); sync_state(); return r; }
EXPORT uint32_t B3_Authorize(void) { sync_state(); return BL_Authorize(&distributed,now); }
EXPORT uint32_t B3_Enable(void)
{
    hello();
    if(command((hw_command_t){.operation=11,.stationary_reference_verified=true})!=HW_OK) return 1;
    if(command((hw_command_t){.operation=5})!=HW_OK) return 2;
    return command((hw_command_t){.operation=6});
}
EXPORT uint32_t B3_Target(uint32_t id,int32_t target)
{ sync_state(); return BL_Target(&distributed,(uint8_t)id,target,3,MotorSafety_EpochLocked(),now); }
EXPORT uint32_t B3_LocalTarget(void)
{ return command((hw_command_t){.operation=7,.target_rad=.02f,.pwm_percent=3}); }
EXPORT uint32_t B3_Ready(void) { sync_state(); return BL_Ready(&distributed,now); }
EXPORT uint32_t B3_Epoch(void) { return MotorSafety_EpochLocked(); }
EXPORT uint32_t B3_Faults(void) { return MotorSafety_GetStopCauses(); }
EXPORT float B3_PWM(void) { return Motor_L298N_AppliedPWM(selected); }
EXPORT uint32_t B3_Disabled(void) { return OldHW_OutputsDisabled() && !Motor_IsEnabled(selected); }
EXPORT void B3_Stop(void) { Motor_StopAll(); sync_state(); }
EXPORT void B3_ThumbFault(void) { thumb_edge(3,3); sync_state(); }
EXPORT void B3_Feedback(void)
{
    encoder_snapshot_t e={0}; motor_state_t m; Motor_GetSnapshot(selected,&m);
    bool valid=Encoder_BSP_GetSnapshot(selected,&e);
    bl_feedback_t f={.id=(uint8_t)selected,.flags=(valid?BL_VALID:0)|
        (Encoder_BSP_IsCalibrated(selected)?BL_CALIBRATED:0)|(m.enabled?BL_ENABLED:0),
        .age_ms=valid?0:UINT32_MAX,.position_mrad=valid?(int32_t)(e.position_rad*1000):0,
        .raw_count=e.raw_count,.pwm_milli=(int32_t)(B3_PWM()*1000),.faults=B3_Faults()};
    BL_OfferFeedback(&distributed,&f,now);
}
EXPORT uint32_t B3_RemoteValid(uint32_t id)
{ bl_feedback_t f; return BL_GetFeedback(&distributed,(uint8_t)id,now,&f) && (f.flags&BL_VALID); }
