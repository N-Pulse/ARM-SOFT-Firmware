/* Native mock boundary above; production controller/safety/session linked separately.
 * Only this adapter schedules the production C with Gazebo's simulation clock.
 */
#include "hand_firmware.h"
#include "motor_map.h"
#include <math.h>
static bool sync_initialized, sync_bootstrapped, sync_fault;
static uint32_t sync_seq, sync_last_control, sync_last_safety, sync_arm_ms;
static uint64_t sync_operator_session, sync_target_revision;
static uint32_t sync_operator_run;
static unsigned sync_phase;
static uint64_t sync_session_id;
static bool sync_have_physics_clock, sync_recover_pending;
static uint32_t sync_physics_last, sync_native_ms;

static bool sync_frame(uint8_t type,const uint8_t *body,size_t n) {
 uint8_t p[144]={0}; Sim_Write64(p,sync_session_id);
 Sim_Write32(p+8,type==SIM_HELLO?0U:++sync_seq);
 Sim_Write32(p+12,type==SIM_HELLO?0U:clock_ms);
 if(n)memcpy(p+16,body,n);
 uint32_t key=Motor_Lock(),epoch=MotorSafety_EpochLocked();Motor_Unlock(key);
 return SimSession_OnFrame(type,p,16U+n,clock_ms,epoch,NULL);
}
static float sync_clean(double x) { return fabs(x)<1e-7?0.0f:(float)x; }
static bool sync_feedback(const double *q,const double *v,const double *u) {
 sim_diagnostics_t d;SimSession_GetDiagnostics(&d);
 uint8_t p[119]={0};Sim_Write32(p,d.run);p[6]=8U;
 for(unsigned i=0;i<8;++i){
  size_t at=7U+14U*i;p[at]=(uint8_t)i;p[at+1]=11U;
  Sim_WriteFloat(p+at+2,sync_clean(q[i]));Sim_WriteFloat(p+at+6,sync_clean(v[i]));
  Sim_Write16(p+at+12,(uint16_t)(int16_t)lround(u[i]*100.0));
 }
 return sync_frame(SIM_FEEDBACK,p,sizeof(p));
}
int npulse_hand_init(void) {
 if(sync_initialized)return 0;
 realtime=false;Motor_InitAll();Comms_Init();Comms_TaskCreate();
 if(!UART4Transport_StartRx((void *)2))return -1;
 sync_initialized=true;return 0;
}
void npulse_hand_stop(void) {
 if(!sync_initialized)return;
 Motor_StopAll();Motor_ServiceStop();sync_phase=0;
}
static void sync_fail(void){
 if(!MotorSafety_IsFaultActive())Motor_StopAll();
 Motor_ServiceStop();sync_phase=3;sync_fault=true;sync_recover_pending=false;
}
static bool sync_connect(void){
 if(sync_session_id==UINT64_MAX)return false;
 ++sync_session_id;sync_seq=0;
 uint8_t config[4]={1,0,255,0};
 sync_bootstrapped=sync_frame(SIM_HELLO,NULL,0)&&sync_frame(SIM_CONFIG,config,sizeof(config));
 return sync_bootstrapped;
}
/* Operator adaptation only: C router/pose table supplies all named poses.
 * Optional preserve_others restores untouched targets before Motor_Update.
 * This explicitly differs from the original router's full-neutral single poses.
 * The native UART SIM_INTENT gate supports only IDs 0..4; 5..16 are dispatched
 * directly after the same armed/fully-enabled/epoch checks, not via UART.
 */
