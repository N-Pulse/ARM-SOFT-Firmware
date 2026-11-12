#ifndef HAPTICS_CONFIG_H
#define HAPTICS_CONFIG_H
/* Provisional wire parameters; independent of the decoding/host link.
 * Existing HELLO has one byte per field, including integer milliseconds.
 * Agreed provisional period: use 7 ms for the requested ~7.5 ms. */
#ifndef HAPTICS_PROTO_VER
#define HAPTICS_PROTO_VER 1U
#endif
#ifndef HAPTICS_STATUS_LEN
#define HAPTICS_STATUS_LEN 2U
#endif
#ifndef HAPTICS_DATA_LEN
#define HAPTICS_DATA_LEN 44U
#endif
#ifndef HAPTICS_HB_PERIOD_MS
#define HAPTICS_HB_PERIOD_MS 7U
#endif
#define HAPTICS_HELLO_RETRY_MS 500U
#define HAPTICS_RX_TIMEOUT_MS 100U
/* Temporary payload: 8 motor angles in motor_id_t order (rad), then thumb,
 * index and middle sensor voltages (V), all LE IEEE754 float32.
 * Outbound Status begins with controller state (0 healthy/nonzero fault).
 * Optional byte 1 is the low validity bitmap; byte 2 contains the high bits.
 * NaN payload fields indicate unavailable readings even with shorter Status.
 * Change the codec after the final haptics DATA/status schema is agreed. */
#define HAPTICS_TELEMETRY_FLOATS 11U
#define HAPTICS_TELEMETRY_BYTES 44U
#define HAPTICS_FRAME_MAX 258U
#if HAPTICS_PROTO_VER > 255U || HAPTICS_STATUS_LEN > 255U || HAPTICS_DATA_LEN > 255U || HAPTICS_HB_PERIOD_MS == 0 || HAPTICS_HB_PERIOD_MS > 255U || HAPTICS_STATUS_LEN + HAPTICS_DATA_LEN + 2U > 255U
#error Invalid initial haptics wire parameters
#endif
#endif
