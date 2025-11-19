#ifndef INTENT_ROUTER_H
#define INTENT_ROUTER_H

#include <stdint.h>

typedef enum {
    INTENT_OPEN_HAND,
    INTENT_CLOSE_HAND,
    INTENT_PINCH,
    INTENT_POINT,
    INTENT_STOP,
} intent_id_t;

typedef struct {
    intent_id_t id;
    uint8_t     strength;   // 0–100 %
} intent_t;

void IntentRouter_Handle(const intent_t *intent);

#endif // INTENT_ROUTER_H
