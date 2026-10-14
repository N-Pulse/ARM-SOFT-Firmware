#ifndef ADC_DMA_BSP_H
#define ADC_DMA_BSP_H

#include <stdint.h>
#include "motor_control.h"

/* Optional finite DMA current acquisition, disabled by default. Calibration
 * is explicit; this API is not a validated electrical protection system. */
#ifndef N_PULSE_ENABLE_CURRENT_ACQUISITION
#define N_PULSE_ENABLE_CURRENT_ACQUISITION 0
#endif
#if N_PULSE_ENABLE_CURRENT_ACQUISITION != 0 && N_PULSE_ENABLE_CURRENT_ACQUISITION != 1
#error "N_PULSE_ENABLE_CURRENT_ACQUISITION must be 0 or 1"
#endif

bool     ADC_DMA_BSP_Init(void);
/* Task context outside Motor_Lock: finite DMA scans at 50 Hz, TC timestamps. */
void ADC_DMA_BSP_Service(void);
void ADC_DMA_BSP_Stop(void);
#define ADC_DMA_PERIOD_MS 20U
#define ADC_DMA_STALE_MS 100U
enum { ADC_SAMPLE_HAL_ERROR=1U<<24, ADC_SAMPLE_TIMEOUT=1U<<25, ADC_SAMPLE_COEFFICIENT=1U<<26 };
typedef enum { ADC_CURRENT_INVALID, ADC_CURRENT_RAW_VALID, ADC_CURRENT_CALIBRATED,
    ADC_CURRENT_STALE, ADC_CURRENT_ERROR, ADC_CURRENT_SATURATED } adc_current_state_t;
typedef struct {
    uint16_t raw;
    float voltage_v, current_a;
    bool active, raw_valid;
    adc_current_state_t state;
    uint32_t timestamp_ms, age_ms, sequence, error;
} adc_current_sample_t;
typedef struct { float offset_v, shunt_ohm, analog_gain, scale; int8_t polarity; } adc_current_calibration_t;
bool ADC_DMA_BSP_SetVref(float volts);
bool ADC_DMA_BSP_SetCalibration(motor_id_t id,const adc_current_calibration_t *calibration);
bool ADC_DMA_BSP_GetSample(motor_id_t id,adc_current_sample_t *out);
uint16_t ADC_DMA_BSP_GetRaw(motor_id_t id);
/* Returns NAN unless a fresh, unsaturated calibrated sample exists. */
float    ADC_DMA_BSP_GetCurrentA(motor_id_t id);

#endif /* ADC_DMA_BSP_H */
