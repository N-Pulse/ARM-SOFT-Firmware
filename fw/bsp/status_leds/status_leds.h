#ifndef STATUS_LEDS_H
#define STATUS_LEDS_H
#include <stdint.h>
typedef enum { STATUS_LED_DECODING, STATUS_LED_CONTROLLER, STATUS_LED_HAPTICS,
    STATUS_LED_COUNT } status_led_id_t;
typedef enum { STATUS_LED_RED, STATUS_LED_GREEN } status_led_color_t;
typedef struct {
    status_led_color_t color[STATUS_LED_COUNT];
    uint32_t timestamp_ms;
} status_leds_snapshot_t;
void StatusLeds_Init(void);
void StatusLeds_Service(void);
void StatusLeds_GetSnapshot(status_leds_snapshot_t *out);
/* Weak board interfaces. Fill the actual red/green pins, polarity or LED-driver
 * commands here when known; no GPIO/UART is claimed by the default hooks.
 * Write is called in task/startup context outside Motor_Lock, on color changes. */
void StatusLeds_BoardInit(void);
void StatusLeds_BoardWrite(status_led_id_t led,status_led_color_t color);
#endif
