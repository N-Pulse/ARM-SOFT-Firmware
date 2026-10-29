#include "main.h"
#include "bench_hw.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

GPIO_TypeDef mock_gpioa, mock_gpiob, mock_gpioc;
RCC_TypeDef mock_rcc;
TIM_TypeDef mock_tim2, mock_tim4;
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
        v->Pin == (GPIO_PIN_2 | GPIO_PIN_3));
    if (v->Pin == 3U) assert(v->Alternate == 1U && v->Pull == 0U);
    if (v->Pin == 12U) assert(v->Alternate == 12U);
    gpio_initializations++;
}
void HAL_GPIO_WritePin(GPIO_TypeDef *p, uint32_t pin, uint32_t state)
{ assert(p == GPIOA && pin == 32U && state == 0U); }
HAL_StatusTypeDef HAL_TIM_PWM_Init(TIM_HandleTypeDef *v)
{ assert(v->Instance == TIM4); TIM4->PSC = v->Init.Prescaler; TIM4->ARR = v->Init.Period; return HAL_OK; }
HAL_StatusTypeDef HAL_TIM_PWM_ConfigChannel(TIM_HandleTypeDef *v, TIM_OC_InitTypeDef *oc, uint32_t channel)
{ assert(v->Instance == TIM4 && oc->OCMode == 6U && channel == 4U); return HAL_OK; }
HAL_StatusTypeDef HAL_TIM_Encoder_Init(TIM_HandleTypeDef *v, TIM_Encoder_InitTypeDef *enc)
{
    assert(v->Instance == TIM2 && v->Init.Period == 0xFFFFFFFFU);
    assert(enc->EncoderMode == 3U && enc->IC1Filter == 4U && enc->IC2Filter == 4U);
    TIM2->SMCR = enc->EncoderMode;
    return HAL_OK;
}
HAL_StatusTypeDef HAL_TIM_Encoder_Start(TIM_HandleTypeDef *v, uint32_t channel)
{ assert(v->Instance == TIM2 && channel == TIM_CHANNEL_ALL); TIM2->CR1 = 1U; TIM2->CCER = 17U; return HAL_OK; }
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
    assert(TIM4->CCR2 == 0U && !(TIM4->CCER & 16U) && !(TIM4->CR1 & 1U));
    assert(((GPIOB->MODER >> 14U) & 3U) == 1U);
    assert(((GPIOC->MODER >> 8U) & 3U) == 1U && ((GPIOC->MODER >> 10U) & 3U) == 1U);
    assert(GPIOB->BSRR == (128U << 16U) && GPIOC->BSRR == (48U << 16U));
    assert(BenchHW_AppliedPWM() == 0.0f);
}
static void receive(uint8_t byte)
{ LPUART1->ISR = USART_ISR_RXNE_RXFNE; LPUART1->RDR = byte; BenchHW_UART_ISR(); }
static void arm_at(uint32_t now)
{ mock_tick = now; BenchHW_Heartbeat(now); BenchHW_ControlAlive(now); assert(BenchHW_Arm(now)); }
static void drive(void)
{ mock_tick += 2U; BenchHW_SetMotor(5.0f); assert(TIM4->CCR2 == 425U); }

