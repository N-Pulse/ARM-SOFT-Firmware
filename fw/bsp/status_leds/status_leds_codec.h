#ifndef STATUS_LEDS_CODEC_H
#define STATUS_LEDS_CODEC_H
#include "status_leds.h"
#define STATUS_LEDS_WIRE_BYTES (STATUS_LED_COUNT * 12U)
/* Inverted UART 4 Mbaud 8N1: two LED bits per UART byte. */
void StatusLeds_Encode(const status_led_color_t colors[STATUS_LED_COUNT],
                       uint8_t out[STATUS_LEDS_WIRE_BYTES]);
#endif
