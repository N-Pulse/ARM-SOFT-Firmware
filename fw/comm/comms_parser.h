#ifndef COMMS_PARSER_H
#define COMMS_PARSER_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "comms_config.h"
/* Callback runs in task context. True means a valid wire message (even when
 * the application rejects it); false requests bounded suffix resynchronisation. */
typedef bool (*comms_frame_fn)(const uint8_t *, size_t, uint32_t, uint32_t, void *);
typedef struct {
    uint8_t bytes[COMMS_PAYLOAD_MAX + 2U];
    uint32_t byte_ms[COMMS_PAYLOAD_MAX + 2U];
    size_t used;
    uint32_t first_ms, last_ms, epoch;
    uint32_t invalid_lengths, invalid_frames, timeouts;
} comms_parser_t;
void CommsParser_Init(comms_parser_t *p);
void CommsParser_Reset(comms_parser_t *p);
void CommsParser_Poll(comms_parser_t *p, uint32_t now);
void CommsParser_Feed(comms_parser_t *p, uint8_t byte, uint32_t ms,
                      uint32_t epoch, comms_frame_fn callback, void *context);
#endif
