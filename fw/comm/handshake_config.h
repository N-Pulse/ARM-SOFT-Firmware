#ifndef HANDSHAKE_CONFIG_H
#define HANDSHAKE_CONFIG_H

/* PLACEHOLDERS: agree these four one-byte values with the host before use.
 * They describe the expected HELLO; received values never replace them. */
#ifndef HANDSHAKE_PROTO_VER
#define HANDSHAKE_PROTO_VER 1U
#endif
#ifndef HANDSHAKE_STATUS_LEN
#define HANDSHAKE_STATUS_LEN 0U
#endif
#ifndef HANDSHAKE_DATA_LEN
#define HANDSHAKE_DATA_LEN 0U
#endif
#ifndef HANDSHAKE_HB_PERIOD_MS
#define HANDSHAKE_HB_PERIOD_MS 100U
#endif

#ifndef HANDSHAKE_SYNC_PERIOD_MS
#define HANDSHAKE_SYNC_PERIOD_MS 500U
#endif

#if HANDSHAKE_PROTO_VER > 255U || HANDSHAKE_STATUS_LEN > 255U || HANDSHAKE_DATA_LEN > 255U || HANDSHAKE_HB_PERIOD_MS > 255U
#error HELLO parameters must fit in one byte
#endif
#if HANDSHAKE_HB_PERIOD_MS == 0
#error Heartbeat period must be nonzero
#endif
#if HANDSHAKE_SYNC_PERIOD_MS == 0 || HANDSHAKE_SYNC_PERIOD_MS >= 0x80000000U
#error Invalid SYNC period
#endif
#endif
