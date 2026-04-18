#ifndef COMM_H
#define COMM_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    INTENT_STOP = 0,
    INTENT_OPEN_HAND = 1,
    INTENT_CLOSE_HAND = 2,
    INTENT_PINCH = 3,
    INTENT_POINT = 4,
    INTENT_DIRECT_CONTROL = 5 
} intent_id_t;

typedef struct {
    intent_id_t id;
    uint8_t     strength; 
    int16_t     positions[8]; 
} intent_t;

void Comms_Init(void);
bool Comms_GetNextIntent(intent_t *out_intent);

#endif