static void startup_pwm_and_reversal(void)
{
    BenchHW_Init(); off();
    assert(gpio_initializations == 3U && uart_initializations == 1U);
    assert(TIM4->ARR == 8499U && TIM4->PSC == 0U); /* 170MHz/8500 =20kHz. */
    assert(BenchHW_EncoderRunning());
    assert(!BenchHW_Arm(0U)); /* Even at boot tick0 an explicit heartbeat is required. */
    assert(!(LPUART1->CR1 & USART_CR1_TXEIE_TXFNFIE));
    BenchHW_SetMotor(5.0f); off(); /* Disabled gate cannot energize. */
    assert(!BenchHW_Arm(501U)); /* No current heartbeat. */
    TIM2->CR1 = 0U; BenchHW_Heartbeat(1000U); assert(!BenchHW_Arm(1000U)); TIM2->CR1 = 1U;
    arm_at(1000U);
    BenchHW_SetMotor(5.0f); off(); /* Initial dead time. */
    drive();
    assert(BenchHW_AppliedPWM() == 5.0f && GPIOC->BSRR == 16U);
    assert(((GPIOB->MODER >> 14U) & 3U) == 2U && (GPIOB->AFR[0] >> 28U) == 2U);
    assert((TIM4->CCER & 16U) && (TIM4->CR1 & 1U));
    assert(TIM4->EGR == 1U); /* CCR preload is loaded before output re-enable. */
    BenchHW_SetMotor(5.001f);
    assert(BenchHW_AppliedPWM() <= 5.001f); /* Duty quantization never raises cap. */
    BenchHW_SetMotor(-5.0f); off(); /* Opposite command first removes ENA + both inputs. */
    mock_tick++; BenchHW_SetMotor(-5.0f); off();
    mock_tick++; BenchHW_SetMotor(-5.0f);
    assert(TIM4->CCR2 == 425U && BenchHW_AppliedPWM() == -5.0f && GPIOC->BSRR == 32U);
    BenchHW_SetMotor(0.0f); off();
    mock_tick += 2U; BenchHW_SetMotor(25.0f);
    assert(TIM4->CCR2 == 2125U && BenchHW_AppliedPWM() == 25.0f);
    BenchHW_SetMotor(25.01f); off(); assert(BenchHW_TakeAsyncFault() == BENCH_ASYNC_ESTOP);
    BenchHW_SetMotor(5.0f); off(); /* Taking the fault never reopens gate. */
    arm_at(2000U); drive(); BenchHW_SetMotor(NAN); off();
    assert(BenchHW_TakeAsyncFault() == BENCH_ASYNC_ESTOP);
    assert(mock_primask == 0U); /* Critical sections restore prior mask. */
}

static void asynchronous_shutdown(void)
{
    arm_at(3000U); drive(); receive('!'); off();
    assert(!BenchHW_Arm(mock_tick)); /* Pending ISR fault blocks arm. */
    assert(BenchHW_TakeAsyncFault() == BENCH_ASYNC_ESTOP);
    mock_tick += 10U; BenchHW_SetMotor(5.0f); off();
    uint8_t byte = 99U;
    assert(BenchHW_ReadByte(&byte) && byte == 0U);
    assert(BenchHW_ReadByte(&byte) && byte == '\n');
    receive('a'); receive('\n'); /* Discard all suffix until newline after priority !. */
    assert(!BenchHW_ReadByte(&byte));
    arm_at(4000U); drive(); BenchHW_TickISR(4050U); assert(TIM4->CCR2 == 425U);
    BenchHW_TickISR(4051U); off(); assert(BenchHW_TakeAsyncFault() == BENCH_ASYNC_LOOP);
    arm_at(5000U); drive(); BenchHW_ControlAlive(5500U); BenchHW_TickISR(5500U);
    assert(TIM4->CCR2 == 425U);
    BenchHW_ControlAlive(5501U); BenchHW_TickISR(5501U); off();
    assert(BenchHW_TakeAsyncFault() == BENCH_ASYNC_LINK);
    arm_at(6000U); drive(); TIM2->CCER &= ~1U; BenchHW_TickISR(6003U); off();
    assert(BenchHW_TakeAsyncFault() == BENCH_ASYNC_ENCODER); TIM2->CCER |= 1U;
    arm_at(7000U); drive(); LPUART1->ISR = USART_ISR_ORE; BenchHW_UART_ISR(); off();
    assert(BenchHW_TakeAsyncFault() == BENCH_ASYNC_UART);
    assert(BenchHW_ReadByte(&byte) && byte == 0U);
    assert(BenchHW_ReadByte(&byte) && byte == '\n'); receive('\n');
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
