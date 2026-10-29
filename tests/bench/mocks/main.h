#ifndef BENCH_NATIVE_MAIN_H
#define BENCH_NATIVE_MAIN_H
/* Minimal register/HAL model for executing production bench_hw.c on the host.
 * This models software register writes, not a bridge, motor or electrical pins. */
#include <stdint.h>

typedef struct { volatile uint32_t MODER, OTYPER, PUPDR, BSRR, AFR[2]; } GPIO_TypeDef;
typedef struct { volatile uint32_t AHB2ENR, APB1ENR1; } RCC_TypeDef;
typedef struct { volatile uint32_t CR1, CCR2, CCER, SMCR, CNT, ARR, PSC, EGR; } TIM_TypeDef;
typedef struct { volatile uint32_t ICR, CR1, CR3, ISR, RDR, TDR; } USART_TypeDef;
extern GPIO_TypeDef mock_gpioa, mock_gpiob, mock_gpioc;
extern RCC_TypeDef mock_rcc;
extern TIM_TypeDef mock_tim2, mock_tim4;
extern USART_TypeDef mock_uart;
#define GPIOA (&mock_gpioa)
#define GPIOB (&mock_gpiob)
#define GPIOC (&mock_gpioc)
#define RCC (&mock_rcc)
#define TIM2 (&mock_tim2)
#define TIM4 (&mock_tim4)
#define LPUART1 (&mock_uart)
#define RCC_AHB2ENR_GPIOBEN (1U << 1)
#define RCC_AHB2ENR_GPIOCEN (1U << 2)
#define RCC_APB1ENR1_TIM2EN (1U << 0)
#define RCC_APB1ENR1_TIM4EN (1U << 2)
#define GPIO_PIN_0 1U
#define GPIO_PIN_1 2U
#define GPIO_PIN_2 4U
#define GPIO_PIN_3 8U
#define GPIO_PIN_4 16U
#define GPIO_PIN_5 32U
#define GPIO_PIN_7 128U
#define TIM_CCER_CC1E 1U
#define TIM_CCER_CC2E 16U
#define TIM_CR1_CEN 1U
#define TIM_EGR_UG 1U
#define TIM_SMCR_SMS 0x10007U
#define TIM_ENCODERMODE_TI12 3U
#define GPIO_AF1_TIM2 1U
#define GPIO_AF2_TIM4 2U
#define GPIO_AF12_LPUART1 12U
#define USART_ISR_ORE (1U << 3)
#define USART_ISR_FE (1U << 1)
#define USART_ISR_NE (1U << 2)
#define USART_ISR_RXNE_RXFNE (1U << 5)
#define USART_ISR_TXE_TXFNF (1U << 7)
#define USART_ICR_ORECF USART_ISR_ORE
#define USART_ICR_FECF USART_ISR_FE
#define USART_ICR_NECF USART_ISR_NE
#define USART_CR3_EIE 1U
#define USART_CR1_RXNEIE_RXFNEIE (1U << 5)
#define USART_CR1_TXEIE_TXFNFIE (1U << 7)

typedef struct { uint32_t Prescaler, CounterMode, Period, ClockDivision, AutoReloadPreload; } TIM_Base_InitTypeDef;
typedef struct { TIM_TypeDef *Instance; TIM_Base_InitTypeDef Init; } TIM_HandleTypeDef;
typedef struct { uint32_t OCMode, OCPolarity, OCFastMode; } TIM_OC_InitTypeDef;
typedef struct { uint32_t EncoderMode, IC1Polarity, IC2Polarity, IC1Selection, IC2Selection,
    IC1Prescaler, IC2Prescaler, IC1Filter, IC2Filter; } TIM_Encoder_InitTypeDef;
typedef struct { uint32_t Pin, Mode, Pull, Speed, Alternate; } GPIO_InitTypeDef;
typedef struct { uint32_t BaudRate, WordLength, StopBits, Parity, Mode, HwFlowCtl,
    OverSampling, OneBitSampling, ClockPrescaler; } UART_InitTypeDef;
