#include "main.h"
#include "bench_hw.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

GPIO_TypeDef mock_gpioa, mock_gpiob, mock_gpioc;
RCC_TypeDef mock_rcc;
TIM_TypeDef mock_tim2, mock_tim3, mock_tim4;
USART_TypeDef mock_uart;
uint32_t mock_primask, mock_tick;
static unsigned gpio_initializations, uart_initializations;

HAL_StatusTypeDef HAL_Init(void) { return HAL_OK; }
uint32_t HAL_GetTick(void) { return mock_tick; }
HAL_StatusTypeDef HAL_PWREx_ControlVoltageScaling(uint32_t value) { assert(value == 1U); return HAL_OK; }
HAL_StatusTypeDef HAL_RCC_OscConfig(RCC_OscInitTypeDef *v)
{ assert(v->PLL.PLLN == 85U && v->PLL.PLLM == 4U && v->PLL.PLLR == 2U); return HAL_OK; }
HAL_StatusTypeDef HAL_RCC_ClockConfig(RCC_ClkInitTypeDef *v, uint32_t latency)
{ assert(v->APB1CLKDivider == 1U && v->APB2CLKDivider == 1U && latency == 4U); return HAL_OK; }
uint32_t HAL_RCC_GetHCLKFreq(void) { return 170000000U; }
uint32_t HAL_RCC_GetPCLK1Freq(void) { return 170000000U; }
uint32_t HAL_RCC_GetPCLK2Freq(void) { return 170000000U; }
void HAL_GPIO_Init(GPIO_TypeDef *p, GPIO_InitTypeDef *v)
{
    assert(p == GPIOA); /* Encoder/VCP/LED only; directions use safe register path. */
    assert(v->Pin == GPIO_PIN_5 || v->Pin == (GPIO_PIN_0 | GPIO_PIN_1) ||
        v->Pin == (GPIO_PIN_2 | GPIO_PIN_3) || v->Pin == (GPIO_PIN_6 | GPIO_PIN_7));
    if (v->Pin == 3U) assert(v->Alternate == 1U && v->Pull == 0U);
    if (v->Pin == 12U) assert(v->Alternate == 12U);
    gpio_initializations++;
}
void HAL_GPIO_WritePin(GPIO_TypeDef *p, uint32_t pin, uint32_t state)
{ assert(p == GPIOA && pin == 32U && state == 0U); }
HAL_StatusTypeDef HAL_TIM_PWM_Init(TIM_HandleTypeDef *v)
{ assert(v->Instance == TIM4); TIM4->PSC = v->Init.Prescaler; TIM4->ARR = v->Init.Period; return HAL_OK; }
HAL_StatusTypeDef HAL_TIM_PWM_ConfigChannel(TIM_HandleTypeDef *v, TIM_OC_InitTypeDef *oc, uint32_t channel)
{ assert(v->Instance == TIM4 && oc->OCMode == 6U && (channel == 4U || channel == 0U)); return HAL_OK; }
HAL_StatusTypeDef HAL_TIM_Encoder_Init(TIM_HandleTypeDef *v, TIM_Encoder_InitTypeDef *enc)
{
    assert((v->Instance == TIM2 && v->Init.Period == 0xFFFFFFFFU) || (v->Instance == TIM3 && v->Init.Period == 0xFFFFU));
    assert(enc->EncoderMode == 3U && enc->IC1Filter == 4U && enc->IC2Filter == 4U);
    v->Instance->SMCR = enc->EncoderMode;
    return HAL_OK;
}
HAL_StatusTypeDef HAL_TIM_Encoder_Start(TIM_HandleTypeDef *v, uint32_t channel)
{ assert((v->Instance == TIM2 || v->Instance == TIM3) && channel == TIM_CHANNEL_ALL); v->Instance->CR1 = 1U; v->Instance->CCER = 17U; return HAL_OK; }
HAL_StatusTypeDef HAL_RCCEx_PeriphCLKConfig(RCC_PeriphCLKInitTypeDef *v)
{ assert(v->Lpuart1ClockSelection == RCC_LPUART1CLKSOURCE_PCLK1); return HAL_OK; }
HAL_StatusTypeDef HAL_UART_Init(UART_HandleTypeDef *v)
{ assert(v->Instance == LPUART1 && v->Init.BaudRate == 115200U && v->Init.ClockPrescaler == 2U); uart_initializations++; return HAL_OK; }
HAL_StatusTypeDef HAL_UARTEx_DisableFifoMode(UART_HandleTypeDef *v)
{ assert(v->Instance == LPUART1); return HAL_OK; }
void HAL_NVIC_SetPriority(uint32_t irq, uint32_t preempt, uint32_t sub)
{ assert(irq == LPUART1_IRQn && preempt == 2U && sub == 0U); }
void HAL_NVIC_EnableIRQ(uint32_t irq) { assert(irq == LPUART1_IRQn); }

