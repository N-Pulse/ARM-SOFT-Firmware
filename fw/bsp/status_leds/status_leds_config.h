#ifndef STATUS_LEDS_CONFIG_H
#define STATUS_LEDS_CONFIG_H
#ifndef STATUS_LEDS_BRIGHTNESS
#define STATUS_LEDS_BRIGHTNESS 32U
#endif
#if STATUS_LEDS_BRIGHTNESS == 0 || STATUS_LEDS_BRIGHTNESS > 255U
#error Invalid LED brightness
#endif
/* Provisional remote status encoding: first Status byte, 0 normal/nonzero fault. */
#ifndef STATUS_LEDS_DECODING_NORMAL_VALUE
#define STATUS_LEDS_DECODING_NORMAL_VALUE 0U
#endif
#ifndef STATUS_LEDS_HAPTICS_NORMAL_VALUE
#define STATUS_LEDS_HAPTICS_NORMAL_VALUE 0U
#endif
/* Haptics RX-status period is not defined yet. This timeout is independent of
 * our negotiated 7 ms outbound telemetry period; adjust with the final protocol. */
#ifndef STATUS_LEDS_HAPTICS_TIMEOUT_MS
#define STATUS_LEDS_HAPTICS_TIMEOUT_MS 1000U
#endif
#ifndef STATUS_LEDS_UPDATE_PERIOD_MS
#define STATUS_LEDS_UPDATE_PERIOD_MS 50U
#endif
#if STATUS_LEDS_DECODING_NORMAL_VALUE > 255U || STATUS_LEDS_HAPTICS_NORMAL_VALUE > 255U || STATUS_LEDS_HAPTICS_TIMEOUT_MS == 0 || STATUS_LEDS_HAPTICS_TIMEOUT_MS >= 0x80000000U || STATUS_LEDS_UPDATE_PERIOD_MS == 0 || STATUS_LEDS_UPDATE_PERIOD_MS >= 0x80000000U
#error Invalid status LED configuration
#endif
#endif
