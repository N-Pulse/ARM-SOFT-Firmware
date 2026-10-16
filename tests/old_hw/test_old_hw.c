#include "old_hw.h"
#include "board_link.h"
#include "encoder_bsp.h"
#include "drv8848.h"
#include "motor_safety.h"
#include "fatal_stop.h"
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

TIM_TypeDef mock_timers[21]; GPIO_TypeDef mock_gpio[4]; RCC_TypeDef mock_rcc;
USART_TypeDef mock_uarts[2];
EXTI_TypeDef mock_exti; SYSCFG_TypeDef mock_syscfg;
TIM_HandleTypeDef htim1,htim2,htim3,htim4,htim8,htim15,htim20;
UART_HandleTypeDef huart3;
static jmp_buf fatal;
static bool slave;
static bool fail_exti;
static bool irq_enabled[3];
static unsigned masked, calls, fail_at, encoder_starts, pwm_channels, init_mask;
#ifndef OLD_HW_INTEGRATION
static unsigned stops;
#endif
static uint32_t now;
static HAL_StatusTypeDef checked(void) { return ++calls == fail_at ? HAL_ERROR : HAL_OK; }
void Error_Handler(void) { longjmp(fatal,1); }
uint32_t __get_PRIMASK(void) { return masked; }
uint32_t __get_IPSR(void) { return 0; }
void __disable_irq(void) { masked=1; }
#ifdef OLD_HW_INTEGRATION
static void (*deferred_irq)(void);
#endif
void __set_PRIMASK(uint32_t p) {
    masked=p;
#ifdef OLD_HW_INTEGRATION
    if (!p && deferred_irq) { void (*irq)(void)=deferred_irq; deferred_irq=NULL; irq(); }
#endif
}
void __NOP(void) {}
void __DSB(void)
{
    for(unsigned i=0;i<4;++i) {
        mock_gpio[i].ODR &= ~(mock_gpio[i].BSRR >> 16);
        mock_gpio[i].ODR |= mock_gpio[i].BSRR & 0xFFFFU;
        mock_gpio[i].BSRR=0;
    }
}
uint32_t HAL_GetTick(void) { return now; }
#ifndef OLD_HW_INTEGRATION
bool Motor_IsEnabled(motor_id_t id) { (void)id; return false; }
void Motor_InvalidatePositionFeedback(motor_id_t id) { (void)id; }
void MotorSafety_RequestStopCause(motor_stop_cause_t cause,motor_id_t id)
{ assert(cause==MOTOR_STOP_DRIVER && id==MOTOR_COUNT); ++stops; }
void MotorSafety_RequestStopCauseLocked(motor_stop_cause_t cause,motor_id_t id)
{ (void)cause; (void)id; ++stops; }
void Motor_ServiceStop(void) {}
#endif
void HAL_GPIO_WritePin(GPIO_TypeDef *p,uint16_t pins,GPIO_PinState s)
{ if(s==GPIO_PIN_RESET) p->ODR &= ~pins; else p->ODR |= pins; }
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *p,uint16_t pin)
{ assert(p==GPIOC && pin==GPIO_PIN_0); return slave?GPIO_PIN_RESET:GPIO_PIN_SET; }
void HAL_GPIO_TogglePin(GPIO_TypeDef *p,uint16_t pin)
{ assert(p==GPIOA && pin==GPIO_PIN_5); p->ODR ^= pin; }
void HAL_GPIO_Init(GPIO_TypeDef *p,const GPIO_InitTypeDef *g)
{
    for(unsigned i=0;i<16;++i) if(g->Pin & (1U<<i)) {
        if(g->Mode==GPIO_MODE_OUTPUT_PP) {
            assert(g_old_hw_role!=BOARD_ROLE_UNKNOWN);
            assert(!(p->ODR & (1U<<i))); /* Must latch low before switching mode. */
        }
        unsigned mode=g->Mode==GPIO_MODE_IT_RISING_FALLING ? 0U : g->Mode;
        p->MODER=(p->MODER & ~(3U<<(2*i))) | mode<<(2*i);
        if(g->Mode==GPIO_MODE_IT_RISING_FALLING && !fail_exti) {
            unsigned shift=(i%4)*4;
            SYSCFG->EXTICR[i/4]=(SYSCFG->EXTICR[i/4]&~(15U<<shift)) | 1U<<shift;
            EXTI->IMR1 |= 1U<<i; EXTI->RTSR1 |= 1U<<i; EXTI->FTSR1 |= 1U<<i;
        }
        p->PUPDR=(p->PUPDR & ~(3U<<(2*i))) | g->Pull<<(2*i);
        if(g->Mode==GPIO_MODE_AF_PP) {
            unsigned reg=i/8,shift=(i%8)*4;
            p->AFR[reg]=(p->AFR[reg]&~(15U<<shift)) | g->Alternate<<shift;
            assert(!(p==GPIOC && (i==10 || i==11))); /* No UART4 pin configuration. */
        }
    }
}
HAL_StatusTypeDef HAL_PWREx_ControlVoltageScaling(uint32_t s) { (void)s; return checked(); }
HAL_StatusTypeDef HAL_RCC_OscConfig(const RCC_OscInitTypeDef *o)
{ assert(o->PLL.PLLM==4 && o->PLL.PLLN==85 && o->PLL.PLLR==2); return checked(); }
HAL_StatusTypeDef HAL_RCC_ClockConfig(const RCC_ClkInitTypeDef *c,uint32_t l)
{ assert(c->AHBCLKDivider==1 && c->APB1CLKDivider==1 && c->APB2CLKDivider==1 && l==4); return checked(); }
HAL_StatusTypeDef HAL_RCCEx_PeriphCLKConfig(const RCC_PeriphCLKInitTypeDef *c)
{ (void)c; return checked(); }
uint32_t HAL_RCC_GetHCLKFreq(void) { return 170000000; }
uint32_t HAL_RCC_GetPCLK1Freq(void) { return 170000000; }
uint32_t HAL_RCC_GetPCLK2Freq(void) { return 170000000; }
HAL_StatusTypeDef HAL_TIM_PWM_Init(TIM_HandleTypeDef *h)
{ assert(h->Init.Period==8499 && h->Init.Prescaler==0); h->Instance->PSC=0; h->Instance->ARR=8499; return checked(); }
HAL_StatusTypeDef HAL_TIM_PWM_ConfigChannel(TIM_HandleTypeDef *h,const TIM_OC_InitTypeDef *o,uint32_t ch)
{
    assert(o->Pulse==0 && ch%4==0);
    assert((slave && (h->Instance==TIM1 || ch==TIM_CHANNEL_4)) ||
           (!slave && h->Instance==TIM4 && ch!=TIM_CHANNEL_4));
    ++pwm_channels; return checked();
}
HAL_StatusTypeDef HAL_TIM_Encoder_Init(TIM_HandleTypeDef *h,const TIM_Encoder_InitTypeDef *e)
{
    assert(h->Instance!=TIM15 && e->IC1Filter==4 && e->IC2Filter==4);
    unsigned index=(unsigned)(h->Instance-mock_timers);
    assert(!(init_mask & (1U<<index))); init_mask |= 1U<<index;
    assert(h->Init.Period==(index==2 ? UINT32_MAX : UINT16_MAX));
    h->Instance->SMCR=TIM_ENCODERMODE_TI12;
    return checked();
}
HAL_StatusTypeDef HAL_TIM_Encoder_Start(TIM_HandleTypeDef *h,uint32_t channel)
{ assert(h->Instance!=TIM15 && channel==TIM_CHANNEL_ALL); ++encoder_starts;
  h->Instance->CR1 |= TIM_CR1_CEN; h->Instance->CCER=TIM_CCER_CC1E|TIM_CCER_CC2E; return checked(); }