typedef struct { USART_TypeDef *Instance; UART_InitTypeDef Init; } UART_HandleTypeDef;
typedef struct { uint32_t PLLState, PLLSource, PLLM, PLLN, PLLP, PLLQ, PLLR; } RCC_PLLInitTypeDef;
typedef struct { uint32_t OscillatorType, HSIState, HSICalibrationValue; RCC_PLLInitTypeDef PLL; } RCC_OscInitTypeDef;
typedef struct { uint32_t ClockType, SYSCLKSource, AHBCLKDivider, APB1CLKDivider, APB2CLKDivider; } RCC_ClkInitTypeDef;
typedef struct { uint32_t PeriphClockSelection, Lpuart1ClockSelection; } RCC_PeriphCLKInitTypeDef;
typedef int HAL_StatusTypeDef;
#define HAL_OK 0
#define PWR_REGULATOR_VOLTAGE_SCALE1_BOOST 1U
#define RCC_OSCILLATORTYPE_HSI 1U
#define RCC_HSI_ON 1U
#define RCC_HSICALIBRATION_DEFAULT 64U
#define RCC_PLL_ON 1U
#define RCC_PLLSOURCE_HSI 1U
#define RCC_PLLM_DIV4 4U
#define RCC_PLLP_DIV2 2U
#define RCC_PLLQ_DIV2 2U
#define RCC_PLLR_DIV2 2U
#define RCC_CLOCKTYPE_HCLK 1U
#define RCC_CLOCKTYPE_SYSCLK 2U
#define RCC_CLOCKTYPE_PCLK1 4U
#define RCC_CLOCKTYPE_PCLK2 8U
#define RCC_SYSCLKSOURCE_PLLCLK 1U
#define RCC_SYSCLK_DIV1 1U
#define RCC_HCLK_DIV1 1U
#define FLASH_LATENCY_4 4U
#define GPIO_MODE_OUTPUT_PP 1U
#define GPIO_MODE_AF_PP 2U
#define GPIO_NOPULL 0U
#define GPIO_SPEED_FREQ_LOW 0U
#define GPIO_SPEED_FREQ_HIGH 2U
#define GPIO_PIN_RESET 0U
#define TIM_COUNTERMODE_UP 0U
#define TIM_CLOCKDIVISION_DIV1 0U
#define TIM_AUTORELOAD_PRELOAD_DISABLE 0U
#define TIM_OCMODE_PWM1 6U
#define TIM_OCPOLARITY_HIGH 0U
#define TIM_OCFAST_DISABLE 0U
#define TIM_CHANNEL_2 4U
#define TIM_CHANNEL_ALL 0x3cU
#define TIM_ICPOLARITY_RISING 0U
#define TIM_ICSELECTION_DIRECTTI 1U
#define TIM_ICPSC_DIV1 0U
#define RCC_PERIPHCLK_LPUART1 1U
#define RCC_LPUART1CLKSOURCE_PCLK1 1U
#define UART_WORDLENGTH_8B 0U
#define UART_STOPBITS_1 0U
#define UART_PARITY_NONE 0U
#define UART_MODE_TX_RX 12U
#define UART_HWCONTROL_NONE 0U
#define UART_OVERSAMPLING_16 0U
#define UART_ONE_BIT_SAMPLE_DISABLE 0U
#define UART_PRESCALER_DIV4 2U
#define LPUART1_IRQn 70U

extern uint32_t mock_primask, mock_tick;
static inline uint32_t __get_PRIMASK(void) { return mock_primask; }
static inline void __disable_irq(void) { mock_primask = 1U; }
static inline void __set_PRIMASK(uint32_t value) { mock_primask = value; }
#define __DSB() ((void)0)
#define __DMB() ((void)0)
#define __NOP() ((void)0)
#define __HAL_RCC_GPIOA_CLK_ENABLE() ((void)0)
#define __HAL_RCC_GPIOB_CLK_ENABLE() (RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN)
#define __HAL_RCC_GPIOC_CLK_ENABLE() (RCC->AHB2ENR |= RCC_AHB2ENR_GPIOCEN)
#define __HAL_RCC_TIM4_CLK_ENABLE() (RCC->APB1ENR1 |= RCC_APB1ENR1_TIM4EN)
#define __HAL_RCC_TIM2_CLK_ENABLE() (RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN)
#define __HAL_RCC_LPUART1_CLK_ENABLE() ((void)0)

HAL_StatusTypeDef HAL_Init(void);
uint32_t HAL_GetTick(void);
HAL_StatusTypeDef HAL_PWREx_ControlVoltageScaling(uint32_t value);
HAL_StatusTypeDef HAL_RCC_OscConfig(RCC_OscInitTypeDef *value);
HAL_StatusTypeDef HAL_RCC_ClockConfig(RCC_ClkInitTypeDef *value, uint32_t latency);
uint32_t HAL_RCC_GetHCLKFreq(void);
uint32_t HAL_RCC_GetPCLK1Freq(void);
uint32_t HAL_RCC_GetPCLK2Freq(void);
void HAL_GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *value);
void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint32_t pin, uint32_t state);
HAL_StatusTypeDef HAL_TIM_PWM_Init(TIM_HandleTypeDef *value);
HAL_StatusTypeDef HAL_TIM_PWM_ConfigChannel(TIM_HandleTypeDef *value, TIM_OC_InitTypeDef *oc, uint32_t channel);
HAL_StatusTypeDef HAL_TIM_Encoder_Init(TIM_HandleTypeDef *value, TIM_Encoder_InitTypeDef *enc);
HAL_StatusTypeDef HAL_TIM_Encoder_Start(TIM_HandleTypeDef *value, uint32_t channel);
HAL_StatusTypeDef HAL_RCCEx_PeriphCLKConfig(RCC_PeriphCLKInitTypeDef *value);
HAL_StatusTypeDef HAL_UART_Init(UART_HandleTypeDef *value);
HAL_StatusTypeDef HAL_UARTEx_DisableFifoMode(UART_HandleTypeDef *value);
void HAL_NVIC_SetPriority(uint32_t irq, uint32_t preempt, uint32_t sub);
void HAL_NVIC_EnableIRQ(uint32_t irq);
#endif
