#ifndef SAFETY_MOCK_HAL_H
#define SAFETY_MOCK_HAL_H
#include <stdint.h>
#include <stddef.h>
typedef enum { HAL_OK, HAL_ERROR } HAL_StatusTypeDef;
typedef enum { GPIO_PIN_RESET, GPIO_PIN_SET } GPIO_PinState;
typedef struct { uint32_t CNT, compare[2], CR1, CR2, CCMR1, ARR, EGR, BDTR, CCER; } TIM_TypeDef;
typedef struct { uint32_t TIMxCR, PERxR, SETx1R, SETx2R, RSTx1R, RSTx2R, CNTxR; } HRTIM_Timerx_TypeDef;
typedef struct { uint32_t MCR; } HRTIM_Master_TypeDef;
typedef struct { uint32_t CR1, CR2, ODSR, ODISR, OENR; } HRTIM_Common_TypeDef;
typedef struct { HRTIM_Master_TypeDef sMasterRegs; HRTIM_Timerx_TypeDef sTimerxRegs[6]; HRTIM_Common_TypeDef sCommonRegs; } HRTIM_TypeDef;
typedef struct { TIM_TypeDef *Instance; } TIM_HandleTypeDef;
typedef struct { TIM_TypeDef *Instance; } LPTIM_HandleTypeDef;
extern TIM_TypeDef encoder_timers[21];
#define TIM1 (&encoder_timers[1])
#define TIM2 (&encoder_timers[2])
#define TIM3 (&encoder_timers[3])
#define TIM4 (&encoder_timers[4])
#define TIM5 (&encoder_timers[5])
#define TIM8 (&encoder_timers[8])
#define TIM20 (&encoder_timers[20])
#define TIM_CHANNEL_ALL 0xFFFFU
#define __HAL_TIM_GET_COUNTER(h) ((h)->Instance->CNT)
HAL_StatusTypeDef HAL_TIM_Encoder_Start(TIM_HandleTypeDef *, uint32_t);
HAL_StatusTypeDef HAL_LPTIM_Encoder_Start(LPTIM_HandleTypeDef *, uint32_t);
typedef struct { uint32_t compare[6][2]; HRTIM_TypeDef *Instance; } HRTIM_HandleTypeDef;
typedef struct { uint32_t pins, BSRR, MODER, OTYPER, PUPDR; } GPIO_TypeDef;
extern GPIO_TypeDef mock_gpio[3];
#define GPIOG (&mock_gpio[0])
#define GPIOC (&mock_gpio[1])
#define GPIOD (&mock_gpio[2])
#define GPIO_PIN_0 (1U << 0)
#define GPIO_PIN_1 (1U << 1)
#define GPIO_PIN_2 (1U << 2)
#define GPIO_PIN_3 (1U << 3)
#define GPIO_PIN_4 (1U << 4)
#define GPIO_PIN_10 (1U << 10)
#define GPIO_PIN_12 (1U << 12)
#define TIM_CHANNEL_1 0U
#define TIM_CHANNEL_2 1U
#define HRTIM_TIMERINDEX_TIMER_A 0U
#define HRTIM_TIMERINDEX_TIMER_B 1U
#define HRTIM_TIMERINDEX_TIMER_C 2U
#define HRTIM_TIMERINDEX_TIMER_D 3U
#define HRTIM_TIMERINDEX_TIMER_E 4U
#define HRTIM_TIMERINDEX_TIMER_F 5U
#define HRTIM_COMPAREUNIT_1 0U
#define HRTIM_COMPAREUNIT_2 1U
#define HRTIM_OUTPUT_TA1 (1U << 0)
#define HRTIM_OUTPUT_TA2 (1U << 1)
#define HRTIM_OUTPUT_TB1 (1U << 2)
#define HRTIM_OUTPUT_TB2 (1U << 3)
#define HRTIM_OUTPUT_TC1 (1U << 4)
#define HRTIM_OUTPUT_TC2 (1U << 5)
#define HRTIM_OUTPUT_TD1 (1U << 6)
#define HRTIM_OUTPUT_TD2 (1U << 7)
#define HRTIM_OUTPUT_TE1 (1U << 8)
#define HRTIM_OUTPUT_TE2 (1U << 9)
#define HRTIM_OUTPUT_TF1 (1U << 10)
#define HRTIM_OUTPUT_TF2 (1U << 11)
#define HRTIM_TIMERID_TIMER_A (1U << 0)
#define HRTIM_TIMERID_TIMER_B (1U << 1)
#define HRTIM_TIMERID_TIMER_C (1U << 2)
#define HRTIM_TIMERID_TIMER_D (1U << 3)
#define HRTIM_TIMERID_TIMER_E (1U << 4)
#define HRTIM_TIMERID_TIMER_F (1U << 5)
uint32_t __get_PRIMASK(void);
uint32_t __get_IPSR(void);
void __disable_irq(void);
void __set_PRIMASK(uint32_t);
uint32_t HAL_GetTick(void);
void HAL_Delay(uint32_t);
void HAL_GPIO_WritePin(GPIO_TypeDef *, uint32_t, GPIO_PinState);
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *, uint32_t);
HAL_StatusTypeDef HAL_HRTIM_WaveformCountStart(HRTIM_HandleTypeDef *, uint32_t);
HAL_StatusTypeDef HAL_HRTIM_WaveformOutputStart(HRTIM_HandleTypeDef *, uint32_t);
HAL_StatusTypeDef HAL_HRTIM_WaveformCountStop(HRTIM_HandleTypeDef *, uint32_t);
HAL_StatusTypeDef HAL_HRTIM_WaveformOutputStop(HRTIM_HandleTypeDef *, uint32_t);
HAL_StatusTypeDef HAL_TIM_PWM_Start(TIM_HandleTypeDef *, uint32_t);
HAL_StatusTypeDef HAL_TIM_PWM_Stop(TIM_HandleTypeDef *, uint32_t);
void mock_compare_write(uint32_t *, uint32_t);
#define __HAL_TIM_SET_COMPARE(h,c,v) mock_compare_write(&(h)->Instance->compare[c],v)
#define __HAL_TIM_GET_COMPARE(h,c) ((h)->Instance->compare[c])
#define __HAL_HRTIM_SETCOMPARE(h,t,c,v) mock_compare_write(&(h)->compare[t][c],v)
void __DSB(void);
#define HRTIM_MCR_TACEN (1U << 17)
#define HRTIM_TIMCR_CK_PSC 7U
#define HRTIM_PRESCALERRATIO_DIV1 5U
#define HRTIM_TIMCR_PREEN (1U << 27)
#define HRTIM_CR1_TAUDIS (1U << 1)
#define HRTIM_CR2_TASWU (1U << 1)
#define HRTIM_SET1R_SST 1U
#define HRTIM_SET2R_SST 1U
#define HRTIM_RST1R_SRT 1U
#define HRTIM_RST2R_SRT 1U
#define HRTIM_SET1R_PER 4U
#define HRTIM_SET2R_PER 4U
#define HRTIM_RST1R_CMP1 8U
#define HRTIM_RST2R_CMP2 16U
#define TIM_CR1_CEN 1U
#define TIM_CR1_UDIS 2U
#define TIM_CR2_CCPC 1U
#define TIM_CCMR1_OC1M ((7U << 4) | (1U << 16))
#define TIM_CCMR1_OC2M ((7U << 12) | (1U << 24))
#define TIM_CCMR1_OC1PE (1U << 3)
#define TIM_CCMR1_OC2PE (1U << 11)
#define TIM_OCMODE_FORCED_INACTIVE (4U << 4)
#define TIM_OCMODE_FORCED_ACTIVE (5U << 4)
#define TIM_OCMODE_PWM1 (6U << 4)
#define TIM_EGR_UG 1U
#define TIM_BDTR_MOE (1U << 15)
#define TIM_CCER_CC1E 1U
#define TIM_CCER_CC1NE 4U
#define TIM_CCER_CC2E 16U
#endif