static void off(void)
{
    assert(TIM4->CCR1 == 0 && TIM4->CCR2 == 0 && !(TIM4->CCER & 17U) && !(TIM4->CR1 & 1U));
    assert(((GPIOB->MODER >> 12U) & 15U) == 5U);
    assert(((GPIOC->MODER >> 8U) & 3U) == 1U && ((GPIOC->MODER >> 10U) & 3U) == 1U);
    assert(((GPIOC->MODER >> 12U) & 3U) == 1U && ((GPIOC->MODER >> 16U) & 3U) == 1U);
    assert(GPIOB->BSRR == (192U << 16U) && GPIOC->BSRR == (368U << 16U));
    assert(BenchHW_AppliedPWM(0) == 0 && BenchHW_AppliedPWM(1) == 0);
}
static void receive(uint8_t byte)
{ LPUART1->ISR = USART_ISR_RXNE_RXFNE; LPUART1->RDR = byte; BenchHW_UART_ISR(); }
static void arm_at(uint32_t now)
{ mock_tick = now; BenchHW_Heartbeat(now); BenchHW_ControlAlive(now); assert(BenchHW_Arm(now)); }
static void drive(void)
{ mock_tick += 2U; BenchHW_SetMotors(5, -4); assert(TIM4->CCR2 == 425 && TIM4->CCR1 == 340); }
static void startup_pwm_and_reversal(void)
{
    BenchHW_Init(); off(); assert(gpio_initializations == 4 && uart_initializations == 1);
    assert(BenchHW_EncoderRunning(0) && BenchHW_EncoderRunning(1));
    assert(!BenchHW_Arm(0)); BenchHW_SetMotors(5, 5); off();
    arm_at(1000); drive();
    assert(BenchHW_AppliedPWM(0) == 5 && BenchHW_AppliedPWM(1) == -4);
    uint32_t egr = TIM4->EGR;
    BenchHW_SetMotors(-5, -4); /* Reversing M1 must not disable M2. */
    assert(TIM4->CCR2 == 0 && TIM4->CCR1 == 340 && (TIM4->CR1 & 1));
    assert(!(TIM4->CCER & 16U) && (TIM4->CCER & 1U));
    mock_tick++; BenchHW_SetMotors(-5, -4); assert(TIM4->CCR2 == 0);
    mock_tick++; BenchHW_SetMotors(-5, -4); assert(TIM4->CCR2 == 425);
    assert(BenchHW_AppliedPWM(0) == -5 && TIM4->EGR == egr);
    BenchHW_SetMotors(-4.999f, 0); assert(fabsf(BenchHW_AppliedPWM(0)) <= 5.0f && TIM4->CCR1 == 0);
    BenchHW_SetMotors(-50.0f, 0); assert(TIM4->CCR2 == 4250);
    BenchHW_SetMotors(-50.001f, 0); off(); assert(BenchHW_TakeAsyncFault() == BENCH_ASYNC_ESTOP);
    arm_at(1500); drive(); BenchHW_SetMotors(0, -50.0f); assert(TIM4->CCR1 == 4250);
    BenchHW_SetMotors(0, 50.001f); off(); assert(BenchHW_TakeAsyncFault() == BENCH_ASYNC_ESTOP);
    arm_at(2000); drive(); BenchHW_SetMotors(5, NAN); off();
    assert(BenchHW_TakeAsyncFault() == BENCH_ASYNC_ESTOP);
}
static void asynchronous_shutdown(void)
{
    uint8_t byte;
    arm_at(3000); drive(); receive('!'); off();
    assert(!BenchHW_Arm(mock_tick)); assert(BenchHW_TakeAsyncFault() == BENCH_ASYNC_ESTOP);
    assert(BenchHW_ReadByte(&byte) && byte == 0); assert(BenchHW_ReadByte(&byte) && byte == '\n'); receive('\n');
    arm_at(4000); drive(); BenchHW_TickISR(4051); off(); assert(BenchHW_TakeAsyncFault() == BENCH_ASYNC_LOOP);
    arm_at(5000); drive(); BenchHW_ControlAlive(5501); BenchHW_TickISR(5501); off();
    assert(BenchHW_TakeAsyncFault() == BENCH_ASYNC_LINK);
    for (unsigned i = 0; i < 2; ++i) {
        arm_at(6000 + i*100); drive(); TIM_TypeDef *timer = i == 0 ? TIM2 : TIM3;
        timer->CR1 = 0; BenchHW_TickISR(mock_tick); off();
        assert(BenchHW_TakeAsyncFault() == BENCH_ASYNC_ENCODER); timer->CR1 = 1;
    }
    arm_at(7000); drive(); LPUART1->ISR = USART_ISR_ORE; BenchHW_UART_ISR(); off();
    assert(BenchHW_TakeAsyncFault() == BENCH_ASYNC_UART);
    assert(BenchHW_ReadByte(&byte) && byte == 0); assert(BenchHW_ReadByte(&byte) && byte == '\n'); receive('\n');
}

