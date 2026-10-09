#include "stm32g4xx_hal.h"
#include <assert.h>
#include <stdio.h>
TIM_TypeDef mock_timers[21];
uint32_t uwTickFreq = 1, uwTickPrio = 15;
extern TIM_HandleTypeDef htim6;
static uint32_t clock_hz = 16000000, divider = RCC_HCLK_DIV1;
static uint32_t tick, priority;
static bool enabled;
static HAL_StatusTypeDef init_status = HAL_OK, start_status = HAL_OK;
HAL_StatusTypeDef HAL_InitTick(uint32_t);
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *);
void HAL_SuspendTick(void);
void HAL_ResumeTick(void);
void HAL_RCC_GetClockConfig(RCC_ClkInitTypeDef *c, uint32_t *l)
{ c->APB1CLKDivider = divider; *l = 0; }
uint32_t HAL_RCC_GetPCLK1Freq(void) { return clock_hz; }
void HAL_NVIC_DisableIRQ(IRQn_Type i) { (void)i; enabled = false; }
void HAL_NVIC_EnableIRQ(IRQn_Type i)
{ assert(i == TIM6_DAC_IRQn && priority == 4); enabled = true; }
void HAL_NVIC_ClearPendingIRQ(IRQn_Type i) { (void)i; }
void HAL_NVIC_SetPriority(IRQn_Type i, uint32_t p, uint32_t s)
{ (void)i; assert(!enabled && s == 0); priority = p; }
HAL_StatusTypeDef HAL_TIM_Base_Init(TIM_HandleTypeDef *h)
{ assert(!enabled); h->Instance->SR |= TIM_FLAG_UPDATE; return init_status; }
HAL_StatusTypeDef HAL_TIM_Base_Start_IT(TIM_HandleTypeDef *h)
{
    assert(!enabled && !(h->Instance->SR & TIM_FLAG_UPDATE));
    assert(h->Instance->CNT == 0);
    h->Instance->DIER |= TIM_IT_UPDATE;
    return start_status;
}
void HAL_IncTick(void) { ++tick; }
int main(void)
{
    assert(HAL_InitTick(4) == HAL_OK && enabled);
    assert(htim6.Init.Prescaler == 15 && htim6.Init.Period == 999);
    clock_hz = 160000000;
    assert(HAL_InitTick(4) == HAL_OK);
    assert(htim6.Init.Prescaler == 159 && uwTickPrio == 4);
    clock_hz = 80000000; divider = RCC_HCLK_DIV2;
    assert(HAL_InitTick(4) == HAL_OK && htim6.Init.Prescaler == 159);
    HAL_TIM_PeriodElapsedCallback(&htim6);
    TIM_HandleTypeDef other = { .Instance = TIM1 };
    HAL_TIM_PeriodElapsedCallback(&other);
    assert(tick == 1);
    HAL_SuspendTick(); assert(!(TIM6->DIER & TIM_IT_UPDATE));
    HAL_ResumeTick(); assert(TIM6->DIER & TIM_IT_UPDATE);
    assert(HAL_InitTick(16) == HAL_ERROR);
    uwTickFreq = 10; assert(HAL_InitTick(4) == HAL_ERROR); uwTickFreq = 1;
    clock_hz = 999999; assert(HAL_InitTick(4) == HAL_ERROR);
    clock_hz = 80000000;
    init_status = HAL_ERROR; assert(HAL_InitTick(4) == HAL_ERROR && !enabled);
    init_status = HAL_OK; start_status = HAL_ERROR;
    assert(HAL_InitTick(4) == HAL_ERROR && !enabled);
    puts("HAL timebase: clocks, single increment, priorities, suspend/resume, failures OK");
    return 0;
}
