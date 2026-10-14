#ifndef ADC_CURRENT_CONFIG_H
#define ADC_CURRENT_CONFIG_H
/* Fill only from measured VDDA, populated board/shunt and calibrated transfer.
 * Zero entries intentionally fail validation. analog_gain is TOTAL voltage
 * gain from shunt to ADC, including external stages and OPAMP PGA; scale is
 * the measured correction factor. Factory OPAMP trim and .ioc PGA retained. */
#ifndef N_PULSE_ADC_VREF_V
#define N_PULSE_ADC_VREF_V 0.0f
#endif
static const adc_current_calibration_t g_adc_current_calibration[MOTOR_COUNT] = {
    [0] = {0}
};
#endif
