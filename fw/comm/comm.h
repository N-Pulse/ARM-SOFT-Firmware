#include <stdbool.h>
#include "intent_router.h"

void Comms_Init(void);
bool Comms_GetNextIntent(intent_t *out_intent);