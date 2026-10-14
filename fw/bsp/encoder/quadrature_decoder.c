#include "quadrature_decoder.h"
#include <limits.h>
void Quadrature_Init(quadrature_decoder_t *d, uint8_t ab)
{
    *d = (quadrature_decoder_t){.state = ab & 3U, .fault = ab > 3U};
}
bool Quadrature_Edge(quadrature_decoder_t *d, uint8_t ab, uint8_t pending)
{
    static const int8_t step[16] = {0,1,-1,0, -1,0,0,1, 1,0,0,-1, 0,-1,1,0};
    if (d->edges != UINT32_MAX) ++d->edges;
    uint8_t changed = d->state ^ ab;
    if (ab > 3U || (pending != 1U && pending != 2U) || changed != pending) {
        if (d->invalid != UINT32_MAX) ++d->invalid;
        d->fault = true; /* Both pending, diagonal or repeated state: lost ordering. */
    }
    int direction = ab <= 3U ? step[(d->state << 2) | ab] : 0;
    d->state = ab & 3U;
    if (!d->fault) {
        if ((direction > 0 && d->count == INT64_MAX) ||
            (direction < 0 && d->count == INT64_MIN)) d->fault = true;
        else d->count += direction;
    }
    return !d->fault;
}