static bool sync_targets(uint8_t mask,const double values[8],uint8_t limit) {
 uint8_t p[49]={0};size_t n=1;
 for(unsigned i=0;i<8;++i)if(mask&(1U<<i)) {
  p[n++]=(uint8_t)i;Sim_WriteFloat(p+n,(float)values[i]);n+=4;p[n++]=limit;++p[0];
 }
 return p[0] && sync_frame(SIM_TARGET,p,n);
}
static bool sync_command(int action,uint8_t mask,const double targets[8],uint8_t limit,int preserve,const double current[8]) {
 if(action==-3)return sync_targets(255,current,limit);
 if(action==-1)return sync_targets(mask,targets,limit);
 double values[8],before[8];
 if(action==-2){
  for(unsigned i=0;i<8;++i)values[i]=MotorMap_GetPose(MOTOR_POSE_NEUTRAL,(motor_id_t)i);
 }else{
  if(action<0||action>ACTION_WRIST_DORSI)return false;
  sim_diagnostics_t diag;SimSession_GetDiagnostics(&diag);
  if(!diag.armed)return false;
  for(unsigned i=0;i<8;++i){
   motor_state_t m;if(!Motor_GetSnapshot((motor_id_t)i,&m)||!m.enabled)return false;
   before[i]=m.target_position;
  }
  uint32_t key=Motor_Lock(),epoch=MotorSafety_EpochLocked();Motor_Unlock(key);
  intent_t intent={.id=(intent_id_t)action,.epoch=epoch};IntentRouter_Handle(&intent);
  uint8_t changed=255;
  if(preserve && action>=ACTION_THUMB_OPEN && action<=ACTION_LITTLE_CLOSE)
   changed=(uint8_t)(1U<<((action-ACTION_THUMB_OPEN)/2));
  if(preserve && (action==ACTION_ROTATE_WRIST_R||action==ACTION_ROTATE_WRIST_L||
                  action==ACTION_WRIST_PALM||action==ACTION_WRIST_DORSI))changed=96;
  for(unsigned i=0;i<8;++i){motor_state_t m;Motor_GetSnapshot((motor_id_t)i,&m);
   values[i]=(changed&(1U<<i))?m.target_position:before[i];}
 }
/* Simulation differential: opposite motor targets produce wrist flexion. */
  if (action == ACTION_WRIST_PALM || action == ACTION_WRIST_DORSI)
    values[MOTOR_WRIST_Y] = -values[MOTOR_WRIST_X];

 /* Bound the router's 100% ceiling to the requested operator ceiling. */
 return sync_targets(255,values,limit);
}
void npulse_hand_step(uint32_t ms,const double q[8],const double v[8],const double u[8],
 int enabled,uint64_t operator_session,uint32_t operator_run,uint64_t revision,
 int action,uint8_t target_mask,const double targets[8],uint8_t limit,int preserve_others,
 struct npulse_hand_result *out) {
 if(!sync_initialized || !out)return;
 /* Epoch-relative HAL clock; forward gaps remain real gaps for C safety.
  * A physics reset stops/latches the run, never silently rearms it.
  */
 uint32_t physics_ms=ms;
 if(!sync_have_physics_clock){
  sync_have_physics_clock=true;sync_physics_last=physics_ms;sync_native_ms=0;
 }else{
  uint32_t delta=physics_ms-sync_physics_last;sync_physics_last=physics_ms;
  if(delta>0x7fffffffU){sync_fail();}else sync_native_ms+=delta;
 }
 ms=sync_native_ms;clock_ms=ms;rtos_ms=ms;current_task=2U;
 if(!sync_bootstrapped && !sync_connect())sync_fail();
 sim_diagnostics_t link;SimSession_GetDiagnostics(&link);
 if(link.live){
  if(!sync_frame(SIM_HEARTBEAT,NULL,0)||!sync_feedback(q,v,u))sync_fail();
 }else if(sync_phase==1 || sync_phase==2)sync_fail();
 if(!enabled){
  if(sync_phase==1 || sync_phase==2)npulse_hand_stop();
 }else if(operator_session!=sync_operator_session || operator_run!=sync_operator_run){
  // Only a new explicit operator run can recover a stop; heartbeats never rearm.
  sync_operator_session=operator_session;sync_operator_run=operator_run;
  sync_fault=false;sync_phase=0;
  sim_diagnostics_t previous;SimSession_GetDiagnostics(&previous);
  if(previous.armed)npulse_hand_stop();
  SimSession_GetDiagnostics(&previous);
  if(!previous.live || !previous.configured){
   if(!sync_connect() || !sync_frame(SIM_HEARTBEAT,NULL,0) || !sync_feedback(q,v,u))sync_fail();
  }
  if(!sync_fault){sync_phase=1;sync_recover_pending=true;sync_arm_ms=ms;}
 }else if(sync_phase==1 && !sync_recover_pending && ms!=sync_arm_ms){
  uint8_t mask=255U;
  if(!sync_frame(SIM_ENABLE,&mask,1)||!sync_command(action,target_mask,targets,limit,preserve_others,q))sync_fail();
  else{sync_phase=2;sync_target_revision=revision;}
 }else if(sync_phase==2 && revision!=sync_target_revision){
  if(!sync_command(action,target_mask,targets,limit,preserve_others,q))sync_fail();else sync_target_revision=revision;
 }
 if((uint32_t)(ms-sync_last_control)>=10U){sync_last_control=ms;Motor_Update();}
 /* Simulation-adapter diagnostic only. Snapshot before GUARD cancels targets.
  * Emit only once on an active-to-fault transition, after C safety inhibition.
  * No changes to PID, guard thresholds, coupling or production fw/ sources.
  */
 motor_state_t diagnostic_before_guard[8];
 bool diagnostic_was_active=(sync_phase==2);
 if(diagnostic_was_active)for(unsigned i=0;i<8;++i)
  Motor_GetSnapshot((motor_id_t)i,&diagnostic_before_guard[i]);
 if((uint32_t)(ms-sync_last_safety)>=20U){
  sync_last_safety=ms;MotorGuard_Poll();MotorSafety_Poll();Motor_ServiceStop();
 }
 /* Recover only after fresh inhibited scheduler updates have restored timing.
  * Recovery/arm still go through production safety/session checks.
  */
 if(enabled && sync_phase==1 && sync_recover_pending && (uint32_t)(ms-sync_arm_ms)>=20U){
  if(!sync_frame(SIM_RECOVER,NULL,0)||!sync_frame(SIM_ARM,NULL,0))sync_fail();
  else{sync_recover_pending=false;sync_arm_ms=ms;}
 }
 sim_diagnostics_t d;SimSession_GetDiagnostics(&d);
 uint32_t causes=MotorSafety_GetStopCauses();
 if(diagnostic_was_active && causes){
  static const char *names[8]={"thumb","index","middle","ring","little","wrist_x","wrist_y","palm"};
  uint32_t guard_mask=MotorSafety_GetCauseMotorMask(MOTOR_STOP_GUARD);
  fprintf(stderr,"HAND_FAULT sim_ms=%u since_arm_ms=%u action=%d target_mask=0x%02x causes=%u guard_mask=0x%02x\n",
   (unsigned)ms,(unsigned)(ms-sync_arm_ms),action,(unsigned)target_mask,(unsigned)causes,(unsigned)guard_mask);
  for(unsigned i=0;i<8;++i){
   const motor_state_t *m=&diagnostic_before_guard[i];
   fprintf(stderr,"HAND_AXIS %s %s target=%.2f measured=%.2f deg velocity=%.4f rad/s requested=%+.3f%% previous_applied=%+.3f%%\n",
    (guard_mask&(1U<<i))?"GUARD_SOURCE":"other",names[i],
    (double)m->target_position*57.295779513,(double)q[i]*57.295779513,(double)v[i],(double)m->output_pwm,(double)u[i]);
  }
  fflush(stderr);
 }
 uint8_t enabled_mask=0;
 for(unsigned i=0;i<8;++i)if(Motor_IsEnabled((motor_id_t)i))enabled_mask|=(uint8_t)(1U<<i);
 if(enabled && sync_phase==2 && (causes || !d.armed || enabled_mask!=255)){sync_fault=true;sync_phase=3;}
 memset(out,0,sizeof(*out));out->sim_ms=physics_ms;out->run=d.run;out->causes=causes;
 out->phase=(uint8_t)(sync_fault?3:sync_phase);out->armed=d.armed;out->enabled_mask=enabled_mask;
 for(unsigned i=0;i<8;++i){motor_state_t m;Motor_GetSnapshot((motor_id_t)i,&m);
  out->feedback_age_ms[i]=ms-m.feedback_ms;
  out->target[i]=m.target_position;out->reference[i]=m.reference_position;
  out->position[i]=m.measured_position;out->velocity[i]=v[i];
  out->output[i]=(enabled&&sync_phase==2&&!causes)?m.output_pwm:0;
  out->kp[i]=m.pid.config.kp;out->ki[i]=m.pid.config.ki;out->kd[i]=m.pid.config.kd;
 }
 assert(!primask&&!physical_calls);
}
