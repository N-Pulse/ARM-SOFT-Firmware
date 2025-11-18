#include "app.h"
#include "motor_control.h"
#include "comms.h"
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

    Motor_Update();  // si tu as besoin de faire avancer les rampes, PID, etc.
}
