#include "intent_router.h"
#include "C:\Users\decju\OneDrive\Bureau\N-Pulse_STM32G474QET6\ARM-SOFT-Firmware\fw\motors\motor_control\motor_control.h"

void IntentRouter_Handle(const intent_t *intent)
{
    switch (intent->id) {
    case INTENT_OPEN_HAND:
        Motor_SetAllTargets(OPEN_POSITION, intent->strength);
        break;

    case INTENT_CLOSE_HAND:
        Motor_SetAllTargets(CLOSED_POSITION, intent->strength);
        break;

    case INTENT_PINCH:
        Motor_SetTarget(MOTOR_THUMB, PINCH_POS_THUMB, intent->strength);
        Motor_SetTarget(MOTOR_INDEX, PINCH_POS_INDEX, intent->strength);
        break;

    case INTENT_STOP:
        Motor_StopAll();
        break;

    default:
        // ignore / log
        break;
    }
}
