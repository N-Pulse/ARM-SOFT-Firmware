#ifndef ADC_DMA_BSP_H
#define ADC_DMA_BSP_H

#include <stdint.h>
#include "motor_control.h"

// ADC BSP — current sensing via shunt resistors, one channel per motor.
//
// TODO: implement adc_dma_bsp.c once current-sense schematic is confirmed.
// Call ADC_DMA_BSP_Init() from MotorBackend_Init() (motor_backend_hw.c).
// On DMA complete ISR: call MotorBackend_OnFeedback() with measured force_mN.

void     ADC_DMA_BSP_Init(void);
uint16_t ADC_DMA_BSP_GetRaw(motor_id_t id);
float    ADC_DMA_BSP_GetCurrentA(motor_id_t id);

#endif /* ADC_DMA_BSP_H */
