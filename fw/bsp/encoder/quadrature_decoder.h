#ifndef QUADRATURE_DECODER_H
#define QUADRATURE_DECODER_H
#include <stdint.h>
#include <stdbool.h>
typedef struct {
    int64_t count;
    uint32_t edges, invalid;
    uint8_t state;
    bool fault;
} quadrature_decoder_t;
/* A is bit 1, B bit 0; pending uses the same two-bit representation.
 * Caller serializes IRQ/read/reset. A missed edge makes position unknown. */
void Quadrature_Init(quadrature_decoder_t *decoder, uint8_t ab);
bool Quadrature_Edge(quadrature_decoder_t *decoder, uint8_t ab, uint8_t pending);
#endif