HAL_StatusTypeDef HAL_UART_Init(UART_HandleTypeDef *h)
{ assert(h->Init.BaudRate==115200 && (h->Instance==USART3 || h->Instance==LPUART1)); return checked(); }
static bool rx_fifo_threshold,tx_fifo_threshold;
HAL_StatusTypeDef HAL_UARTEx_SetRxFifoThreshold(UART_HandleTypeDef *h,uint32_t threshold) {
    assert(h->Instance==USART3 && threshold==UART_RXFIFO_THRESHOLD_1_8);
    rx_fifo_threshold=true; return checked();
}
HAL_StatusTypeDef HAL_UARTEx_SetTxFifoThreshold(UART_HandleTypeDef *h,uint32_t threshold) {
    assert(h->Instance==USART3 && threshold==UART_TXFIFO_THRESHOLD_1_8);
    tx_fifo_threshold=true; return checked();
}
HAL_StatusTypeDef HAL_UARTEx_EnableFifoMode(UART_HandleTypeDef *h) {
    assert(h->Instance==USART3 && rx_fifo_threshold && tx_fifo_threshold);
    h->Instance->CR1 |= USART_CR1_FIFOEN; return checked();
}
HAL_StatusTypeDef HAL_UARTEx_DisableFifoMode(UART_HandleTypeDef *h) {
    assert(h->Instance==LPUART1); h->Instance->CR1 &= ~USART_CR1_FIFOEN; return checked();
}
void HAL_NVIC_DisableIRQ(IRQn_Type irq) { irq_enabled[irq]=false; }
void HAL_NVIC_EnableIRQ(IRQn_Type irq) {
#if N_PULSE_DUAL_BOARD_LINK
    assert(irq==EXTI15_10_IRQn || irq==USART3_IRQn);
#else
    assert(irq==EXTI15_10_IRQn);
#endif
    irq_enabled[irq]=true;
}
uint32_t NVIC_GetEnableIRQ(IRQn_Type irq) { return irq_enabled[irq]; }
void HAL_NVIC_ClearPendingIRQ(IRQn_Type irq) { (void)irq; }
void HAL_NVIC_SetPriority(IRQn_Type irq,uint32_t priority,uint32_t sub)
{ assert(priority==(irq==EXTI15_10_IRQn?3U:6U) && sub==0); }

