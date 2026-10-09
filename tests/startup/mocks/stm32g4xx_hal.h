#ifndef MOCK_HAL_H
#define MOCK_HAL_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
typedef enum { HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT } HAL_StatusTypeDef;
typedef struct { uint32_t CNT, DIER, SR, CR1; } TIM_TypeDef;
extern TIM_TypeDef mock_timers[21];
#define TIM1 (&mock_timers[1])
#define TIM2 (&mock_timers[2])
#define TIM3 (&mock_timers[3])
#define TIM4 (&mock_timers[4])
#define TIM5 (&mock_timers[5])
#define TIM6 (&mock_timers[6])
#define TIM8 (&mock_timers[8])
#define TIM20 (&mock_timers[20])
typedef struct {
    uint32_t Period, Prescaler, ClockDivision, CounterMode, AutoReloadPreload;
} TIM_Base_InitTypeDef;
typedef struct { TIM_TypeDef *Instance; TIM_Base_InitTypeDef Init; } TIM_HandleTypeDef;
typedef struct { TIM_TypeDef *Instance; } LPTIM_HandleTypeDef;
typedef struct { uint32_t APB1CLKDivider; } RCC_ClkInitTypeDef;
typedef struct { uint32_t interrupts; } DMA_HandleTypeDef;
typedef struct { DMA_HandleTypeDef *DMA_Handle; } ADC_HandleTypeDef;
typedef struct { uint32_t unused; } OPAMP_HandleTypeDef;
typedef enum { TIM6_DAC_IRQn, DMA1_Channel8_IRQn, DMA1_Channel2_IRQn,
    DMA1_Channel4_IRQn, DMA1_Channel5_IRQn, DMA1_Channel3_IRQn } IRQn_Type;
#define __NVIC_PRIO_BITS 4
#define RCC_HCLK_DIV1 1U
#define RCC_HCLK_DIV2 2U
#define TIM_IT_UPDATE 1U
#define TIM_FLAG_UPDATE 1U
#define TIM_CLOCKDIVISION_DIV1 0U
#define TIM_COUNTERMODE_UP 0U
#define TIM_AUTORELOAD_PRELOAD_DISABLE 0U
#define TIM_CHANNEL_ALL 0xFFFFU
#define HAL_TICK_FREQ_1KHZ 1U
#define DMA_IT_HT 1U
#define DMA_IT_TC 2U
#define DMA_IT_TE 4U
#define __HAL_RCC_TIM6_CLK_ENABLE() ((void)0)
#define __HAL_TIM_DISABLE_IT(h, bits) ((h)->Instance->DIER &= ~(bits))
#define __HAL_TIM_ENABLE_IT(h, bits) ((h)->Instance->DIER |= (bits))
#define __HAL_TIM_DISABLE(h) ((h)->Instance->CR1 = 0U)
#define __HAL_TIM_CLEAR_FLAG(h, bits) ((h)->Instance->SR &= ~(bits))
#define __HAL_TIM_SET_COUNTER(h, value) ((h)->Instance->CNT = (value))
#define __HAL_TIM_GET_COUNTER(h) ((h)->Instance->CNT)
#define __HAL_DMA_DISABLE_IT(h, bits) ((h)->interrupts &= ~(bits))
extern uint32_t uwTickFreq, uwTickPrio;
void HAL_RCC_GetClockConfig(RCC_ClkInitTypeDef *, uint32_t *);
uint32_t HAL_RCC_GetPCLK1Freq(void);
void HAL_NVIC_DisableIRQ(IRQn_Type);
void HAL_NVIC_EnableIRQ(IRQn_Type);
void HAL_NVIC_ClearPendingIRQ(IRQn_Type);
void HAL_NVIC_SetPriority(IRQn_Type, uint32_t, uint32_t);
uint32_t NVIC_GetEnableIRQ(IRQn_Type);
HAL_StatusTypeDef HAL_TIM_Base_Init(TIM_HandleTypeDef *);
HAL_StatusTypeDef HAL_TIM_Base_Start_IT(TIM_HandleTypeDef *);
HAL_StatusTypeDef HAL_TIM_Encoder_Start(TIM_HandleTypeDef *, uint32_t);
HAL_StatusTypeDef HAL_LPTIM_Encoder_Start(LPTIM_HandleTypeDef *, uint32_t);
HAL_StatusTypeDef HAL_ADC_Start_DMA(ADC_HandleTypeDef *, uint32_t *, uint32_t);
HAL_StatusTypeDef HAL_ADC_Stop_DMA(ADC_HandleTypeDef *);
HAL_StatusTypeDef HAL_OPAMP_Start(OPAMP_HandleTypeDef *);
HAL_StatusTypeDef HAL_OPAMP_Stop(OPAMP_HandleTypeDef *);
void HAL_IncTick(void);
uint32_t HAL_GetTick(void);
uint32_t __get_PRIMASK(void);
void __disable_irq(void);
void __set_PRIMASK(uint32_t);
#endif
