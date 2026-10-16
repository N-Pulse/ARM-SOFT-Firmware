#include "main.h"
#include "old_hw.h"
#include "board_link.h"
#include "comms_config.h"

volatile board_role_t g_old_hw_role = BOARD_ROLE_UNKNOWN;
volatile uint32_t g_old_hw_rejected_commands;
volatile uint32_t g_old_hw_encoder_unavailable_mask;
extern TIM_HandleTypeDef htim1, htim2, htim3, htim4, htim8, htim20;
UART_HandleTypeDef hlpuart1;
extern UART_HandleTypeDef huart3;

/* Recovered from simulation_pipeline C, not from the incomplete .ioc. */
const old_hw_motor_pins_t g_old_hw_motor_pins[MOTOR_COUNT] = {
    [MOTOR_THUMB]   = {TIM1,TIM_CHANNEL_1,GPIOA,GPIO_PIN_8, GPIOB,GPIO_PIN_5, GPIOB,GPIO_PIN_4},
    [MOTOR_INDEX]   = {TIM1,TIM_CHANNEL_2,GPIOA,GPIO_PIN_9, GPIOC,GPIO_PIN_10,GPIOC,GPIO_PIN_11},
    [MOTOR_MIDDLE]  = {TIM1,TIM_CHANNEL_3,GPIOA,GPIO_PIN_10,GPIOC,GPIO_PIN_12,GPIOA,GPIO_PIN_15},
    [MOTOR_RING]    = {TIM1,TIM_CHANNEL_4,GPIOA,GPIO_PIN_11,GPIOB,GPIO_PIN_12,GPIOB,GPIO_PIN_13},
    [MOTOR_LITTLE]  = {TIM4,TIM_CHANNEL_1,GPIOB,GPIO_PIN_6, GPIOC,GPIO_PIN_8, GPIOC,GPIO_PIN_9},
    [MOTOR_WRIST_X] = {TIM4,TIM_CHANNEL_2,GPIOB,GPIO_PIN_7, GPIOC,GPIO_PIN_4, GPIOC,GPIO_PIN_5},
    [MOTOR_WRIST_Y] = {TIM4,TIM_CHANNEL_3,GPIOB,GPIO_PIN_8, GPIOB,GPIO_PIN_0, GPIOB,GPIO_PIN_1},
    [MOTOR_PALM]    = {TIM4,TIM_CHANNEL_4,GPIOB,GPIO_PIN_9, GPIOD,GPIO_PIN_2, GPIOC,GPIO_PIN_3},
};

static bool local_motor(motor_id_t id)
{
    bool master = id == MOTOR_WRIST_X || id == MOTOR_WRIST_Y || id == MOTOR_LITTLE;
    return g_old_hw_role == BOARD_ROLE_MASTER ? master :
           g_old_hw_role == BOARD_ROLE_SLAVE && !master;
}

void OldHW_DetectRole(void)
{
    if (g_old_hw_role != BOARD_ROLE_UNKNOWN) return;
    __HAL_RCC_GPIOC_CLK_ENABLE();
    GPIO_InitTypeDef g = {0};
    g.Pin = GPIO_PIN_0; g.Mode = GPIO_MODE_INPUT; g.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOC, &g);
    for (volatile unsigned i = 0; i < 1000U; ++i) __NOP();
    g_old_hw_role = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_0) == GPIO_PIN_RESET ?
                    BOARD_ROLE_SLAVE : BOARD_ROLE_MASTER;
}

static void output_low(GPIO_TypeDef *port, uint16_t pin)
{
    HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET); /* Latch before output mode. */
    GPIO_InitTypeDef g = {0};
    g.Pin = pin; g.Mode = GPIO_MODE_OUTPUT_PP; g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(port, &g);
}

void OldHW_HoldOutputsLow(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE(); __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    if (g_old_hw_role == BOARD_ROLE_SLAVE) __HAL_RCC_GPIOD_CLK_ENABLE();
    if (RCC->APB2ENR & RCC_APB2ENR_TIM1EN) {
        TIM1->BDTR &= ~(TIM_BDTR_MOE | TIM_BDTR_AOE);
        TIM1->CCER = 0; TIM1->CR1 &= ~TIM_CR1_CEN;
        TIM1->CCR1 = TIM1->CCR2 = TIM1->CCR3 = TIM1->CCR4 = 0;
    }
    if (RCC->APB1ENR1 & RCC_APB1ENR1_TIM4EN) {
        TIM4->CCER = 0; TIM4->CR1 &= ~TIM_CR1_CEN;
        TIM4->CCR1 = TIM4->CCR2 = TIM4->CCR3 = TIM4->CCR4 = 0;
    }
    for (unsigned i = 0; i < MOTOR_COUNT; ++i) {
        if (!local_motor((motor_id_t)i)) continue;
        const old_hw_motor_pins_t *m = &g_old_hw_motor_pins[i];
        output_low(m->enable_port, m->enable_pin);
        output_low(m->in1_port, m->in1_pin);
        output_low(m->in2_port, m->in2_pin);
    }
    __DSB();
}

