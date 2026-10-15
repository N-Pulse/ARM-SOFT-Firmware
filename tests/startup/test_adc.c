#include "adc_dma_bsp.h"
#include "stm32g4xx_hal.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
static DMA_HandleTypeDef dma[5];
ADC_HandleTypeDef hadc1 = { .DMA_Handle=&dma[0] }, hadc2 = { .DMA_Handle=&dma[1] },
    hadc3 = { .DMA_Handle=&dma[2] }, hadc4 = { .DMA_Handle=&dma[3] }, hadc5 = { .DMA_Handle=&dma[4] };
OPAMP_HandleTypeDef hopamp1, hopamp2, hopamp3, hopamp4, hopamp5, hopamp6;
static uint32_t enabled[6] = { 1, 1, 1, 1, 1, 1 };
static int starts, amps, stops, fail;
static int fail_amp,fail_cal,fail_init,cal_count,init_count;
static uint16_t *dma_buffers[5];
static unsigned stopped_adcs;
static uint32_t clock_ms,primask;
uint32_t HAL_GetTick(void) { return clock_ms; }
uint32_t __get_PRIMASK(void) { return primask; }
void __disable_irq(void) { primask=1; }
void __set_PRIMASK(uint32_t value) { primask=value; }
bool Motor_IsEnabled(motor_id_t id) { (void)id; return false; }
HAL_StatusTypeDef HAL_ADC_Init(ADC_HandleTypeDef *h) { (void)h; return ++init_count==fail_init?HAL_ERROR:HAL_OK; }
HAL_StatusTypeDef HAL_DMA_Init(DMA_HandleTypeDef *h) { (void)h; return HAL_OK; }
HAL_StatusTypeDef HAL_ADCEx_Calibration_Start(ADC_HandleTypeDef *h,uint32_t mode) { (void)h; (void)mode; return ++cal_count==fail_cal?HAL_ERROR:HAL_OK; }
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
{ (void)h; return ++amps==fail_amp?HAL_ERROR:HAL_OK; }
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
    dma_buffers[index]=(uint16_t *)b;
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
    if(argc>1 && argv[1][0]=='a') fail_amp=atoi(argv[1]+1);
    if(argc>1 && argv[1][0]=='c') fail_cal=atoi(argv[1]+1);
    if(argc>1 && argv[1][0]=='i') fail_init=atoi(argv[1]+1);
