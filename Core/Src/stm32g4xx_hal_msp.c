#include "main.h"
/* Peripheral pins/clocks are owned only by bench_hw.c. */
void HAL_MspInit(void)
{
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);
    HAL_PWREx_DisableUCPDDeadBattery();
}
