#include "board_link.h"
#include "comms_session.h"
#include "motor_safety.h"
#include "intent_router.h"
#include <assert.h>
#include <stdio.h>
static uint32_t now=100,epoch=19,masked;
static bool master=true,allowed=true;
static unsigned routed; static unsigned motors[8];
uint32_t HAL_GetTick(void) { return now; }
uint32_t __get_PRIMASK(void) { return masked; }
void __disable_irq(void) { masked=1; }
void __set_PRIMASK(uint32_t x) { masked=x; }
bool BoardLink_HostAuthority(void) { return master; }
uint32_t MotorSafety_EpochLocked(void) { return epoch; }
bool MotorSafety_CanArmLocked(void) { return allowed; }
void MotorSafety_RequestStopCauseLocked(motor_stop_cause_t cause,motor_id_t id)
{ (void)cause;(void)id; allowed=false; ++epoch; }
void Motor_StopAll(void) { allowed=false; ++epoch; }
bool BoardLink_RouteTarget(motor_id_t id,float target,uint8_t speed,uint32_t e)
{
    assert(master && (unsigned)id<8 && isfinite(target) && speed==100 && e==epoch);
    ++routed; ++motors[id]; return allowed;
}
int main(void)
{
    for(unsigned role=0;role<2;++role) {
        master=role==0; CommsSession_Init(true);
        rx_result_t hello={.type=RX_TYPE_HELLO,.proto_version=1,.data.device_id=7}; bool started;
        assert(CommsSession_OnMessage(&hello,now,epoch,&started) && started);
        rx_result_t action={.type=RX_TYPE_ACTION,.data.action_id=ACTION_CLOSE_HAND};
        assert(CommsSession_OnMessage(&action,now,epoch,NULL)==master);
        intent_t intent;
        if(master) {
            assert(CommsSession_GetIntent(&intent)); IntentRouter_Handle(&intent); assert(routed==8);
            for(unsigned i=0;i<8;++i) assert(motors[i]==1);
            intent.id=ACTION_ROTATE_WRIST_R; IntentRouter_Handle(&intent); assert(routed==10 && motors[5]==2 && motors[6]==2);
            action.data.action_id=ACTION_OPEN_HAND; assert(CommsSession_OnMessage(&action,now,epoch,NULL));
            Motor_StopAll(); CommsSession_OnStopLocked(); assert(!CommsSession_GetIntent(&intent));
            allowed=true;
        } else assert(!CommsSession_GetIntent(&intent));
    }
    puts("PASS real host session -> MASTER intent routing, eight IDs/wrist pair, STOP epoch invalidation, SLAVE diagnostic-only authority; exit=0");
    return 0;
}