static bool pin_low(GPIO_TypeDef *port, uint16_t pin)
{
    unsigned shift = 0;
    while ((1U << shift) != pin) ++shift;
    return !(port->ODR & pin) && !(port->OTYPER & pin) &&
           ((port->MODER >> (2U * shift)) & 3U) == 1U;
}

bool OldHW_OutputsDisabled(void)
{
    if (g_old_hw_role == BOARD_ROLE_UNKNOWN) return false;
    if ((RCC->APB2ENR & RCC_APB2ENR_TIM1EN) &&
        ((TIM1->BDTR & (TIM_BDTR_MOE | TIM_BDTR_AOE)) || TIM1->CCER ||
         (TIM1->CR1 & TIM_CR1_CEN) || TIM1->CCR1 || TIM1->CCR2 || TIM1->CCR3 || TIM1->CCR4)) return false;
    if ((RCC->APB1ENR1 & RCC_APB1ENR1_TIM4EN) &&
        (TIM4->CCER || (TIM4->CR1 & TIM_CR1_CEN) ||
         TIM4->CCR1 || TIM4->CCR2 || TIM4->CCR3 || TIM4->CCR4)) return false;
    for (unsigned i = 0; i < MOTOR_COUNT; ++i) {
        if (!local_motor((motor_id_t)i)) continue;
        const old_hw_motor_pins_t *m = &g_old_hw_motor_pins[i];
        if (!pin_low(m->enable_port,m->enable_pin) || !pin_low(m->in1_port,m->in1_pin) ||
            !pin_low(m->in2_port,m->in2_pin)) return false;
    }
    return true; /* Register state only, not an electrical cut-off certificate. */
}

void OldHW_ClockConfig(void)
{
    RCC_OscInitTypeDef o = {0}; RCC_ClkInitTypeDef c = {0};
    if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST) != HAL_OK) Error_Handler();
    o.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    o.HSIState = RCC_HSI_ON; o.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    o.PLL.PLLState = RCC_PLL_ON; o.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    o.PLL.PLLM = RCC_PLLM_DIV4; o.PLL.PLLN = 85;
    o.PLL.PLLP = RCC_PLLP_DIV2; o.PLL.PLLQ = RCC_PLLQ_DIV2; o.PLL.PLLR = RCC_PLLR_DIV2;
    if (HAL_RCC_OscConfig(&o) != HAL_OK) Error_Handler();
    c.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    c.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    c.AHBCLKDivider = RCC_SYSCLK_DIV1; c.APB1CLKDivider = RCC_HCLK_DIV1; c.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&c, FLASH_LATENCY_4) != HAL_OK) Error_Handler();
    if (HAL_RCC_GetHCLKFreq() != 170000000U || HAL_RCC_GetPCLK1Freq() != 170000000U ||
        HAL_RCC_GetPCLK2Freq() != 170000000U) Error_Handler();
}

static void pwm_prepare(TIM_HandleTypeDef *h, TIM_TypeDef *timer, unsigned channels)
{
    h->Instance = timer; h->Init.Prescaler = 0; h->Init.CounterMode = TIM_COUNTERMODE_UP;
    h->Init.Period = 8499U; /* Historical 20 kHz at 170 MHz. Not started at boot. */
    h->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    h->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_PWM_Init(h) != HAL_OK) Error_Handler();
    TIM_OC_InitTypeDef oc = {0};
    oc.OCMode = TIM_OCMODE_PWM1; oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc.OCNPolarity = TIM_OCNPOLARITY_HIGH; oc.OCFastMode = TIM_OCFAST_DISABLE;
    oc.OCIdleState = TIM_OCIDLESTATE_RESET; oc.OCNIdleState = TIM_OCNIDLESTATE_RESET;
    for (unsigned i = 0; i < 4; ++i)
        if ((channels & (1U << i)) && HAL_TIM_PWM_ConfigChannel(h,&oc,i*4U) != HAL_OK) Error_Handler();
    /* No MSP post-init / AF on ENA pins, PWM start, CCER enable or MOE. */
}

