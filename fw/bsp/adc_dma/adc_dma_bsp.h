#ifndef ADC_DMA_BSP_H
#define ADC_DMA_BSP_H

#include <stdint.h>
#include "motor_control.h"

/* Experimental raw acquisition only. Disabled by default for startup bring-up.
 * No calibrated current, force conversion or validated current protection. */
#ifndef N_PULSE_ENABLE_CURRENT_ACQUISITION
#define N_PULSE_ENABLE_CURRENT_ACQUISITION 0
#endif
#if N_PULSE_ENABLE_CURRENT_ACQUISITION != 0 && N_PULSE_ENABLE_CURRENT_ACQUISITION != 1
#error "N_PULSE_ENABLE_CURRENT_ACQUISITION must be 0 or 1"
#endif

bool     ADC_DMA_BSP_Init(void);
uint16_t ADC_DMA_BSP_GetRaw(motor_id_t id);
/* Returns NAN until a calibrated conversion exists, including acquisition off. */
float    ADC_DMA_BSP_GetCurrentA(motor_id_t id);

#endif /* ADC_DMA_BSP_H */
