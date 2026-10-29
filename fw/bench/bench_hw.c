#include "bench_hw.h"
#include "main.h"
#include <math.h>
#include <string.h>

#define RX_SIZE 512U
#define TX_SIZE 4096U
static volatile uint32_t rx_head, rx_tail, tx_head, tx_tail;
static uint8_t rx_ring[RX_SIZE], tx_ring[TX_SIZE];
static uint32_t rx_time[RX_SIZE];
static volatile uint32_t pending_fault, heartbeat_ms, control_ms;
static volatile bool gate, heartbeat_seen;
static volatile bool rx_poison, rx_discard, rx_reset_lf;
static volatile float applied;
static int last_direction;
static uint32_t zero_since;
static TIM_HandleTypeDef encoder_timer, pwm_timer;
static UART_HandleTypeDef host_uart;

static uint32_t lock(void) { uint32_t saved = __get_PRIMASK(); __disable_irq(); return saved; }
static void unlock(uint32_t saved) { __set_PRIMASK(saved); }

void BenchHW_CutoffRegisters(void)
{
    /* Fixed peripheral addresses and clock checks: no HAL handle dependence. */
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN | RCC_AHB2ENR_GPIOCEN;
    (void)RCC->AHB2ENR;
    if (RCC->APB1ENR1 & RCC_APB1ENR1_TIM4EN) {
        TIM4->CCER &= ~TIM_CCER_CC2E;
        TIM4->CCR2 = 0U;
        TIM4->CR1 &= ~TIM_CR1_CEN;
    }
    GPIOB->BSRR = GPIO_PIN_7 << 16U;
    GPIOC->BSRR = (GPIO_PIN_4 | GPIO_PIN_5) << 16U;
    GPIOB->OTYPER &= ~GPIO_PIN_7;
    GPIOC->OTYPER &= ~(GPIO_PIN_4 | GPIO_PIN_5);
    GPIOB->PUPDR &= ~(3U << 14U);
    GPIOC->PUPDR &= ~((3U << 8U) | (3U << 10U));
    GPIOB->MODER = (GPIOB->MODER & ~(3U << 14U)) | (1U << 14U);
    GPIOC->MODER = (GPIOC->MODER & ~((3U << 8U) | (3U << 10U))) |
        (1U << 8U) | (1U << 10U);
    __DSB();
}

static void shutdown(uint32_t fault)
{
    gate = false;
    pending_fault |= fault;
    applied = 0.0f;
    BenchHW_CutoffRegisters();
}

void BenchHW_Disable(void)
{
    uint32_t saved = lock();
    shutdown(0);
    last_direction = 0;
    zero_since = HAL_GetTick();
    unlock(saved);
}

_Noreturn void BenchHW_Fatal(void)
{
    __disable_irq();
    shutdown(BENCH_ASYNC_ESTOP);
    for (;;) __NOP();
}

static void clock_init(void)
{
    RCC_OscInitTypeDef o = {0}; RCC_ClkInitTypeDef c = {0};
    if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST) != HAL_OK) BenchHW_Fatal();
    o.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    o.HSIState = RCC_HSI_ON; o.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    o.PLL.PLLState = RCC_PLL_ON; o.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    o.PLL.PLLM = RCC_PLLM_DIV4; o.PLL.PLLN = 85;
    o.PLL.PLLP = RCC_PLLP_DIV2; o.PLL.PLLQ = RCC_PLLQ_DIV2; o.PLL.PLLR = RCC_PLLR_DIV2;
    if (HAL_RCC_OscConfig(&o) != HAL_OK) BenchHW_Fatal();
    c.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    c.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    c.AHBCLKDivider = RCC_SYSCLK_DIV1; c.APB1CLKDivider = RCC_HCLK_DIV1; c.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&c, FLASH_LATENCY_4) != HAL_OK) BenchHW_Fatal();
    if (HAL_RCC_GetHCLKFreq() != 170000000U || HAL_RCC_GetPCLK1Freq() != 170000000U ||
        HAL_RCC_GetPCLK2Freq() != 170000000U) BenchHW_Fatal();
}

