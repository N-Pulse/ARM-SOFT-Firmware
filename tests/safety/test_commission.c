/* Reuse the existing hardware/register fixture AND real encoder BSP. */
#define TEST_REAL_BSP
#define main unused_safety_suite
#include "test_safety.c"
#undef main
#include "../../fw/bsp/encoder/encoder_bsp.c"
TIM_TypeDef encoder_timers[21];
TIM_HandleTypeDef htim1={TIM1},htim2={TIM2},htim3={TIM3},htim4={TIM4},htim5={TIM5},htim8={TIM8},htim20={TIM20};
static TIM_TypeDef lp;
LPTIM_HandleTypeDef hlptim1={&lp};
HAL_StatusTypeDef HAL_TIM_Encoder_Start(TIM_HandleTypeDef *h,uint32_t c) { (void)h; (void)c; return HAL_OK; }
HAL_StatusTypeDef HAL_LPTIM_Encoder_Start(LPTIM_HandleTypeDef *h,uint32_t n) { (void)h; (void)n; return HAL_OK; }
static void reference_injection(int mode) { (void)mode; }
#include "hw_commission.h"
#include "comms_session.h"
#include "motor_lock.h"
#include "motor_estimation.h"
static uint32_t request_id;
static uint32_t command(hw_command_t c) {
    c.request_id=++request_id; c.epoch=MotorSafety_EpochLocked();
    c.motor_id=MOTOR_INDEX;
    return HWCommission_Execute(&c,clock_ms,c.epoch);
}
static void hello(void) {
    bool begun; rx_result_t r={.type=RX_TYPE_HELLO,.proto_version=1,.data.device_id=1};
    assert(CommsSession_OnMessage(&r,clock_ms,MotorSafety_EpochLocked(),&begun));
}
int main(void) {
    reset(); CommsSession_Init(true); assert_stopped();
    assert(!Motor_DRV8848_Arm());
    assert(command((hw_command_t){.operation=5})==HW_SESSION);
    hello(); assert(command((hw_command_t){.operation=5})==HW_PARAMETERS);
    assert(command((hw_command_t){.operation=1,.minimum_rad=0,.maximum_rad=.3f,
        .maximum_step_rad=.05f,.pwm_percent=3,.reference_rate_rad_s=.1f})==HW_OK);
    assert(command((hw_command_t){.operation=5})==HW_CALIBRATION);
    assert(command((hw_command_t){.operation=2,.cpr=1000,.quadrature=4,.gear_ratio=1,.direction=1})==HW_OK);
    assert(command((hw_command_t){.operation=3})==HW_OK);
    step(10,false);
    for(unsigned i=0;i<MOTOR_COUNT;++i) if(i!=MOTOR_INDEX) {
        assert(!Encoder_BSP_IsCalibrated((motor_id_t)i));
        assert(!Motor_Enable((motor_id_t)i));
    }
    assert(command((hw_command_t){.operation=5})==HW_OK);
    assert(command((hw_command_t){.operation=6})==HW_OK);
    assert(command((hw_command_t){.operation=3})==HW_DISABLE_FIRST);
    assert(command((hw_command_t){.operation=7,.target_rad=.2f,.pwm_percent=3})==HW_PARAMETERS);
    assert(command((hw_command_t){.operation=7,.target_rad=.02f,.pwm_percent=3})==HW_OK);
    step(10,false); motor_state_t m; Motor_GetSnapshot(MOTOR_INDEX,&m);
    assert(m.output_pwm>0 && m.output_pwm<=3);
    for(unsigned i=0;i<MOTOR_COUNT;++i) if(i!=MOTOR_INDEX) {
        Motor_SetTarget((motor_id_t)i,.02f,100); Motor_DRV8848_SetRaw((motor_id_t)i,100);
        assert(!Motor_IsEnabled((motor_id_t)i) && Motor_DRV8848_DrivingDir((motor_id_t)i)==0);
    }
    uint32_t old_epoch=MotorSafety_EpochLocked(); Motor_StopAll(); assert_stopped();
    step(10,false); assert(command((hw_command_t){.operation=4})==HW_OK); assert_stopped();
    hw_command_t old={.operation=5,.request_id=++request_id,.epoch=old_epoch,.motor_id=MOTOR_INDEX};
    assert(HWCommission_Execute(&old,clock_ms,old_epoch)==HW_STALE); assert_stopped();
    assert(command((hw_command_t){.operation=5})==HW_OK);
    assert(command((hw_command_t){.operation=6})==HW_OK);
    step(10,false); Motor_GetSnapshot(MOTOR_INDEX,&m); assert(m.output_pwm==0);
    hardware_fault=true; MotorSafety_Poll(); assert_stopped();
    assert(command((hw_command_t){.operation=4})==HW_FAULT);
    hardware_fault=false; step(10,false); assert(command((hw_command_t){.operation=4})==HW_OK);
    assert_stopped();
    motor_kalman_config_t synthetic={.inertia_kg_m2=1,.torque_nm_a=1,.q_position=.001,.q_velocity=.01,
        .r_position=.001,.initial_position_variance=1,.initial_velocity_variance=1,.innovation_limit_rad=.5,
        .missing_current_q_factor=10,.max_dt_s=.1,.consecutive_limit=3,.current_max_age_ms=50,
        .encoder_max_age_ms=100,.encoder_to_motor_ratio=1};
    MotorEstimation_Init(); assert_stopped();
    assert(MotorEstimation_Configure(MOTOR_INDEX,&synthetic));
    assert(!MotorEstimation_Reset(MOTOR_INDEX,false));
    assert(MotorEstimation_Reset(MOTOR_INDEX,true));
    step(10,false); MotorEstimation_Service(); MotorEstimation_Stop(MOTOR_INDEX); assert_stopped();
    puts("PASS commissioning: missing parameters/session/calibration, one of eight enabled, bounded PID, disabled reference, STOP epoch, explicit recovery, global fault; no automatic motion");
    return 0;
}
