#include "app.h"
#include "motor_control.h"
#include "comm.h"
#include "intent_router.h"
#include "motor_estimation.h"

void App_Init(void)
{
    Motor_InitAll();
    MotorEstimation_Init();
    Comms_Init(); /* Buffers/objects before the tasks start UART4 IT. */
}
void App_Task(void)
{
    intent_t intent;
    if (Comms_GetNextIntent(&intent)) IntentRouter_Handle(&intent);
}
