#include "comms_parser.h"
#include "handshake_protocol.h"
#include "data_protocol.h"
#include <string.h>

void CommsParser_Init(comms_parser_t *p) { memset(p, 0, sizeof(*p)); }
void CommsParser_Reset(comms_parser_t *p) { p->used = 0U; }
void CommsParser_Poll(comms_parser_t *p, uint32_t now)
{
    if (p->used && ((uint32_t)(now - p->last_ms) > COMMS_INTERBYTE_TIMEOUT_MS ||
                    (uint32_t)(now - p->first_ms) > COMMS_FRAME_TIMEOUT_MS)) {
        ++p->timeouts;
        CommsParser_Reset(p);
    }
}
static void consume(comms_parser_t *p, size_t n)
{
    p->used -= n;
    memmove(p->bytes, p->bytes + n, p->used);
    memmove(p->byte_ms, p->byte_ms + n, p->used * sizeof(p->byte_ms[0]));
    if (p->used) p->first_ms = p->byte_ms[0];
}
void CommsParser_Feed(comms_parser_t *p, uint8_t byte, uint32_t ms,
                      uint32_t epoch, comms_frame_fn callback, void *context)
{
    CommsParser_Poll(p, ms);
    if (p->used && epoch != p->epoch) CommsParser_Reset(p);
    if (!p->used) { p->first_ms = ms; p->epoch = epoch; }
    p->last_ms = ms;
    /* The loop below leaves at most an incomplete bounded candidate. */
    if (p->used >= sizeof(p->bytes)) CommsParser_Reset(p);
    p->byte_ms[p->used] = ms;
    p->bytes[p->used++] = byte;
    while (p->used) {
        if(p->bytes[0]==DATA_PACKET_TYPE) {
            if(p->used<3U) break;
            size_t length=p->bytes[2];
            if(length<2U || length>COMMS_PAYLOAD_MAX+2U) {
                ++p->invalid_lengths; consume(p,1U); continue;
            }
            if(p->used<length+3U) break;
            const uint8_t *data; size_t data_length;
            if(Data_Decode(p->bytes,length+3U,&data,&data_length)) {
                callback(p->bytes,length+3U,p->last_ms,p->epoch,context);
                consume(p,length+3U);
            } else { ++p->invalid_frames; consume(p,1U); }
            continue;
        }
        if(p->bytes[0]==HANDSHAKE_HELLO) {
            if(p->used<3U) break;
            if(p->bytes[2]!=6U) { ++p->invalid_lengths; consume(p,1U); continue; }
            if(p->used<HANDSHAKE_HELLO_SIZE) break;
            bool matches;
            if(Handshake_DecodeHello(p->bytes,HANDSHAKE_HELLO_SIZE,&matches)) {
                callback(p->bytes,HANDSHAKE_HELLO_SIZE,p->last_ms,p->epoch,context);
                consume(p,HANDSHAKE_HELLO_SIZE);
            } else { ++p->invalid_frames; consume(p,1U); }
            continue;
        }
        if (p->bytes[0] != 0xAAU) { consume(p, 1U); continue; }
        if (p->used < 2U) break;
        size_t length = p->bytes[1];
        if (!length || length > COMMS_PAYLOAD_MAX) {
            ++p->invalid_lengths; consume(p, 1U); continue;
        }
        if (p->used < length + 2U) break;
        if (callback(p->bytes + 2U, length, p->last_ms, p->epoch, context))
            consume(p, length + 2U);
        else {
            ++p->invalid_frames;
            consume(p, 1U); /* Preserve a following AA already in the candidate. */
        }
    }
}
