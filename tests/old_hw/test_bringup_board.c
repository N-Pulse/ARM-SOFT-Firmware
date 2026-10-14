/* One identical DLL image can run both PC0 roles, with all real OLD_HW modules
 * and the real USART3 adapter. Only HAL/register/RTOS/entropy boundaries modeled. */
#define OLD_HW_INTEGRATION
#include "test_old_hw.c"
#include "hw_commission.h"
#include "comms_session.h"
#include "motor_guard.h"
#include "adc_dma_bsp.h"
#include "../../fw/comm/board_link_runtime.c"
#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))
#endif
RNG_TypeDef mock_rng;
static motor_id_t selected=(motor_id_t)N_PULSE_HW_TEST_MOTOR;
static uint32_t request;
bool ADC_DMA_BSP_Init(void) { return true; }
bool ADC_DMA_BSP_GetSample(motor_id_t id,adc_current_sample_t *out)
{ (void)id; *out=(adc_current_sample_t){.voltage_v=NAN,.current_a=NAN,.state=ADC_CURRENT_INVALID}; return false; }
static void hello(void) {
    rx_result_t m={.type=RX_TYPE_HELLO,.proto_version=1,.data.device_id=1};
    assert(CommsSession_OnMessage(&m,now,MotorSafety_EpochLocked(),NULL));
}
static uint32_t command(hw_command_t c) {
    c.motor_id=selected; c.request_id=++request; c.epoch=MotorSafety_EpochLocked();
    return HWCommission_Execute(&c,now,c.epoch);
}
static uint16_t wire_length;
static uint32_t control_ms;
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *u,uint8_t *p,uint16_t n)
{ assert(u==&huart3 && p==&rx_byte && n==1); return HAL_OK; }
HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef *u,uint8_t *p,uint16_t n)
{ assert(u==&huart3 && p==tx && masked && n<=BL_WIRE_MAX); wire_length=n; return HAL_OK; }
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *u) { assert(u==&huart3 && !masked); return HAL_OK; }
HAL_StatusTypeDef HAL_UART_AbortTransmit(UART_HandleTypeDef *u) { assert(u==&huart3 && !masked); wire_length=0; return HAL_OK; }
void vTaskDelay(TickType_t ticks) { assert(!masked); now+=ticks; }
void vTaskDelete(TaskHandle_t task) { (void)task; assert(!masked); }
TaskHandle_t xTaskCreateStatic(void (*fn)(void *),const char *name,uint32_t depth,void *arg,
                               uint32_t priority,StackType_t *stack,StaticTask_t *tcb)
{ assert(fn && name && depth==768 && !arg && priority==2 && stack && tcb); return tcb; }
EXPORT void B4_Init(uint32_t role,uint32_t clock,uint64_t session)
{
    assert(setjmp(fatal)==0); now=clock; slave=role==BOARD_ROLE_SLAVE;
    BoardLink_Init(); OldHW_HoldOutputsLow(); OldHW_ClockConfig(); OldHW_Init();
    Motor_InitAll(); CommsSession_Init(true); hello(); BoardLink_RuntimeInit();
    assert(BL_Init(&link,(uint8_t)BoardLink_Role(),session,now,(bl_hooks_t){stop_hook,target_hook,NULL}));
    started=true; HAL_NVIC_EnableIRQ(USART3_IRQn); HAL_UART_Receive_IT(&huart3,&rx_byte,1);
    control_ms=now; BoardLink_SafetyPollLocked(now); assert(OldHW_OutputsDisabled() && !Motor_L298N_IsArmed());
}
EXPORT void B4_Step(uint32_t elapsed,uint32_t keep_host)
{
    now+=elapsed; if(keep_host) hello();
    if(now-control_ms>=10U) { control_ms=now; Motor_Update(); MotorGuard_Poll(); }
    MotorSafety_Poll(); Motor_ServiceStop(); service();
}
EXPORT uint32_t B4_Take(uint8_t *out)
{
    uint32_t n=wire_length; if(!n) return 0;
    memcpy(out,tx,n); wire_length=0;
    huart3.gState=HAL_UART_STATE_READY; huart3.TxXferCount=0; BoardLink_TxCallback(&huart3);
    return n;
}
EXPORT void B4_Feed(const uint8_t *p,uint32_t n)
{ for(unsigned i=0;i<n;++i) { rx_byte=p[i]; BoardLink_RxCallback(&huart3); } }
EXPORT uint32_t B4_Operator(uint32_t op)
{ return BoardLink_Operator(op,++request,MotorSafety_EpochLocked(),now); }
EXPORT uint32_t B4_Command(uint32_t op)
{ return command((hw_command_t){.operation=op,.stationary_reference_verified=true}); }
EXPORT uint32_t B4_Configure(void)
{
    uint32_t r=command((hw_command_t){.operation=1,.minimum_rad=0,.maximum_rad=.3f,
        .maximum_step_rad=.05f,.pwm_percent=3,.reference_rate_rad_s=.1f}); if(r) return r;
    r=command((hw_command_t){.operation=2,.cpr=1000,.quadrature=4,.gear_ratio=1,.direction=1}); if(r) return r;
    return command((hw_command_t){.operation=3});
}
EXPORT uint32_t B4_Target(void) { return command((hw_command_t){.operation=7,.target_rad=.02f,.pwm_percent=3}); }
EXPORT uint32_t B4_Fresh(void) { return BL_Fresh(&link,now); }
EXPORT uint32_t B4_Faults(void) { return MotorSafety_GetStopCauses(); }
EXPORT uint32_t B4_Disabled(void) { return OldHW_OutputsDisabled() && !Motor_L298N_IsArmed(); }
EXPORT uint32_t B4_Epoch(void) { return MotorSafety_EpochLocked(); }
EXPORT uint32_t B4_Role(void) { return (uint32_t)BoardLink_Role(); }
EXPORT float B4_PWM(void) { return Motor_L298N_AppliedPWM(selected); }
EXPORT void B4_Stop(void) { Motor_StopAll(); }
EXPORT void B4_EncoderFault(void) { TIM2->CR1&=~TIM_CR1_CEN; TIM3->CR1&=~TIM_CR1_CEN; }
EXPORT uint32_t B4_RemoteRaw(uint32_t id) { bl_feedback_t f; return BoardLink_GetRemote((motor_id_t)id,&f) && (f.flags&BL_RAW_VALID); }