void BenchHW_Init(void)
{
    BenchHW_CutoffRegisters();
    if (HAL_Init() != HAL_OK) BenchHW_Fatal();
    clock_init();
    __HAL_RCC_GPIOA_CLK_ENABLE(); __HAL_RCC_GPIOB_CLK_ENABLE(); __HAL_RCC_GPIOC_CLK_ENABLE();
    BenchHW_Disable();
    GPIO_InitTypeDef g = {0};
    g.Pin = GPIO_PIN_5; g.Mode = GPIO_MODE_OUTPUT_PP; g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW; HAL_GPIO_Init(GPIOA, &g);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);

    __HAL_RCC_TIM4_CLK_ENABLE();
    pwm_timer.Instance = TIM4; pwm_timer.Init.Prescaler = 0;
    pwm_timer.Init.CounterMode = TIM_COUNTERMODE_UP; pwm_timer.Init.Period = 8499U;
    pwm_timer.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    pwm_timer.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_PWM_Init(&pwm_timer) != HAL_OK) BenchHW_Fatal();
    TIM_OC_InitTypeDef oc = {0}; oc.OCMode = TIM_OCMODE_PWM1;
    oc.OCPolarity = TIM_OCPOLARITY_HIGH; oc.OCFastMode = TIM_OCFAST_DISABLE;
    if (HAL_TIM_PWM_ConfigChannel(&pwm_timer, &oc, TIM_CHANNEL_2) != HAL_OK) BenchHW_Fatal();
    /* PB7 remains GPIO low until a nonzero, armed command. */

    __HAL_RCC_TIM2_CLK_ENABLE();
    g.Pin = GPIO_PIN_0 | GPIO_PIN_1; g.Mode = GPIO_MODE_AF_PP;
    g.Pull = GPIO_NOPULL; g.Speed = GPIO_SPEED_FREQ_HIGH; g.Alternate = GPIO_AF1_TIM2;
    HAL_GPIO_Init(GPIOA, &g);
    encoder_timer.Instance = TIM2; encoder_timer.Init.Prescaler = 0;
    encoder_timer.Init.CounterMode = TIM_COUNTERMODE_UP; encoder_timer.Init.Period = UINT32_MAX;
    encoder_timer.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    encoder_timer.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    TIM_Encoder_InitTypeDef enc = {0}; enc.EncoderMode = TIM_ENCODERMODE_TI12;
    enc.IC1Polarity = enc.IC2Polarity = TIM_ICPOLARITY_RISING;
    enc.IC1Selection = enc.IC2Selection = TIM_ICSELECTION_DIRECTTI;
    enc.IC1Prescaler = enc.IC2Prescaler = TIM_ICPSC_DIV1;
    enc.IC1Filter = enc.IC2Filter = 4U;
    if (HAL_TIM_Encoder_Init(&encoder_timer, &enc) != HAL_OK ||
        HAL_TIM_Encoder_Start(&encoder_timer, TIM_CHANNEL_ALL) != HAL_OK) BenchHW_Fatal();

    RCC_PeriphCLKInitTypeDef peripheral = {0};
    peripheral.PeriphClockSelection = RCC_PERIPHCLK_LPUART1;
    /* 170MHz/4=42.5MHz fits LPUART BRR at115200; verify clock divider explicitly. */
    peripheral.Lpuart1ClockSelection = RCC_LPUART1CLKSOURCE_PCLK1;
    if (HAL_RCCEx_PeriphCLKConfig(&peripheral) != HAL_OK) BenchHW_Fatal();
    __HAL_RCC_LPUART1_CLK_ENABLE();
    g.Pin = GPIO_PIN_2 | GPIO_PIN_3; g.Mode = GPIO_MODE_AF_PP;
    g.Pull = GPIO_NOPULL; g.Alternate = GPIO_AF12_LPUART1;
    HAL_GPIO_Init(GPIOA, &g);
    host_uart.Instance = LPUART1; host_uart.Init.BaudRate = 115200U;
    host_uart.Init.WordLength = UART_WORDLENGTH_8B; host_uart.Init.StopBits = UART_STOPBITS_1;
    host_uart.Init.Parity = UART_PARITY_NONE; host_uart.Init.Mode = UART_MODE_TX_RX;
    host_uart.Init.HwFlowCtl = UART_HWCONTROL_NONE; host_uart.Init.OverSampling = UART_OVERSAMPLING_16;
    host_uart.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    host_uart.Init.ClockPrescaler = UART_PRESCALER_DIV4;
    if (HAL_UART_Init(&host_uart) != HAL_OK || HAL_UARTEx_DisableFifoMode(&host_uart) != HAL_OK) BenchHW_Fatal();
    LPUART1->ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NECF;
    LPUART1->CR3 |= USART_CR3_EIE;
    LPUART1->CR1 |= USART_CR1_RXNEIE_RXFNEIE;
    HAL_NVIC_SetPriority(LPUART1_IRQn, 2U, 0U);
    HAL_NVIC_EnableIRQ(LPUART1_IRQn);
    BenchHW_Disable();
}