#ifndef MOTOR_BACKEND_SIM
static void encoder_prepare(TIM_HandleTypeDef *h, TIM_TypeDef *timer,
                            GPIO_TypeDef *p1, uint16_t pin1, uint32_t af1,
                            GPIO_TypeDef *p2, uint16_t pin2, uint32_t af2, bool wide)
{
    GPIO_InitTypeDef g = {0};
    g.Mode = GPIO_MODE_AF_PP; g.Pull = GPIO_PULLUP; g.Speed = GPIO_SPEED_FREQ_HIGH;
    g.Pin = pin1; g.Alternate = af1; HAL_GPIO_Init(p1,&g);
    g.Pin = pin2; g.Alternate = af2; HAL_GPIO_Init(p2,&g);
    h->Instance = timer; h->Init.CounterMode = TIM_COUNTERMODE_UP;
    h->Init.Period = wide ? UINT32_MAX : UINT16_MAX;
    h->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    h->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    TIM_Encoder_InitTypeDef e = {0};
    e.EncoderMode = TIM_ENCODERMODE_TI12;
    e.IC1Polarity = e.IC2Polarity = TIM_ICPOLARITY_RISING;
    e.IC1Selection = e.IC2Selection = TIM_ICSELECTION_DIRECTTI;
    e.IC1Prescaler = e.IC2Prescaler = TIM_ICPSC_DIV1;
    e.IC1Filter = e.IC2Filter = 4;
    if (HAL_TIM_Encoder_Init(h,&e) != HAL_OK) Error_Handler();
    /* Modern Encoder_BSP is the sole owner of Encoder_Start and calibration. */
}
#endif

static void uart_prepare(UART_HandleTypeDef *h, USART_TypeDef *uart)
{
    h->Instance = uart; h->Init.BaudRate = 115200U;
    h->Init.WordLength = UART_WORDLENGTH_8B; h->Init.StopBits = UART_STOPBITS_1;
    h->Init.Parity = UART_PARITY_NONE; h->Init.Mode = UART_MODE_TX_RX;
    h->Init.HwFlowCtl = UART_HWCONTROL_NONE; h->Init.OverSampling = UART_OVERSAMPLING_16;
    h->Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE; h->Init.ClockPrescaler = UART_PRESCALER_DIV1;
    if (HAL_UART_Init(h) != HAL_OK) Error_Handler();
    if (uart==USART3) {
        /* Full-duplex board traffic must survive short IRQ-masked motor and
         * protocol operations. A single RDR overflows after one character
         * (~87 us at 115200). Use the hardware FIFO, with a one-byte RX
         * threshold so the existing byte callback also handles short frames. */
        if (HAL_UARTEx_SetRxFifoThreshold(h,UART_RXFIFO_THRESHOLD_1_8)!=HAL_OK ||
            HAL_UARTEx_SetTxFifoThreshold(h,UART_TXFIFO_THRESHOLD_1_8)!=HAL_OK ||
            HAL_UARTEx_EnableFifoMode(h)!=HAL_OK) Error_Handler();
    } else if (HAL_UARTEx_DisableFifoMode(h)!=HAL_OK) Error_Handler();
}

bool OldHW_HostEnabled(void)
{
    /* Historical main initializes COM1/LPUART1 on BOTH boards, including the
     * slave's diagnostic VCP. Follow executable code over wiring.md here. */
    return g_old_hw_role != BOARD_ROLE_UNKNOWN;
}

