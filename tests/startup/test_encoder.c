#include "encoder_bsp.h"
#include "stm32g4xx_hal.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
TIM_TypeDef mock_timers[21];
TIM_HandleTypeDef htim1 = { .Instance = TIM1 }, htim2 = { .Instance = TIM2 },
    htim3 = { .Instance = TIM3 }, htim4 = { .Instance = TIM4 },
    htim5 = { .Instance = TIM5 }, htim8 = { .Instance = TIM8 },
    htim20 = { .Instance = TIM20 };
static TIM_TypeDef lptim;
LPTIM_HandleTypeDef hlptim1 = { .Instance = &lptim };
int mock_critical_depth;
static uint32_t primask;
uint32_t __get_PRIMASK(void) { return primask; }
void __disable_irq(void) { primask = 1U; }
void __set_PRIMASK(uint32_t n) { primask = n; }
bool Motor_IsEnabled(motor_id_t id) { (void)id; return false; }
void Motor_InvalidatePositionFeedback(motor_id_t id) { (void)id; }
static int starts[21], lp_starts, failure;
uint32_t HAL_GetTick(void) { return 123; }
HAL_StatusTypeDef HAL_TIM_Encoder_Start(TIM_HandleTypeDef *h, uint32_t channel)
{
    assert(channel == TIM_CHANNEL_ALL);
    int index = (int)(h->Instance - mock_timers);
    assert(++starts[index] == 1);
    return failure == 1 && h->Instance == TIM8 ? HAL_ERROR : HAL_OK;
}
HAL_StatusTypeDef HAL_LPTIM_Encoder_Start(LPTIM_HandleTypeDef *h, uint32_t period)
{
    assert(h == &hlptim1 && period == 65535 && ++lp_starts == 1);
    return failure == 2 ? HAL_ERROR : HAL_OK;
}
int main(int argc, char **argv)
{
    failure = argc > 1 ? atoi(argv[1]) : 0;
    TIM8->CNT = 65535;
    if (failure) {
        assert(!Encoder_BSP_Init());
        assert(!Encoder_BSP_Init()); /* A failure is not retried/hidden. */
        puts("Encoder HAL failure propagated OK");
        return 0;
    }
    assert(Encoder_BSP_Init());
    assert(!Encoder_BSP_IsCalibrated(MOTOR_INDEX));
    TIM8->CNT = 0;
    assert(Encoder_BSP_GetRawCount(MOTOR_INDEX) == 1); /* forward wrap */
    TIM8->CNT = 65535;
    assert(Encoder_BSP_GetRawCount(MOTOR_INDEX) == 0); /* reverse wrap */
    motor_id_t ids[] = { MOTOR_THUMB, MOTOR_INDEX, MOTOR_MIDDLE, MOTOR_RING,
        MOTOR_LITTLE, MOTOR_WRIST_X, MOTOR_WRIST_Y };
    TIM_TypeDef *timers[] = { TIM1, TIM8, TIM2, TIM4, TIM20, TIM5, TIM3 };
    for (unsigned i = 0; i < 7; ++i) {
        assert(Encoder_BSP_IsLocal(ids[i]));
        Encoder_BSP_Reset(ids[i]);
        ++timers[i]->CNT;
        assert(Encoder_BSP_GetRawCount(ids[i]) == 1);
    }
    assert(Encoder_BSP_IsLocal(MOTOR_PALM));
    lptim.CNT = 9; assert(Encoder_BSP_GetRawCount(MOTOR_PALM) == 9);
    Encoder_BSP_Reset(MOTOR_PALM);
    assert(lptim.CNT == 9 && Encoder_BSP_GetRawCount(MOTOR_PALM) == 0);
    lptim.CNT = 10; assert(Encoder_BSP_GetRawCount(MOTOR_PALM) == 1);
    assert(Encoder_BSP_Init());
    assert(Encoder_BSP_GetRawCount(MOTOR_PALM) == 1 && lp_starts == 1);
    assert(!Encoder_BSP_IsLocal(MOTOR_COUNT));
    assert(mock_critical_depth == 0);
    puts("Encoder BSP: sole start, eight mappings, wrap, logical reset, idempotence OK");
    return 0;
}
