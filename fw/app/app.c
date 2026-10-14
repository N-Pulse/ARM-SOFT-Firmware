#include "app.h"
#include "motor_control.h"
#include "comm.h"
#include "intent_router.h"
#include "motor_estimation.h"
#include "board_link.h"

void App_Init(void)
{
    Motor_InitAll();
    MotorEstimation_Init();
    Comms_Init(); /* Buffers/objects before the tasks start UART4 IT. */
#if N_PULSE_DUAL_BOARD_LINK && !defined(MOTOR_BACKEND_SIM)
    BoardLink_RuntimeInit();
#endif
}
void App_Task(void)
{
    intent_t intent;
    if (Comms_GetNextIntent(&intent)) IntentRouter_Handle(&intent);
}