void OldHW_Init(void)
{
    if (g_old_hw_role == BOARD_ROLE_UNKNOWN) Error_Handler();
    OldHW_HoldOutputsLow();
    __HAL_RCC_TIM4_CLK_ENABLE();
    pwm_prepare(&htim4,TIM4,g_old_hw_role == BOARD_ROLE_MASTER ? 7U : 8U);
    if (g_old_hw_role == BOARD_ROLE_SLAVE) {
        __HAL_RCC_TIM1_CLK_ENABLE(); pwm_prepare(&htim1,TIM1,15U);
    }
#ifndef MOTOR_BACKEND_SIM
    __HAL_RCC_TIM2_CLK_ENABLE(); __HAL_RCC_TIM3_CLK_ENABLE(); __HAL_RCC_TIM8_CLK_ENABLE();
    encoder_prepare(&htim2,TIM2,GPIOA,GPIO_PIN_0,GPIO_AF1_TIM2,GPIOA,GPIO_PIN_1,GPIO_AF1_TIM2,true);
    encoder_prepare(&htim3,TIM3,GPIOA,GPIO_PIN_6,GPIO_AF2_TIM3,GPIOA,GPIO_PIN_7,GPIO_AF2_TIM3,false);
    encoder_prepare(&htim8,TIM8,GPIOC,GPIO_PIN_6,GPIO_AF4_TIM8,GPIOC,GPIO_PIN_7,GPIO_AF4_TIM8,false);
    if (g_old_hw_role == BOARD_ROLE_SLAVE) {
        /* Encoder_BSP subsequently owns the software EXTI decoder startup.
         * TIM15 is never used as an encoder. Inputs stay passive until then. */
        g_old_hw_encoder_unavailable_mask = 1U << MOTOR_THUMB;
        GPIO_InitTypeDef thumb = {0};
        thumb.Pin = GPIO_PIN_14 | GPIO_PIN_15; thumb.Mode = GPIO_MODE_INPUT;
        thumb.Pull = GPIO_PULLUP; HAL_GPIO_Init(GPIOB,&thumb);
        __HAL_RCC_TIM20_CLK_ENABLE();
        encoder_prepare(&htim20,TIM20,GPIOB,GPIO_PIN_2,GPIO_AF3_TIM20,GPIOC,GPIO_PIN_2,GPIO_AF6_TIM20,false);
    }
#endif
    RCC_PeriphCLKInitTypeDef c = {0};
    c.PeriphClockSelection = RCC_PERIPHCLK_USART3 | RCC_PERIPHCLK_LPUART1;
    c.Usart3ClockSelection = RCC_USART3CLKSOURCE_PCLK1;
    c.Lpuart1ClockSelection = RCC_LPUART1CLKSOURCE_PCLK1;
    if (HAL_RCCEx_PeriphCLKConfig(&c) != HAL_OK) Error_Handler();
    __HAL_RCC_USART3_CLK_ENABLE();
    GPIO_InitTypeDef g = {0}; g.Mode = GPIO_MODE_AF_PP; g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_HIGH; g.Pin = GPIO_PIN_10 | GPIO_PIN_11; g.Alternate = GPIO_AF7_USART3;
    HAL_GPIO_Init(GPIOB,&g); uart_prepare(&huart3,USART3);
    HAL_NVIC_DisableIRQ(USART3_IRQn); HAL_NVIC_ClearPendingIRQ(USART3_IRQn);
    HAL_NVIC_SetPriority(USART3_IRQn,COMMS_UART_IRQ_PRIORITY,0);
    /* board_link_runtime enables USART3 IT only after scheduler/objects exist. */
    if (OldHW_HostEnabled()) {
        __HAL_RCC_LPUART1_CLK_ENABLE();
        g.Pin = GPIO_PIN_2 | GPIO_PIN_3; g.Alternate = GPIO_AF12_LPUART1;
        HAL_GPIO_Init(GPIOA,&g); uart_prepare(&hlpuart1,LPUART1);
        HAL_NVIC_DisableIRQ(LPUART1_IRQn); HAL_NVIC_ClearPendingIRQ(LPUART1_IRQn);
        HAL_NVIC_SetPriority(LPUART1_IRQn,COMMS_UART_IRQ_PRIORITY,0);
    }
    output_low(GPIOA,GPIO_PIN_5); /* Historical Nucleo LED, never PB10 link TX. */
    OldHW_HoldOutputsLow();
    if (!OldHW_OutputsDisabled()) Error_Handler();
}

void OldHW_Heartbeat(void) {
#if !N_PULSE_TIMING_DIAGNOSTICS
    HAL_GPIO_TogglePin(GPIOA,GPIO_PIN_5);
#endif
}

bool OldHW_EncoderRunning(motor_id_t id)
{
    TIM_TypeDef *t = NULL;
    if (!BoardLink_IsLocalMotor(id) || id == MOTOR_THUMB) return false;
    if (id == MOTOR_INDEX || id == MOTOR_WRIST_X) t = TIM2;
    if (id == MOTOR_MIDDLE || id == MOTOR_LITTLE) t = TIM3;
    if (id == MOTOR_RING || id == MOTOR_WRIST_Y) t = TIM8;
    if (id == MOTOR_PALM) t = TIM20;
    if (t == TIM2 && !(RCC->APB1ENR1 & RCC_APB1ENR1_TIM2EN)) return false;
    if (t == TIM3 && !(RCC->APB1ENR1 & RCC_APB1ENR1_TIM3EN)) return false;
    if (t == TIM8 && !(RCC->APB2ENR & RCC_APB2ENR_TIM8EN)) return false;
    if (t == TIM20 && !(RCC->APB2ENR & RCC_APB2ENR_TIM20EN)) return false;
    return t && (t->CR1 & TIM_CR1_CEN) &&
           (t->SMCR & TIM_SMCR_SMS) == TIM_ENCODERMODE_TI12 &&
           (t->CCER & (TIM_CCER_CC1E | TIM_CCER_CC2E)) == (TIM_CCER_CC1E | TIM_CCER_CC2E);
}