static void __attribute__((unused)) fatal_checks(void)
{
    /* Fatal handler must work without clocks/handles/role being initialized. */
    memset(mock_gpio,0xFF,sizeof mock_gpio);
    for(unsigned i=0;i<4;++i) mock_gpio[i].BSRR=0;
    memset(mock_timers,0xFF,sizeof mock_timers);
    mock_rcc=(RCC_TypeDef){0};
    N_PULSE_FatalCutoff(N_PULSE_FATAL_ERROR);
    assert(masked && TIM1->CCR1==UINT32_MAX && TIM4->CCR1==UINT32_MAX);
    assert(GPIOA->ODR==~0x0F00U && GPIOB->ODR==~0x03C0U);
    assert((GPIOA->MODER & 0x00FF0000U)==0x00550000U);
    assert((GPIOB->MODER & 0x000FF000U)==0x00055000U);
    assert((GPIOA->MODER & ~0x00FF0000U)==~0x00FF0000U);
    assert(GPIOC->MODER==UINT32_MAX && GPIOD->MODER==UINT32_MAX);
    mock_rcc.APB2ENR=RCC_APB2ENR_TIM1EN;
    N_PULSE_FatalCutoff(N_PULSE_FATAL_HARDFAULT);
    assert(!(TIM1->BDTR & (TIM_BDTR_MOE|TIM_BDTR_AOE)) && !TIM1->CCER && !(TIM1->CR1&1));
    assert(!(TIM1->CCR1|TIM1->CCR2|TIM1->CCR3|TIM1->CCR4));
    assert(TIM4->CCR1==UINT32_MAX);
    mock_rcc.APB1ENR1=RCC_APB1ENR1_TIM4EN;
    N_PULSE_FatalCutoff(N_PULSE_FATAL_ASSERT);
    assert(!TIM4->CCER && !(TIM4->CR1&1) && !(TIM4->CCR1|TIM4->CCR2|TIM4->CCR3|TIM4->CCR4));
    assert(g_n_pulse_fatal_cause==N_PULSE_FATAL_ASSERT);
}

