#ifndef INTENT_CONFIG_H
#define INTENT_CONFIG_H
/* A different intent received before this interval expires is discarded. */
#ifndef INTENT_MIN_RUN_MS
#define INTENT_MIN_RUN_MS 50U
#endif
#if INTENT_MIN_RUN_MS >= 0x80000000U
#error Invalid intent protection interval
#endif
#endif
