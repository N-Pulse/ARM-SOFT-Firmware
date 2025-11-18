#include "comm.h"

void Comms_Init(void)
{
    // plus tard: init UART / CAN / BLE / etc.
}

bool Comms_GetNextIntent(intent_t *out_intent)
{
    (void)out_intent;
    // pour l’instant: pas d’intents reçus
    return false;
}
