#ifndef BATTERY_CONFIG_H
#define BATTERY_CONFIG_H
#include "../../app/board_hardware_config.h"
/* Current user-specified Keeppower 4S1P 4000mAh pack; not a SoC curve. */
#define BATTERY_SERIES_CELLS 4U
#define BATTERY_NOMINAL_VOLTAGE_V 14.8f
#define BATTERY_NOMINAL_CAPACITY_MAH 4000U
/* Final lou board: R21=10k top, R22=2.2k bottom; nominal VREF=3.3 V.
 * pack_voltage = ADC_voltage * DIVIDER_RATIO, ratio=(Rtop+Rbottom)/Rbottom.
 * Zero means unconfigured. No guessed pack voltage is published. */
#ifndef BATTERY_ADC_VREF_V
#define BATTERY_ADC_VREF_V BOARD_ADC_VREF_V
#endif
#ifndef BATTERY_DIVIDER_RATIO
#define BATTERY_DIVIDER_RATIO ((BOARD_BATTERY_DIVIDER_TOP_OHM + BOARD_BATTERY_DIVIDER_BOTTOM_OHM) / BOARD_BATTERY_DIVIDER_BOTTOM_OHM)
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
/* State-of-charge input is independent of the voltage ADC. */
#define BATTERY_SOC_LOW_PERCENT 20U
#define BATTERY_SOC_SAMPLE_TIMEOUT_MS 1000U
/* Strictly below 20% warns; strictly below 10% latches a 30-second shutdown.
 * Zero can disable critical shutdown in an explicitly overridden build. */
#ifndef BATTERY_SOC_CRITICAL_PERCENT
#define BATTERY_SOC_CRITICAL_PERCENT 10U
#endif
#define BATTERY_SHUTDOWN_DELAY_MS 30000U
#define BATTERY_FAST_BLINK_HALF_PERIOD_MS 100U
#if BATTERY_SOC_CRITICAL_PERCENT >= BATTERY_SOC_LOW_PERCENT
#error Invalid state-of-charge warning thresholds
#endif
#if BATTERY_SAMPLE_TIMEOUT_MS == 0 || BATTERY_SAMPLE_TIMEOUT_MS >= 0x80000000U || BATTERY_LOW_CONFIRM_MS >= 0x80000000U || BATTERY_CLEAR_CONFIRM_MS >= 0x80000000U
#error Invalid battery time bounds
#endif
#endif
