#include "adc_dma_bsp.h"
#include "main.h"
#include <math.h>

extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;
extern ADC_HandleTypeDef hadc3;
extern ADC_HandleTypeDef hadc4;
extern ADC_HandleTypeDef hadc5;
extern OPAMP_HandleTypeDef hopamp1;
extern OPAMP_HandleTypeDef hopamp2;
extern OPAMP_HandleTypeDef hopamp3;
extern OPAMP_HandleTypeDef hopamp4;
extern OPAMP_HandleTypeDef hopamp5;
extern OPAMP_HandleTypeDef hopamp6;

static volatile uint16_t s_adc1[1];
static volatile uint16_t s_adc2[2];
static volatile uint16_t s_adc3[1];
static volatile uint16_t s_adc4[3];
static volatile uint16_t s_adc5[2];
static bool s_started;

#if N_PULSE_ENABLE_CURRENT_ACQUISITION
static HAL_StatusTypeDef start_raw_dma(ADC_HandleTypeDef *adc,
                                       volatile uint16_t *buffer,
                                       uint32_t length, IRQn_Type irq)
{
    /* HAL enables HT/TC before starting the ADC. Mask the IRQ during this
     * window, then retain only DMA error interrupts. Tiny circular buffers
     * must not generate an interrupt on every sample. Raw values are polled. */
    uint32_t enabled = NVIC_GetEnableIRQ(irq);
    HAL_NVIC_DisableIRQ(irq);
    HAL_StatusTypeDef status = HAL_ADC_Start_DMA(adc, (uint32_t *)buffer, length);
    __HAL_DMA_DISABLE_IT(adc->DMA_Handle, DMA_IT_HT | DMA_IT_TC);
    HAL_NVIC_ClearPendingIRQ(irq);
    if (enabled) HAL_NVIC_EnableIRQ(irq);
    return status;
}
#endif

bool ADC_DMA_BSP_Init(void)
{
    s_started = false;
#if N_PULSE_ENABLE_CURRENT_ACQUISITION
    /* CubeMX's channel/rank configuration is authoritative:
     * ADC1: Curr1
     * ADC2 ranks 1..2: Curr2, Curr3
     * ADC5 ranks 1..2: Curr4, Curr7
     * ADC3 rank 1: Curr5, PE13/ADC3_IN3
     * ADC4 ranks 1..3: Curr6, Batt_lvl (PE15/ADC4_IN2), Curr8.
     * Keep the battery rank in the DMA scan, but never use it as motor current. */
    if (HAL_OPAMP_Start(&hopamp1) != HAL_OK ||
        HAL_OPAMP_Start(&hopamp2) != HAL_OK ||
        HAL_OPAMP_Start(&hopamp3) != HAL_OK ||
        HAL_OPAMP_Start(&hopamp4) != HAL_OK ||
        HAL_OPAMP_Start(&hopamp5) != HAL_OK ||
        HAL_OPAMP_Start(&hopamp6) != HAL_OK)
    {
        s_started = false;
        goto failed;
    }

    if (start_raw_dma(&hadc1, s_adc1, 1U, DMA1_Channel8_IRQn) != HAL_OK ||
        start_raw_dma(&hadc2, s_adc2, 2U, DMA1_Channel2_IRQn) != HAL_OK ||
        start_raw_dma(&hadc3, s_adc3, 1U, DMA1_Channel3_IRQn) != HAL_OK ||
        start_raw_dma(&hadc4, s_adc4, 3U, DMA1_Channel4_IRQn) != HAL_OK ||
        start_raw_dma(&hadc5, s_adc5, 2U, DMA1_Channel5_IRQn) != HAL_OK)
    {
        s_started = false;
        goto failed;
    }

    s_started = true;
    return true;

failed:
    /* Stop any conversions/amplifiers started before the failure. */
    (void)HAL_ADC_Stop_DMA(&hadc1);
    (void)HAL_ADC_Stop_DMA(&hadc2);
    (void)HAL_ADC_Stop_DMA(&hadc3);
    (void)HAL_ADC_Stop_DMA(&hadc4);
    (void)HAL_ADC_Stop_DMA(&hadc5);
    (void)HAL_OPAMP_Stop(&hopamp1);
    (void)HAL_OPAMP_Stop(&hopamp2);
    (void)HAL_OPAMP_Stop(&hopamp3);
    (void)HAL_OPAMP_Stop(&hopamp4);
    (void)HAL_OPAMP_Stop(&hopamp5);
    (void)HAL_OPAMP_Stop(&hopamp6);
    return false;
#else
    /* Success means optional acquisition was skipped, not that data is valid. */
    return true;
#endif
}

uint16_t ADC_DMA_BSP_GetRaw(motor_id_t id)
{
    if (!s_started || (uint32_t)id >= MOTOR_COUNT)
    {
        return 0U;
    }

    switch (id)
    {
    case MOTOR_INDEX:   return s_adc1[0]; /* Curr1, PA3/OPAMP1 */
    case MOTOR_MIDDLE:  return s_adc2[0]; /* Curr2, PA7/OPAMP2 */
    case MOTOR_RING:    return s_adc2[1]; /* Curr3, PB0/OPAMP3 */
    case MOTOR_LITTLE:  return s_adc5[0]; /* Curr4, PD11/OPAMP4 */
    case MOTOR_THUMB:   return s_adc3[0]; /* Curr5, PE13/ADC3_IN3 */
    case MOTOR_PALM:    return s_adc4[0]; /* Curr6, PE14 */
    case MOTOR_WRIST_X: return s_adc5[1]; /* Curr7, PC3/OPAMP5 */
    case MOTOR_WRIST_Y: return s_adc4[2]; /* Curr8, PD9/OPAMP6 */
    default:            return 0U;
    }
}

float ADC_DMA_BSP_GetCurrentA(motor_id_t id)
{
    (void)id;
    /* The RC-filtered ISEN voltage is not a calibrated instantaneous winding
     * current measurement. No conversion or threshold is applied here. */
    return NAN; /* Unavailable, not a measured zero current. */
}
