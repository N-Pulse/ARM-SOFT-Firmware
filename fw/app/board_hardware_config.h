#ifndef BOARD_HARDWARE_CONFIG_H
#define BOARD_HARDWARE_CONFIG_H
/* Confirmed board: ARM-ELEC-main/CTRL_BOARD + power_board_lou + SHOWBATTERY.
 * Nominal filtered 3.3 V VDDA/VREF; replace with a measured value if needed. */
#ifndef BOARD_ADC_VREF_V
#define BOARD_ADC_VREF_V 3.3f
#endif
#define BOARD_BATTERY_DIVIDER_TOP_OHM 10000.0f
#define BOARD_BATTERY_DIVIDER_BOTTOM_OHM 2200.0f

/* Zero-based daisy-chain positions: LED1 Arm, LED2 Haptics, LED3 EMG.
 * PB10 -> R39 47 ohm -> control J8.2 -> status J2.1 (DIN).
 * These define roles, not the LED supply voltage or cable pinout. */
#define BOARD_LED_ARM_INDEX 0U
#define BOARD_LED_HAPTICS_INDEX 1U
#define BOARD_LED_EMG_INDEX 2U

/* Assembly convention: wire thumb/index/middle to J18 pins 1/2/3.
 * Haptic_in1 PA2 ADC1_IN3; Haptic_in2 PA5 ADC2_IN13;
 * Haptic_in3 PC5 ADC2_IN11. ADC numbers are peripheral IDs, not DMA ranks.
 * These force channels are separate from the current/OPAMP scan. */
#define BOARD_FORCE_THUMB_INPUT 1U
#define BOARD_FORCE_THUMB_ADC 1U
#define BOARD_FORCE_THUMB_CHANNEL 3U
#define BOARD_FORCE_INDEX_INPUT 2U
#define BOARD_FORCE_INDEX_ADC 2U
#define BOARD_FORCE_INDEX_CHANNEL 13U
#define BOARD_FORCE_MIDDLE_INPUT 3U
#define BOARD_FORCE_MIDDLE_ADC 2U
#define BOARD_FORCE_MIDDLE_CHANNEL 11U
#endif
