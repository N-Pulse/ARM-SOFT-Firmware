#ifndef INTENT_ROUTER_H
#define INTENT_ROUTER_H

#include <stdint.h>

/**
 * @brief Intent IDs mapped to the Protobuf Action enumeration.
 */
typedef enum {
    ACTION_OPEN_HAND      = 0,
    ACTION_CLOSE_HAND     = 1,
    ACTION_PINCH          = 2,
    ACTION_ROTATE_WRIST_R = 3,
    ACTION_ROTATE_WRIST_L = 4,
    ACTION_THUMB_OPEN     = 5,
    ACTION_THUMB_CLOSE    = 6,
    ACTION_INDEX_OPEN     = 7,
    ACTION_INDEX_CLOSE    = 8,
    ACTION_MIDDLE_OPEN    = 9,
    ACTION_MIDDLE_CLOSE   = 10,
    ACTION_RING_OPEN      = 11,
    ACTION_RING_CLOSE     = 12,
    ACTION_LITTLE_OPEN    = 13,
    ACTION_LITTLE_CLOSE   = 14,
    ACTION_WRIST_PALM     = 15,
    ACTION_WRIST_DORSI    = 16,
    ACTION_UNKNOWN        = 99 // Used for safety stops/timeouts
} intent_id_t;

/**
 * @brief Command stored in the bounded interlock-protected intent ring.
 */
typedef struct {
    intent_id_t id;
    uint32_t epoch; /* Stop/recovery generation at reception, never restamped. */
} intent_t;

/**
 * @brief Dispatches the intent to the corresponding motor drivers.
 */
void IntentRouter_Handle(const intent_t* intent);

#endif /* INTENT_ROUTER_H */
