#ifndef COMM_H
#define COMM_H

#include <stdint.h>
#include <stdbool.h>
#include "intent_router.h" 

void Comms_Init(void);
bool Comms_GetNextIntent(intent_t *out_intent);

// Add the prototype for the bridge function we built for rx.cpp
void enqueue_intent_from_proto(intent_id_t id); 

#endif