#ifndef BATTERY_CONFIG_H
#define BATTERY_CONFIG_H
/* Pack identification from the supplied label; not a calibrated SoC curve. */
#define BATTERY_SERIES_CELLS 4U
#define BATTERY_NOMINAL_VOLTAGE_V 14.8f
#define BATTERY_NOMINAL_CAPACITY_MAH 3500U
/* Set from measured VDDA and the populated resistor divider.
 * pack_voltage = ADC_voltage * DIVIDER_RATIO, ratio=(Rtop+Rbottom)/Rbottom.
 * Zero means unconfigured. No guessed pack voltage is published. */
#ifndef BATTERY_ADC_VREF_V
#define BATTERY_ADC_VREF_V 0.0f
#endif
#ifndef BATTERY_DIVIDER_RATIO
#define BATTERY_DIVIDER_RATIO 0.0f
#endif
#define BATTERY_ADC_FULL_SCALE 4095U
/* Whole-pack thresholds, to be agreed. Zero disables voltage-based warning.
 * Clear must be greater than Low to give recovery hysteresis. */
#ifndef BATTERY_LOW_WARNING_V
#define BATTERY_LOW_WARNING_V 0.0f
#endif
#ifndef BATTERY_WARNING_CLEAR_V
#define BATTERY_WARNING_CLEAR_V 0.0f
#endif
#ifndef BATTERY_LOW_CONFIRM_MS
#define BATTERY_LOW_CONFIRM_MS 1000U
#endif
#ifndef BATTERY_CLEAR_CONFIRM_MS
#define BATTERY_CLEAR_CONFIRM_MS 1000U
#endif
#ifndef BATTERY_SAMPLE_TIMEOUT_MS
#define BATTERY_SAMPLE_TIMEOUT_MS 1000U
#endif
#define BATTERY_READ_PERIOD_MS 100U
#if BATTERY_SAMPLE_TIMEOUT_MS == 0 || BATTERY_SAMPLE_TIMEOUT_MS >= 0x80000000U || BATTERY_LOW_CONFIRM_MS >= 0x80000000U || BATTERY_CLEAR_CONFIRM_MS >= 0x80000000U
#error Invalid battery time bounds
#endif
#endif
