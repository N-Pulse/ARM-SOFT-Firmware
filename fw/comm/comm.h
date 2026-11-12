#ifndef COMM_H
#define COMM_H

#include <stdint.h>
#include <stdbool.h>
#include "intent_router.h"
#include "FreeRTOS.h"
#include "task.h"

void Comms_Init(void);
void Comms_TaskCreate(void);

/* One bounded task iteration, production entry also used by host tests. */
void Comms_ProcessRxOnce(TickType_t wait);

#endif
