#ifndef INTENT_ROUTER_H
#define INTENT_ROUTER_H

#include <stdint.h>

/**
 * @brief Intent IDs mapped to the Protobuf Action enumeration.
 */
/* Presentation build : MAIN seule. Les actions poignet (ROTATE_WRIST_R/L,
 * valeurs 3/4 dans le proto) restent reçues mais sont ignorées (default →
 * Motor_StopAll par sécurité). Voir branche `simulation_pipeline` pour le
 * poignet. */
typedef enum {
    ACTION_OPEN_HAND      = 0,
    ACTION_CLOSE_HAND     = 1,
    ACTION_PINCH          = 2,
    ACTION_UNKNOWN        = 99 // Used for safety stops/timeouts
} intent_id_t;

/**
 * @brief The data structure sent through the FreeRTOS queue.
 */
typedef struct {
    intent_id_t id;
} intent_t;

/**
 * @brief Dispatches the intent to the corresponding motor drivers.
 */
void IntentRouter_Handle(const intent_t* intent);

#endif /* INTENT_ROUTER_H */