#ifndef OLD_HW_INTEGRATION
int main(int argc,char **argv)
{
    slave=argc>1 && atoi(argv[1]); fail_at=argc>2 ? (unsigned)atoi(argv[2]) : 0;
    memset(mock_gpio,0,sizeof mock_gpio);
    if(setjmp(fatal)) { assert(fail_at && OldHW_OutputsDisabled()); puts("PASS OLD_HW HAL initialization failure remains disabled"); return 0; }
    BoardLink_Init();
    assert(BoardLink_Role()==(slave?BOARD_ROLE_SLAVE:BOARD_ROLE_MASTER));
    slave=!slave; BoardLink_Init(); slave=!slave; /* Latched, never resampled. */
    assert(BoardLink_Role()==(slave?BOARD_ROLE_SLAVE:BOARD_ROLE_MASTER));
    OldHW_HoldOutputsLow(); OldHW_ClockConfig(); OldHW_Init();
    assert(USART3->CR1 & USART_CR1_FIFOEN);
    assert(!(LPUART1->CR1 & USART_CR1_FIFOEN));
    assert(pwm_channels==(slave?5U:3U) && OldHW_OutputsDisabled());
    assert(OldHW_HostEnabled() && !encoder_starts);
    assert(g_old_hw_encoder_unavailable_mask==(slave ? 1U<<MOTOR_THUMB : 0));
    if (!Encoder_BSP_Init()) {
        assert(fail_at && !Encoder_BSP_Init() && OldHW_OutputsDisabled());
        puts("PASS OLD_HW encoder start failure propagates; no hidden retry or output enable");
        return 0;
    }
    assert(encoder_starts==(slave?4U:3U));
    for(unsigned i=0;i<MOTOR_COUNT;++i) {
        bool master=i==MOTOR_WRIST_X || i==MOTOR_WRIST_Y || i==MOTOR_LITTLE;
        assert(BoardLink_IsLocalMotor((motor_id_t)i)==(slave?!master:master));
        assert(Encoder_BSP_IsLocal((motor_id_t)i)==(slave?!master:master));
        encoder_snapshot_t sample;
        assert(!Encoder_BSP_GetSnapshot((motor_id_t)i,&sample)); /* No default mechanical calibration. */
    }
    assert(!BoardLink_IsLocalMotor(MOTOR_COUNT) && !BoardLink_IsLocalMotor((motor_id_t)-1));
    motor_id_t local=slave?MOTOR_INDEX:MOTOR_WRIST_X;
    TIM2->CNT=UINT32_MAX;
    assert(Encoder_BSP_GetRawCount(local)==-1);
    TIM2->CNT=0; now=10; assert(Encoder_BSP_GetRawCount(local)==0);
    TIM2->CNT=70000; assert(Encoder_BSP_GetRawCount(local)==70000); /* 32-bit, not 16-bit truncation. */
    motor_id_t short_counter=slave?MOTOR_MIDDLE:MOTOR_LITTLE;
    TIM3->CNT=UINT16_MAX; assert(Encoder_BSP_GetRawCount(short_counter)==-1);
    TIM3->CNT=0; assert(Encoder_BSP_GetRawCount(short_counter)==0);
    Motor_DRV8848_Init();
    assert(!Motor_DRV8848_Arm() && !Motor_DRV8848_IsArmed() && !Motor_DRV8848_FaultActive());
    Motor_DRV8848_SetRaw(local,100); Motor_DRV8848_Brake(local);
    BoardLink_SendMotorCmd(local,1,100);
    assert(stops==1 && g_old_hw_rejected_commands==3 && OldHW_OutputsDisabled());
    assert(Motor_DRV8848_CleanupStoppedInternal());
    TIM4->CCER=1; assert(!OldHW_OutputsDisabled());
    Motor_DRV8848_InhibitLocked(); assert(OldHW_OutputsDisabled());
    OldHW_Heartbeat(); /* Checks PA5, never the PB10 USART3 line. */
    assert(!masked && encoder_starts==(slave?4U:3U));
    assert(!fail_at); /* A requested failure index must actually be exercised. */
    fatal_checks();
    printf("PASS OLD_HW %s: role, safe pins, clocks, local encoders + THUMB EXTI, default inhibit, fatal cutoff (%u HAL calls)\n",slave?"SLAVE":"MASTER",calls);
    return 0;
}
#endif
