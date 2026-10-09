#define TEST_REAL_BSP
#define main unused_safety_suite
#include "test_safety.c"
#undef main
#include "../../fw/bsp/encoder/encoder_bsp.c"
TIM_TypeDef encoder_timers[21];
TIM_HandleTypeDef htim1={TIM1}, htim2={TIM2}, htim3={TIM3}, htim4={TIM4}, htim5={TIM5}, htim8={TIM8}, htim20={TIM20};
static TIM_TypeDef lp;
LPTIM_HandleTypeDef hlptim1={&lp};
static unsigned encoder_starts;
HAL_StatusTypeDef HAL_TIM_Encoder_Start(TIM_HandleTypeDef *h, uint32_t ch) { (void)h; assert(!primask && ch == TIM_CHANNEL_ALL); ++encoder_starts; return HAL_OK; }
HAL_StatusTypeDef HAL_LPTIM_Encoder_Start(LPTIM_HandleTypeDef *h, uint32_t n) { assert(!primask && h == &hlptim1 && n == 65535U); ++encoder_starts; return HAL_OK; }
static void reference_injection(int mode) {
    encoder_calibration_t c={1000,4,1.0f,1,100};
    if(mode==8) { assert(!Encoder_BSP_SetCalibration(MOTOR_INDEX,&c)); return; }
    Motor_Disable(MOTOR_INDEX);
    assert(Encoder_BSP_SetCalibration(MOTOR_INDEX,&c));
    assert(!Motor_HasFreshPositionFeedback(MOTOR_INDEX));
    assert(!Motor_Enable(MOTOR_INDEX));
}
int main(void) {
    reset(); assert(encoder_starts == 8U);
    assert(!Motor_Enable(MOTOR_INDEX));
    encoder_calibration_t c={1000,4,1.0f,1,100};
    assert(Encoder_BSP_SetCalibration(MOTOR_INDEX,&c));
    assert(!Motor_HasFreshPositionFeedback(MOTOR_INDEX));
    assert(Encoder_BSP_Reset(MOTOR_INDEX)); step(10U,false); arm(); enable(MOTOR_INDEX); step(10U,false);
    assert(Motor_DRV8848_DrivingDir(MOTOR_INDEX) != 0);
    assert(!Encoder_BSP_SetCalibration(MOTOR_INDEX,&c));
    assert(!Encoder_BSP_Reset(MOTOR_INDEX)); assert(!Encoder_BSP_ResetAll());
    assert(Motor_IsEnabled(MOTOR_INDEX));
    Motor_Disable(MOTOR_INDEX); assert(Encoder_BSP_SetCalibration(MOTOR_INDEX,&c));
    assert(!Motor_HasFreshPositionFeedback(MOTOR_INDEX)); assert(!Motor_Enable(MOTOR_INDEX));
    assert(Encoder_BSP_Reset(MOTOR_INDEX)); step(10U,false);
    assert(Motor_Enable(MOTOR_INDEX)); Motor_SetTarget(MOTOR_INDEX,1.0f,80U);
    injection=8; step(10U,false); assert(Motor_IsEnabled(MOTOR_INDEX));
    injection=7; step(10U,false); assert(!Motor_IsEnabled(MOTOR_INDEX));
    assert(Motor_DRV8848_DrivingDir(MOTOR_INDEX)==0);
    assert(Encoder_BSP_Reset(MOTOR_INDEX)); step(10U,false); enable(MOTOR_INDEX);
    /* A disabled controller may change reference while another PID is prepared;
     * accepted mutation must invalidate that controller's revision/feedback. */
    s_raw_count[MOTOR_INDEX]=INT64_MAX; TIM8->CNT=1;
    MotorBackend_RefreshFeedback();
    assert(!Encoder_BSP_IsCalibrated(MOTOR_INDEX));
    assert(!Motor_HasFreshPositionFeedback(MOTOR_INDEX)); assert_stopped();
    assert(!Motor_RecoverFromFault());
    assert(Encoder_BSP_Reset(MOTOR_INDEX)); step(10U,false);
    assert(Motor_RecoverFromFault()); assert_stopped();
    assert(Encoder_BSP_Init()); assert(encoder_starts == 8U);
    puts("PASS real BSP -> HW feedback -> interlock: active reference refusal, invalidation, accumulation fault, reset recovery, eight sole starts");
    return 0;
}