static void uart_buffers(void)
{
    uint8_t byte;
    for (unsigned i = 0U; i < 512U; ++i) receive('x');
    receive('y'); off();
    assert(BenchHW_TakeAsyncFault() == BENCH_ASYNC_RX);
    assert(BenchHW_ReadByte(&byte) && byte == 0U); /* Never process a truncated prefix. */
    assert(BenchHW_ReadByte(&byte) && byte == '\n');
    receive('a'); receive('\n'); assert(!BenchHW_ReadByte(&byte));
    receive('o'); receive('k'); receive('\n');
    assert(BenchHW_ReadByte(&byte) && byte == 'o');
    assert(BenchHW_ReadByte(&byte) && byte == 'k');
    assert(BenchHW_ReadByte(&byte) && byte == '\n');
    assert(!BenchHW_ReadByte(&byte) && !BenchHW_ReadByte(NULL));
    receive('a'); mock_tick += 250U;
    assert(BenchHW_ReadByte(&byte) && byte == 'a'); /* Boundary remains fresh. */
    receive('a'); mock_tick += 251U;
    assert(BenchHW_ReadByte(&byte) && byte == 0U); /* Stale arm/ping prefix is poisoned. */
    off(); assert(BenchHW_TakeAsyncFault() == BENCH_ASYNC_RX);
    assert(BenchHW_ReadByte(&byte) && byte == '\n');
    receive('r'); receive('m'); receive('\n');
    assert(!BenchHW_ReadByte(&byte));
    receive('o'); assert(BenchHW_ReadByte(&byte) && byte == 'o');
    char payload[4095]; memset(payload, 'T', sizeof payload);
    assert(!BenchHW_Write(NULL, 2U) && !BenchHW_Write(payload, 4096U));
    assert(BenchHW_Write(payload, sizeof payload));
    assert(!BenchHW_Write("not-partial", 11U));
    for (unsigned i = 0U; i < sizeof payload; ++i) {
        LPUART1->ISR = USART_ISR_TXE_TXFNF; BenchHW_UART_ISR(); assert(LPUART1->TDR == 'T');
    }
    BenchHW_UART_ISR(); assert(!(LPUART1->CR1 & USART_CR1_TXEIE_TXFNFIE));
    assert(BenchHW_Write("OK", 2U));
    BenchHW_UART_ISR(); assert(LPUART1->TDR == 'O');
    BenchHW_UART_ISR(); assert(LPUART1->TDR == 'K');
    BenchHW_UART_ISR(); assert(!(LPUART1->CR1 & USART_CR1_TXEIE_TXFNFIE));
}

int main(void)
{
    startup_pwm_and_reversal();
    asynchronous_shutdown();
    uart_buffers();
    puts("Native register/HAL software tests passed (startup, PWM/reversal, ISR shutdown, UART framing/watchdogs); no electrical validation.");
    return 0;
}