bool BenchHW_EncoderRunning(void)
{
    return (RCC->APB1ENR1 & RCC_APB1ENR1_TIM2EN) &&
        (TIM2->CR1 & TIM_CR1_CEN) &&
        (TIM2->SMCR & TIM_SMCR_SMS) == TIM_ENCODERMODE_TI12 &&
        (TIM2->CCER & (TIM_CCER_CC1E | TIM_CCER_CC2E)) == (TIM_CCER_CC1E | TIM_CCER_CC2E);
}
uint32_t BenchHW_Counter(void) { return TIM2->CNT; }

bool BenchHW_Arm(uint32_t now_ms)
{
    uint32_t saved = lock();
    bool valid = pending_fault == 0U && heartbeat_seen &&
        (uint32_t)(now_ms - heartbeat_ms) <= 500U && BenchHW_EncoderRunning();
    if (valid) { control_ms = now_ms; gate = true; last_direction = 0; zero_since = now_ms; }
    unlock(saved);
    return valid;
}

void BenchHW_SetMotor(float command)
{
    uint32_t saved = lock();
    if (!gate || !isfinite(command) || fabsf(command) > 25.0f) {
        shutdown(!isfinite(command) || fabsf(command) > 25.0f ? BENCH_ASYNC_ESTOP : 0U);
        unlock(saved); return;
    }
    uint32_t now = HAL_GetTick();
    int direction = command > 0.0f ? 1 : command < 0.0f ? -1 : 0;
    /* A reversal always de-energizes both bridge inputs and ENA for>=2ms.
     * The caller retries on the next100Hz control update; no busy wait. */
    if (!direction || (last_direction && direction != last_direction)) {
        BenchHW_CutoffRegisters(); applied = 0.0f;
        if (last_direction) { zero_since = now; last_direction = 0; }
        unlock(saved); return;
    }
    if (!last_direction && (uint32_t)(now - zero_since) < 2U) { unlock(saved); return; }
    TIM4->CCER &= ~TIM_CCER_CC2E;
    TIM4->CR1 &= ~TIM_CR1_CEN;
    GPIOC->BSRR = (GPIO_PIN_4 | GPIO_PIN_5) << 16U;
    GPIOC->BSRR = direction > 0 ? GPIO_PIN_4 : GPIO_PIN_5;
    /* Truncate so quantization can never exceed the requested duty cap. */
    TIM4->CCR2 = (uint32_t)(fabsf(command) * 85.0f);
    TIM4->EGR = TIM_EGR_UG; /* Load HAL's CCR preload while output is gated. */
    GPIOB->AFR[0] = (GPIOB->AFR[0] & ~(15U << 28U)) | (GPIO_AF2_TIM4 << 28U);
    GPIOB->MODER = (GPIOB->MODER & ~(3U << 14U)) | (2U << 14U);
    TIM4->CCER |= TIM_CCER_CC2E;
    TIM4->CR1 |= TIM_CR1_CEN;
    applied = direction * ((float)TIM4->CCR2 / 85.0f);
    last_direction = direction;
    unlock(saved);
}
float BenchHW_AppliedPWM(void) { return applied; }
void BenchHW_Heartbeat(uint32_t now_ms) { heartbeat_ms = now_ms; heartbeat_seen = true; }
void BenchHW_ControlAlive(uint32_t now_ms) { control_ms = now_ms; }
uint32_t BenchHW_TakeAsyncFault(void)
{
    uint32_t saved = lock(); uint32_t result = pending_fault; pending_fault = 0;
    unlock(saved); return result;
}
void BenchHW_TickISR(uint32_t now_ms)
{
    if (gate) {
        if ((uint32_t)(now_ms - control_ms) > 50U) shutdown(BENCH_ASYNC_LOOP);
        else if ((uint32_t)(now_ms - heartbeat_ms) > 500U) shutdown(BENCH_ASYNC_LINK);
        else if (!BenchHW_EncoderRunning()) shutdown(BENCH_ASYNC_ENCODER);
    }
}

