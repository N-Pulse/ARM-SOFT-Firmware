#ifndef STATUS_LEDS_H
#define STATUS_LEDS_H
#include <stdint.h>
#include "../../app/board_hardware_config.h"
#if BOARD_LED_ARM_INDEX > 2U || BOARD_LED_HAPTICS_INDEX > 2U || BOARD_LED_EMG_INDEX > 2U || BOARD_LED_ARM_INDEX == BOARD_LED_HAPTICS_INDEX || BOARD_LED_ARM_INDEX == BOARD_LED_EMG_INDEX || BOARD_LED_HAPTICS_INDEX == BOARD_LED_EMG_INDEX
#error LED chain indices must be a permutation of 0, 1, 2
#endif
/* IDs are physical zero-based positions in the LED1 -> LED2 -> LED3 chain. */
typedef enum { STATUS_LED_CONTROLLER = BOARD_LED_ARM_INDEX,
    STATUS_LED_HAPTICS = BOARD_LED_HAPTICS_INDEX,
    STATUS_LED_DECODING = BOARD_LED_EMG_INDEX, STATUS_LED_COUNT = 3 } status_led_id_t;
typedef enum { STATUS_LED_RED, STATUS_LED_GREEN, STATUS_LED_OFF } status_led_color_t;
typedef struct {
    status_led_color_t color[STATUS_LED_COUNT];
    uint32_t timestamp_ms;
} status_leds_snapshot_t;
void StatusLeds_Init(void);
void StatusLeds_Service(void);
void StatusLeds_GetSnapshot(status_leds_snapshot_t *out);
/* Physical PB10/USART3 DMA driver in status_leds_board.c. Weak disconnected
 * fallbacks remain for SIM/host fixtures. Write/Service run in the same task
 * (or startup), outside Motor_Lock. OFF is required for critical-battery blink. */
void StatusLeds_BoardInit(void);
void StatusLeds_BoardWrite(status_led_id_t led,status_led_color_t color);
void StatusLeds_BoardService(void);
#endif
