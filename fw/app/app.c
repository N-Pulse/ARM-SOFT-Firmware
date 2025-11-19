#include "app.h"
#include "motor_control.h"
#include "comm.h"
#include "intent_router.h"

void App_Init(void)
{
   Motor_InitAll();
   Comms_Init();
}

void App_Task(void)
{
    intent_t intent;
    if (Comms_GetNextIntent(&intent)) {
        IntentRouter_Handle(&intent);
    }
}
