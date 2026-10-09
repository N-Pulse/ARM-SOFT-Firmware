#include "adc_dma_bsp.h"
#include "stm32g4xx_hal.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
static DMA_HandleTypeDef dma[5];
ADC_HandleTypeDef hadc1 = { &dma[0] }, hadc2 = { &dma[1] },
    hadc3 = { &dma[2] }, hadc4 = { &dma[3] }, hadc5 = { &dma[4] };
OPAMP_HandleTypeDef hopamp1, hopamp2, hopamp3, hopamp4, hopamp5, hopamp6;
static uint32_t enabled[6] = { 1, 1, 1, 1, 1, 1 };
static int starts, amps, stops, fail;
static unsigned stopped_adcs;
static unsigned adc_index(ADC_HandleTypeDef *h)
{
    if (h == &hadc1) return 0;
    if (h == &hadc2) return 1;
    if (h == &hadc3) return 2;
    if (h == &hadc4) return 3;
    assert(h == &hadc5); return 4;
}
uint32_t NVIC_GetEnableIRQ(IRQn_Type irq) { return enabled[irq]; }
void HAL_NVIC_DisableIRQ(IRQn_Type irq) { enabled[irq] = 0; }
void HAL_NVIC_EnableIRQ(IRQn_Type irq) { enabled[irq] = 1; }
void HAL_NVIC_ClearPendingIRQ(IRQn_Type irq) { assert(!enabled[irq]); }
HAL_StatusTypeDef HAL_OPAMP_Start(OPAMP_HandleTypeDef *h)
{ (void)h; ++amps; return HAL_OK; }
HAL_StatusTypeDef HAL_OPAMP_Stop(OPAMP_HandleTypeDef *h)
{ (void)h; ++stops; return HAL_OK; }
HAL_StatusTypeDef HAL_ADC_Stop_DMA(ADC_HandleTypeDef *h)
{ stopped_adcs |= 1U << adc_index(h); ++stops; return HAL_OK; }
HAL_StatusTypeDef HAL_ADC_Start_DMA(ADC_HandleTypeDef *h, uint32_t *b, uint32_t n)
{
    static const IRQn_Type irqs[5] = { DMA1_Channel8_IRQn, DMA1_Channel2_IRQn,
        DMA1_Channel3_IRQn, DMA1_Channel4_IRQn, DMA1_Channel5_IRQn };
    static const uint32_t lengths[5] = { 1, 2, 1, 3, 2 };
    unsigned index = adc_index(h);
    assert(!enabled[irqs[index]] && n == lengths[index]);
    ++starts;
    h->DMA_Handle->interrupts = DMA_IT_HT | DMA_IT_TC | DMA_IT_TE;
    /* Distinct sentinels: Curr5=300, battery=401. */
    for (uint32_t i = 0; i < n; ++i) ((uint16_t *)b)[i] = (uint16_t)(100*(index+1)+i);
    return fail && starts == fail ? HAL_ERROR : HAL_OK;
}
int main(int argc, char **argv)
{
    fail = argc > 1 ? atoi(argv[1]) : 0;
#if N_PULSE_ENABLE_CURRENT_ACQUISITION
    if (fail) {
        assert(!ADC_DMA_BSP_Init());
        assert(stops == 11 && stopped_adcs == 31U);
        for (unsigned i = 0; i < MOTOR_COUNT; ++i) {
            assert(ADC_DMA_BSP_GetRaw((motor_id_t)i) == 0);
            assert(isnan(ADC_DMA_BSP_GetCurrentA((motor_id_t)i)));
        }
        printf("Experimental ADC failure at start %d: all five DMA streams cleaned, current unavailable OK\n",fail);
        return 0;
    }
    assert(ADC_DMA_BSP_Init() && starts == 5 && amps == 6);
    assert(ADC_DMA_BSP_GetRaw(MOTOR_INDEX) == 100);
    assert(ADC_DMA_BSP_GetRaw(MOTOR_MIDDLE) == 200);
    assert(ADC_DMA_BSP_GetRaw(MOTOR_RING) == 201);
    assert(ADC_DMA_BSP_GetRaw(MOTOR_LITTLE) == 500);
    assert(ADC_DMA_BSP_GetRaw(MOTOR_THUMB) == 300);
    assert(ADC_DMA_BSP_GetRaw(MOTOR_PALM) == 400);
    assert(ADC_DMA_BSP_GetRaw(MOTOR_WRIST_X) == 501);
    assert(ADC_DMA_BSP_GetRaw(MOTOR_WRIST_Y) == 402);
    for (unsigned i = 0; i < MOTOR_COUNT; ++i) assert(ADC_DMA_BSP_GetRaw((motor_id_t)i) != 401);
    for (unsigned i = 0; i < 5; ++i) {
        assert(!(dma[i].interrupts & (DMA_IT_HT | DMA_IT_TC)));
        assert(dma[i].interrupts & DMA_IT_TE);
    }
    for (unsigned i = 0; i < 6; ++i) assert(enabled[i]);
    puts("Experimental raw ADC: Curr5=ADC3/PE13, battery=ADC4 rank2 excluded; sample IRQs suppressed OK");
#else
    assert(ADC_DMA_BSP_Init() && starts == 0 && amps == 0);
    for (unsigned i = 0; i < MOTOR_COUNT; ++i) assert(ADC_DMA_BSP_GetRaw((motor_id_t)i) == 0);
    puts("Default ADC acquisition: no hardware starts, no valid samples OK");
#endif
    for (unsigned i = 0; i < MOTOR_COUNT; ++i) assert(isnan(ADC_DMA_BSP_GetCurrentA((motor_id_t)i)));
    assert(ADC_DMA_BSP_GetRaw(MOTOR_COUNT) == 0 && isnan(ADC_DMA_BSP_GetCurrentA(MOTOR_COUNT)));
    return 0;
}
