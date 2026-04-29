#ifndef INTENT_ROUTER_H
#define INTENT_ROUTER_H

#include <stdint.h>

// We adapt to use 'selected_mode' values as our intent IDs
typedef enum {
    MODE_OPEN   = 0,
    MODE_CLOSE  = 1,
    MODE_PINCH  = 2,
    MODE_WRIST_R = 3,
    MODE_WRIST_L = 4
} intent_id_t;

typedef struct {
    intent_id_t id;
} intent_t;

#ifdef __cplusplus
extern "C" {
#endif

void IntentRouter_Handle(const intent_t* intent);

#ifdef __cplusplus
}
#endif

#endif