#if N_PULSE_ENABLE_CURRENT_ACQUISITION
    if (fail || fail_amp || fail_cal || fail_init) {
        assert(!ADC_DMA_BSP_Init());
        assert(stops >= 11 && stopped_adcs == 31U);
        for (unsigned i = 0; i < MOTOR_COUNT; ++i) {
            assert(ADC_DMA_BSP_GetRaw((motor_id_t)i) == 0);
            assert(isnan(ADC_DMA_BSP_GetCurrentA((motor_id_t)i)));
        }
        printf("ADC injected failure %s: all five DMA streams and six OPAMPs cleaned, current unavailable OK\n",argv[1]);
        return 0;
    }
    assert(ADC_DMA_BSP_Init() && starts == 5 && amps == 6);
    adc_current_sample_t sample;
    assert(ADC_DMA_BSP_GetSample(MOTOR_INDEX,&sample) && !sample.raw_valid && sample.active);
    HAL_ADC_ConvCpltCallback(&hadc1); HAL_ADC_ConvCpltCallback(&hadc2);
    HAL_ADC_ConvCpltCallback(&hadc3); HAL_ADC_ConvCpltCallback(&hadc4); HAL_ADC_ConvCpltCallback(&hadc5);
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
        assert(!(dma[i].interrupts & DMA_IT_HT));
        assert(dma[i].interrupts & DMA_IT_TC);
        assert(dma[i].interrupts & DMA_IT_TE);
    }
    for (unsigned i = 0; i < 6; ++i) assert(enabled[i]);
    assert(ADC_DMA_BSP_GetSample(MOTOR_INDEX,&sample) && sample.state==ADC_CURRENT_RAW_VALID && isnan(sample.voltage_v));
    assert(!ADC_DMA_BSP_SetVref(NAN) && ADC_DMA_BSP_SetVref(3.3f));
    adc_current_calibration_t cal={.offset_v=0.05f,.shunt_ohm=.1f,.analog_gain=2,.scale=1,.polarity=1};
    assert(ADC_DMA_BSP_SetCalibration(MOTOR_INDEX,&cal));
    assert(ADC_DMA_BSP_GetSample(MOTOR_INDEX,&sample) && sample.state==ADC_CURRENT_CALIBRATED);
    assert(fabsf(sample.current_a-(100*3.3f/4095-.05f)/.2f)<1e-6f);
    cal.offset_v=sample.voltage_v;
    assert(ADC_DMA_BSP_SetCalibration(MOTOR_INDEX,&cal));
    assert(ADC_DMA_BSP_GetSample(MOTOR_INDEX,&sample) && sample.state==ADC_CURRENT_CALIBRATED && sample.current_a==0);
    /* A valid zero-current measurement needs completed non-rail data + cal. */
    cal.shunt_ohm=0; assert(!ADC_DMA_BSP_SetCalibration(MOTOR_INDEX,&cal));
    cal.shunt_ohm=.1f; cal.offset_v=4; assert(!ADC_DMA_BSP_SetCalibration(MOTOR_INDEX,&cal));
    clock_ms=20; ADC_DMA_BSP_Service(); dma_buffers[0][0]=0;
    HAL_ADC_ConvCpltCallback(&hadc1);
    assert(ADC_DMA_BSP_GetSample(MOTOR_INDEX,&sample) && sample.state==ADC_CURRENT_SATURATED && isnan(sample.current_a));
    assert(sample.voltage_v==0);
    dma_buffers[1][0]=4095; HAL_ADC_ConvCpltCallback(&hadc2);
    assert(ADC_DMA_BSP_GetSample(MOTOR_MIDDLE,&sample) && sample.state==ADC_CURRENT_SATURATED);
    clock_ms=101;
    clock_ms=121;
    assert(ADC_DMA_BSP_GetSample(MOTOR_INDEX,&sample) && sample.state==ADC_CURRENT_STALE && isnan(sample.current_a));
    hadc3.ErrorCode=4; HAL_ADC_ErrorCallback(&hadc3);
    assert(ADC_DMA_BSP_GetSample(MOTOR_THUMB,&sample) && sample.state==ADC_CURRENT_ERROR);
    ADC_DMA_BSP_Service();
    assert(ADC_DMA_BSP_GetSample(MOTOR_INDEX,&sample) && !sample.active);
    assert(ADC_DMA_BSP_Init()); clock_ms+=20; ADC_DMA_BSP_Service();
    assert(ADC_DMA_BSP_GetSample(MOTOR_INDEX,&sample) && sample.state==ADC_CURRENT_ERROR && (sample.error&ADC_SAMPLE_TIMEOUT));
    puts("Finite ADC scans: completed-only freshness, raw/calibrated/stale/error, coefficients, Curr5=ADC3/PE13, battery excluded OK");
#else
    assert(ADC_DMA_BSP_Init() && starts == 0 && amps == 0);
    for (unsigned i = 0; i < MOTOR_COUNT; ++i) assert(ADC_DMA_BSP_GetRaw((motor_id_t)i) == 0);
    puts("Default ADC acquisition: no hardware starts, no valid samples OK");
#endif
    for (unsigned i = 0; i < MOTOR_COUNT; ++i) assert(isnan(ADC_DMA_BSP_GetCurrentA((motor_id_t)i)));
    assert(ADC_DMA_BSP_GetRaw(MOTOR_COUNT) == 0 && isnan(ADC_DMA_BSP_GetCurrentA(MOTOR_COUNT)));
    return 0;
}
