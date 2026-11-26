
/* Native mock boundary above; production controller/safety/session linked separately.
 * Only this adapter schedules the production C with Gazebo's simulation clock.
 */
#include "sync_firmware.h"
#include "motor_map.h"
#include <math.h>
static bool sync_initialized, sync_bootstrapped, sync_fault;
static uint32_t sync_seq, sync_last_control, sync_last_safety, sync_arm_ms;
static uint64_t sync_operator_session, sync_target_revision;
static uint32_t sync_operator_run;
static unsigned sync_phase;

static bool sync_frame(uint8_t type,const uint8_t *body,size_t n) {
 uint8_t p[144]={0}; Sim_Write64(p,1U);
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
int npulse_sync_init(void) {
 if(sync_initialized)return 0;
 realtime=false;Motor_InitAll();Comms_Init();Comms_TaskCreate();
 if(!UART4Transport_StartRx((void *)2))return -1;
 sync_initialized=true;return 0;
}
void npulse_sync_stop(void) {
 if(!sync_initialized)return;
 Motor_StopAll();Motor_ServiceStop();sync_phase=0;
}
static void sync_fail(void){Motor_StopAll();Motor_ServiceStop();sync_phase=3;sync_fault=true;}
static bool sync_target(double value,uint8_t limit) {
 uint8_t p[7];p[0]=1U;p[1]=(uint8_t)MOTOR_INDEX;
 Sim_WriteFloat(p+2,(float)value);p[6]=limit;
 return sync_frame(SIM_TARGET,p,sizeof(p));
}
void npulse_sync_step(uint32_t ms,const double q[8],const double v[8],const double u[8],
 int enabled,uint64_t operator_session,uint32_t operator_run,uint64_t revision,
 double target,uint8_t limit,struct npulse_sync_result *out) {
 if(!sync_initialized || !out)return;
 clock_ms=ms;rtos_ms=ms;current_task=2U;
 if(!sync_bootstrapped){
  uint8_t config[4]={1,0,255,0};
  if(!sync_frame(SIM_HELLO,NULL,0)||!sync_frame(SIM_CONFIG,config,sizeof(config))){sync_fail();}
  else sync_bootstrapped=true;
 }
 if(sync_bootstrapped){
  if(!sync_frame(SIM_HEARTBEAT,NULL,0)||!sync_feedback(q,v,u))sync_fail();
 }
 if(!enabled){
  if(sync_phase==1 || sync_phase==2)npulse_sync_stop();
 }else if(operator_session!=sync_operator_session || operator_run!=sync_operator_run){
  // Only a new explicit operator run can recover a stop; heartbeats never rearm.
  sync_operator_session=operator_session;sync_operator_run=operator_run;
  sync_fault=false;sync_phase=0;
  sim_diagnostics_t previous;SimSession_GetDiagnostics(&previous);
  if(previous.armed)npulse_sync_stop();
  MotorGuard_Poll();Motor_ServiceStop();
  if(!sync_frame(SIM_RECOVER,NULL,0)||!sync_frame(SIM_ARM,NULL,0))sync_fail();
  else{sync_phase=1;sync_arm_ms=ms;}
 }else if(sync_phase==1 && ms!=sync_arm_ms){
  uint8_t mask=2U;
  if(!sync_frame(SIM_ENABLE,&mask,1)||!sync_target(target,limit))sync_fail();
  else{sync_phase=2;sync_target_revision=revision;}
 }else if(sync_phase==2 && revision!=sync_target_revision){
  if(!sync_target(target,limit))sync_fail();else sync_target_revision=revision;
 }
 if((uint32_t)(ms-sync_last_control)>=10U){sync_last_control=ms;Motor_Update();}
 if((uint32_t)(ms-sync_last_safety)>=20U){
  sync_last_safety=ms;MotorGuard_Poll();MotorSafety_Poll();Motor_ServiceStop();
 }
 sim_diagnostics_t d;SimSession_GetDiagnostics(&d);
 uint32_t causes=MotorSafety_GetStopCauses();
 if(enabled && sync_phase==2 && (causes || !d.armed || !Motor_IsEnabled(MOTOR_INDEX))){sync_fault=true;sync_phase=3;}
 motor_state_t m;Motor_GetSnapshot(MOTOR_INDEX,&m);
 memset(out,0,sizeof(*out));out->sim_ms=ms;out->run=d.run;out->causes=causes;
 out->phase=(uint8_t)(sync_fault?3:sync_phase);out->armed=d.armed;out->enabled=m.enabled;
 out->feedback_age_ms=ms-m.feedback_ms;
 out->target=m.target_position;out->reference=m.reference_position;out->position=m.measured_position;
 out->velocity=v[1];out->output=(enabled&&sync_phase==2&&!causes)?m.output_pwm:0;
 out->kp=m.pid.config.kp;out->ki=m.pid.config.ki;out->kd=m.pid.config.kd;
 assert(!primask&&!physical_calls);
}
