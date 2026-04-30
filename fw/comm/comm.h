#ifndef COMM_H
#define COMM_H

#include <stdint.h>
#include <stdbool.h>
#include "intent_router.h"

void Comms_Init(void);
void Comms_TaskCreate(void);
bool Comms_GetNextIntent(intent_t *out_intent);

// Called by the comm-stack (rx.cpp) when a SelectMode proto message is decoded.
void enqueue_intent_from_proto(intent_id_t id);

#endif