bool BenchHW_ReadByte(uint8_t *byte)
{
    if (byte == NULL) return false;
    uint32_t saved = lock();
    if (rx_tail != rx_head && (uint32_t)(HAL_GetTick() - rx_time[rx_tail & (RX_SIZE - 1U)]) > 250U) {
        /* Old queued ping/arm/move lines cannot become a fresh session. */
        shutdown(BENCH_ASYNC_RX); rx_poison = rx_discard = true;
    }
    if (rx_poison) {
        rx_tail = rx_head; rx_poison = false; rx_reset_lf = true; *byte = 0U;
        unlock(saved); return true; /* Invalidates any partial protocol line. */
    }
    if (rx_reset_lf) { rx_reset_lf = false; *byte = '\n'; unlock(saved); return true; }
    if (rx_tail == rx_head) { unlock(saved); return false; }
    *byte = rx_ring[rx_tail & (RX_SIZE - 1U)];
    __DMB(); rx_tail++; unlock(saved); return true;
}
bool BenchHW_Write(const char *data, size_t length)
{
    if (data == NULL || length >= TX_SIZE) return false;
    /* Sole producer foreground; ISR can only free space during this copy. */
    uint32_t head = tx_head;
    if (length > TX_SIZE - (head - tx_tail) - 1U) return false;
    for (size_t i = 0; i < length; ++i) tx_ring[(head + (uint32_t)i) & (TX_SIZE - 1U)] = (uint8_t)data[i];
    __DMB(); tx_head = head + (uint32_t)length;
    LPUART1->CR1 |= USART_CR1_TXEIE_TXFNFIE;
    return true;
}
void BenchHW_UART_ISR(void)
{
    uint32_t status = LPUART1->ISR;
    if (status & (USART_ISR_ORE | USART_ISR_FE | USART_ISR_NE)) {
        LPUART1->ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NECF;
        shutdown(BENCH_ASYNC_UART); rx_poison = rx_discard = true;
    }
    if (status & USART_ISR_RXNE_RXFNE) {
        uint8_t byte = (uint8_t)LPUART1->RDR;
        if (byte == '!') { shutdown(BENCH_ASYNC_ESTOP); rx_poison = rx_discard = true; }
        else if (rx_discard) { if (byte == '\n') rx_discard = false; }
        else if (rx_head - rx_tail >= RX_SIZE) { shutdown(BENCH_ASYNC_RX); rx_poison = rx_discard = true; }
        else {
            uint32_t slot = rx_head & (RX_SIZE - 1U);
            rx_ring[slot] = byte; rx_time[slot] = HAL_GetTick();
            __DMB(); rx_head++;
        }
    }
    if ((status & USART_ISR_TXE_TXFNF) && (LPUART1->CR1 & USART_CR1_TXEIE_TXFNFIE)) {
        if (tx_tail != tx_head) { LPUART1->TDR = tx_ring[tx_tail & (TX_SIZE - 1U)]; tx_tail++; }
        else LPUART1->CR1 &= ~USART_CR1_TXEIE_TXFNFIE;
    }
}
