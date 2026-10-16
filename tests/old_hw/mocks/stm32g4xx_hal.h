#ifndef OLD_HW_MOCK_HAL_H
#define OLD_HW_MOCK_HAL_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
typedef enum {HAL_OK,HAL_ERROR} HAL_StatusTypeDef;
typedef enum {GPIO_PIN_RESET,GPIO_PIN_SET} GPIO_PinState;
typedef struct {uint32_t CNT,CR1,CCER,BDTR,CCR1,CCR2,CCR3,CCR4,PSC,ARR,SMCR,CCMR1,CCMR2,EGR;} TIM_TypeDef;
extern TIM_TypeDef mock_timers[21];
#define TIM1 (&mock_timers[1])
#define TIM2 (&mock_timers[2])
#define TIM3 (&mock_timers[3])
#define TIM4 (&mock_timers[4])
#define TIM5 (&mock_timers[5])
#define TIM8 (&mock_timers[8])
#define TIM15 (&mock_timers[15])
#define TIM20 (&mock_timers[20])
typedef struct {uint32_t Prescaler,CounterMode,Period,ClockDivision,AutoReloadPreload,RepetitionCounter;} TIM_Base_InitTypeDef;
typedef struct {TIM_TypeDef *Instance;TIM_Base_InitTypeDef Init;} TIM_HandleTypeDef;
typedef struct {TIM_TypeDef *Instance;} LPTIM_HandleTypeDef;
typedef struct {uint32_t OCMode,Pulse,OCPolarity,OCNPolarity,OCFastMode,OCIdleState,OCNIdleState;} TIM_OC_InitTypeDef;
typedef struct {uint32_t EncoderMode,IC1Polarity,IC2Polarity,IC1Selection,IC2Selection,IC1Prescaler,IC2Prescaler,IC1Filter,IC2Filter;} TIM_Encoder_InitTypeDef;
typedef struct {uint32_t ODR,BSRR,MODER,OTYPER,PUPDR,IDR,AFR[2];} GPIO_TypeDef;
extern GPIO_TypeDef mock_gpio[4];
#define GPIOA (&mock_gpio[0])
#define GPIOB (&mock_gpio[1])
#define GPIOC (&mock_gpio[2])
#define GPIOD (&mock_gpio[3])
typedef struct {uint32_t Pin,Mode,Pull,Speed,Alternate;} GPIO_InitTypeDef;
typedef struct {uint32_t AHB2ENR,APB1ENR1,APB2ENR,CRRCR,CCIPR;} RCC_TypeDef;
typedef struct {uint32_t PR1,IMR1,RTSR1,FTSR1;} EXTI_TypeDef;
typedef struct {uint32_t EXTICR[4];} SYSCFG_TypeDef;
extern EXTI_TypeDef mock_exti;
extern SYSCFG_TypeDef mock_syscfg;
#define EXTI (&mock_exti)
#define SYSCFG (&mock_syscfg)
extern RCC_TypeDef mock_rcc;
#define RCC (&mock_rcc)
typedef struct {uint32_t PLLState,PLLSource,PLLM,PLLN,PLLP,PLLQ,PLLR;} PLL_InitTypeDef;
typedef struct {uint32_t OscillatorType,HSIState,HSICalibrationValue;PLL_InitTypeDef PLL;} RCC_OscInitTypeDef;
typedef struct {uint32_t ClockType,SYSCLKSource,AHBCLKDivider,APB1CLKDivider,APB2CLKDivider;} RCC_ClkInitTypeDef;
typedef struct {uint32_t PeriphClockSelection,Usart3ClockSelection,Lpuart1ClockSelection;} RCC_PeriphCLKInitTypeDef;
typedef struct {uint32_t CR1;} USART_TypeDef;
extern USART_TypeDef mock_uarts[2];
#define LPUART1 (&mock_uarts[0])
#define USART3 (&mock_uarts[1])
typedef struct {USART_TypeDef *Instance; struct {uint32_t BaudRate,WordLength,StopBits,Parity,Mode,HwFlowCtl,OverSampling,OneBitSampling,ClockPrescaler;} Init; uint32_t gState,TxXferCount,ErrorCode;} UART_HandleTypeDef;
typedef enum {LPUART1_IRQn,USART3_IRQn,EXTI15_10_IRQn} IRQn_Type;
#define GPIO_PIN_0 (1U << 0)
#define GPIO_PIN_1 (1U << 1)
#define GPIO_PIN_2 (1U << 2)
#define GPIO_PIN_3 (1U << 3)
#define GPIO_PIN_4 (1U << 4)
#define GPIO_PIN_5 (1U << 5)
#define GPIO_PIN_6 (1U << 6)
#define GPIO_PIN_7 (1U << 7)
#define GPIO_PIN_8 (1U << 8)
#define GPIO_PIN_9 (1U << 9)
#define GPIO_PIN_10 (1U << 10)
#define GPIO_PIN_11 (1U << 11)
#define GPIO_PIN_12 (1U << 12)
#define GPIO_PIN_13 (1U << 13)
#define GPIO_PIN_14 (1U << 14)
#define GPIO_PIN_15 (1U << 15)
#define TIM_CHANNEL_1 0U
#define TIM_CHANNEL_2 4U
#define TIM_CHANNEL_3 8U
#define TIM_CHANNEL_4 12U
#define TIM_CHANNEL_ALL 0xFFFFU
#define GPIO_MODE_INPUT 0U
#define GPIO_MODE_OUTPUT_PP 1U
#define GPIO_MODE_AF_PP 2U
#define GPIO_MODE_IT_RISING_FALLING 3U
#define GPIO_PULLUP 1U
#define GPIO_NOPULL 0U
#define GPIO_SPEED_FREQ_LOW 0U
#define GPIO_SPEED_FREQ_HIGH 3U
#define TIM_CR1_CEN 1U
#define TIM_BDTR_MOE (1U << 15)
#define TIM_BDTR_AOE (1U << 14)
#define RCC_AHB2ENR_GPIOAEN 1U
#define RCC_AHB2ENR_GPIOBEN 2U
#define RCC_APB2ENR_TIM1EN (1U << 11)
#define RCC_APB1ENR1_TIM4EN 4U
#define GPIO_AF1_TIM2 1U
#define GPIO_AF2_TIM3 2U
#define GPIO_AF4_TIM8 4U
#define GPIO_AF1_TIM15 1U
#define GPIO_AF3_TIM20 3U
#define GPIO_AF6_TIM20 6U
#define GPIO_AF7_USART3 7U
#define GPIO_AF12_LPUART1 12U
#define RCC_PLLM_DIV4 4U
#define RCC_PLLP_DIV2 2U
#define RCC_PLLQ_DIV2 2U
#define RCC_PLLR_DIV2 2U
#define RCC_SYSCLK_DIV1 1U
#define RCC_HCLK_DIV1 1U
#define FLASH_LATENCY_4 4U
#define PWR_REGULATOR_VOLTAGE_SCALE1_BOOST 1U
#define RCC_CLOCKTYPE_HCLK 1U
#define RCC_CLOCKTYPE_PCLK1 1U
#define RCC_CLOCKTYPE_PCLK2 1U
#define RCC_CLOCKTYPE_SYSCLK 1U
#define RCC_HSICALIBRATION_DEFAULT 1U
#define RCC_HSI_ON 1U
#define RCC_LPUART1CLKSOURCE_PCLK1 1U
#define RCC_OSCILLATORTYPE_HSI 1U
#define RCC_PERIPHCLK_LPUART1 1U
#define RCC_PERIPHCLK_USART3 1U
#define RCC_PLLSOURCE_HSI 1U
#define RCC_PLL_ON 1U
#define RCC_SYSCLKSOURCE_PLLCLK 1U
#define RCC_USART3CLKSOURCE_PCLK1 1U
#define TIM_AUTORELOAD_PRELOAD_DISABLE 1U
#define TIM_AUTORELOAD_PRELOAD_ENABLE 1U
#define TIM_CLOCKDIVISION_DIV1 1U
#define TIM_COUNTERMODE_UP 1U
#define TIM_ENCODERMODE_TI12 3U
#define TIM_ICPOLARITY_RISING 1U
#define TIM_ICPSC_DIV1 1U
#define TIM_ICSELECTION_DIRECTTI 1U
#define TIM_OCFAST_DISABLE 1U
#define TIM_OCIDLESTATE_RESET 1U
#define TIM_OCMODE_PWM1 (6U << 4)
#define TIM_OCNIDLESTATE_RESET 1U
#define TIM_OCNPOLARITY_HIGH 1U
#define TIM_OCPOLARITY_HIGH 1U
#define UART_HWCONTROL_NONE 1U
#define UART_MODE_TX_RX 1U
#define UART_ONE_BIT_SAMPLE_DISABLE 1U
#define UART_OVERSAMPLING_16 1U
#define UART_PARITY_NONE 1U
#define UART_PRESCALER_DIV1 1U
#define UART_STOPBITS_1 1U
#define UART_WORDLENGTH_8B 1U
#define UART_RXFIFO_THRESHOLD_1_8 0U
#define UART_TXFIFO_THRESHOLD_1_8 0U
#define USART_CR1_FIFOEN (1U << 29)
#define __HAL_RCC_GPIOA_CLK_ENABLE() (mock_rcc.AHB2ENR |= 1U)
#define __HAL_RCC_GPIOB_CLK_ENABLE() (mock_rcc.AHB2ENR |= 2U)
#define __HAL_RCC_GPIOC_CLK_ENABLE() (mock_rcc.AHB2ENR |= 4U)
#define __HAL_RCC_GPIOD_CLK_ENABLE() (mock_rcc.AHB2ENR |= 8U)
#define __HAL_RCC_TIM1_CLK_ENABLE() (mock_rcc.APB2ENR |= RCC_APB2ENR_TIM1EN)
#define __HAL_RCC_TIM4_CLK_ENABLE() (mock_rcc.APB1ENR1 |= RCC_APB1ENR1_TIM4EN)
#define __HAL_RCC_TIM2_CLK_ENABLE() (mock_rcc.APB1ENR1 |= RCC_APB1ENR1_TIM2EN)
#define __HAL_RCC_TIM3_CLK_ENABLE() (mock_rcc.APB1ENR1 |= RCC_APB1ENR1_TIM3EN)
#define __HAL_RCC_TIM8_CLK_ENABLE() (mock_rcc.APB2ENR |= RCC_APB2ENR_TIM8EN)
#define __HAL_RCC_TIM15_CLK_ENABLE() ((void)0)
#define __HAL_RCC_TIM20_CLK_ENABLE() (mock_rcc.APB2ENR |= RCC_APB2ENR_TIM20EN)
#define __HAL_RCC_USART3_CLK_ENABLE() ((void)0)
#define __HAL_RCC_LPUART1_CLK_ENABLE() ((void)0)
#define __HAL_TIM_GET_COUNTER(h) ((h)->Instance->CNT)
void __DSB(void); void __NOP(void); void __disable_irq(void);
uint32_t __get_PRIMASK(void); void __set_PRIMASK(uint32_t);
uint32_t __get_IPSR(void);
void HAL_NVIC_EnableIRQ(IRQn_Type);
uint32_t NVIC_GetEnableIRQ(IRQn_Type);
#define __HAL_RCC_SYSCFG_CLK_ENABLE() ((void)0)
#define TIM_SMCR_SMS (7U | (1U << 16))
#define TIM_CCER_CC1E 1U
#define TIM_CCER_CC2E (1U << 4)
#define TIM_CR1_DIR (1U << 4)
#define TIM_CR1_CMS (3U << 5)
#define TIM_CR1_UDIS 2U
#define TIM_BDTR_LOCK (3U << 8)
#define TIM_CCMR1_OC1M ((7U << 4) | (1U << 16))
#define TIM_CCMR1_OC1PE (1U << 3)
#define TIM_EGR_UG 1U
#define RCC_APB1ENR1_TIM2EN 1U
#define RCC_APB1ENR1_TIM3EN 2U
#define RCC_APB2ENR_TIM8EN (1U << 13)
#define RCC_APB2ENR_TIM20EN (1U << 20)
uint32_t HAL_GetTick(void);
void HAL_GPIO_Init(GPIO_TypeDef *,const GPIO_InitTypeDef *);
void HAL_GPIO_WritePin(GPIO_TypeDef *,uint16_t,GPIO_PinState);
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *,uint16_t);
void HAL_GPIO_TogglePin(GPIO_TypeDef *,uint16_t);
HAL_StatusTypeDef HAL_PWREx_ControlVoltageScaling(uint32_t);
HAL_StatusTypeDef HAL_RCC_OscConfig(const RCC_OscInitTypeDef *);
HAL_StatusTypeDef HAL_RCC_ClockConfig(const RCC_ClkInitTypeDef *,uint32_t);
HAL_StatusTypeDef HAL_RCCEx_PeriphCLKConfig(const RCC_PeriphCLKInitTypeDef *);
uint32_t HAL_RCC_GetHCLKFreq(void); uint32_t HAL_RCC_GetPCLK1Freq(void); uint32_t HAL_RCC_GetPCLK2Freq(void);
HAL_StatusTypeDef HAL_TIM_PWM_Init(TIM_HandleTypeDef *);
HAL_StatusTypeDef HAL_TIM_PWM_ConfigChannel(TIM_HandleTypeDef *,const TIM_OC_InitTypeDef *,uint32_t);
HAL_StatusTypeDef HAL_TIM_Encoder_Init(TIM_HandleTypeDef *,const TIM_Encoder_InitTypeDef *);
HAL_StatusTypeDef HAL_TIM_Encoder_Start(TIM_HandleTypeDef *,uint32_t);
HAL_StatusTypeDef HAL_UART_Init(UART_HandleTypeDef *);
HAL_StatusTypeDef HAL_UARTEx_DisableFifoMode(UART_HandleTypeDef *);
HAL_StatusTypeDef HAL_UARTEx_EnableFifoMode(UART_HandleTypeDef *);
HAL_StatusTypeDef HAL_UARTEx_SetRxFifoThreshold(UART_HandleTypeDef *,uint32_t);
HAL_StatusTypeDef HAL_UARTEx_SetTxFifoThreshold(UART_HandleTypeDef *,uint32_t);
void HAL_NVIC_DisableIRQ(IRQn_Type); void HAL_NVIC_ClearPendingIRQ(IRQn_Type);
void HAL_NVIC_SetPriority(IRQn_Type,uint32_t,uint32_t);
#if N_PULSE_DUAL_BOARD_LINK
typedef struct {uint32_t CR,SR,DR;} RNG_TypeDef;
extern RNG_TypeDef mock_rng;
#define RNG (&mock_rng)
#define RCC_CRRCR_HSI48ON 1U
#define RCC_CRRCR_HSI48RDY 2U
#define RCC_CCIPR_CLK48SEL 3U
#define RNG_CR_RNGEN 4U
#define RNG_SR_DRDY 1U
#define RNG_SR_CECS 2U
#define RNG_SR_SECS 4U
#define RNG_SR_CEIS 8U
#define RNG_SR_SEIS 16U
#define __HAL_RCC_RNG_CLK_ENABLE() ((void)0)
#define __HAL_RCC_RNG_FORCE_RESET() ((void)0)
#define __HAL_RCC_RNG_RELEASE_RESET() ((void)0)
#define HAL_UART_STATE_READY 0U
#define UART_CLEAR_OREF 1U
#define UART_CLEAR_NEF 2U
#define UART_CLEAR_FEF 4U
#define UART_CLEAR_PEF 8U
#define UART_RXDATA_FLUSH_REQUEST 1U
#define __HAL_UART_CLEAR_FLAG(u,f) ((void)(u),(void)(f))
#define __HAL_UART_SEND_REQ(u,f) ((void)(u),(void)(f))
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *,uint8_t *,uint16_t);
HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef *,uint8_t *,uint16_t);
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *);
HAL_StatusTypeDef HAL_UART_AbortTransmit(UART_HandleTypeDef *);
#endif
#if N_PULSE_TIMING_DIAGNOSTICS
typedef struct {uint32_t CTRL,CYCCNT;} mock_dwt_t;
typedef struct {uint32_t DEMCR;} mock_core_debug_t;
extern mock_dwt_t mock_dwt;
extern mock_core_debug_t mock_core_debug;
#define DWT (&mock_dwt)
#define CoreDebug (&mock_core_debug)
#define DWT_CTRL_CYCCNTENA_Msk 1U
#define CoreDebug_DEMCR_TRCENA_Msk (1U<<24)
#endif
#endif
