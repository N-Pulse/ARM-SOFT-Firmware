#include "status_leds.h"
#include "status_leds_config.h"
#include "comms_session.h"
#include "haptics.h"
#include "motor_lock.h"
#include "controller_status.h"
#include "battery_config.h"

static status_leds_snapshot_t display;
static uint32_t last_update_ms;
__attribute__((weak)) void StatusLeds_BoardInit(void) { }
__attribute__((weak)) void StatusLeds_BoardService(void) { }
__attribute__((weak)) void StatusLeds_BoardWrite(status_led_id_t led,status_led_color_t color) {
    (void)led; (void)color; /* Disconnected fallback for SIM/host fixtures. */
}
void StatusLeds_Init(void) {
    StatusLeds_BoardInit();
    uint32_t key=Motor_Lock(),now=HAL_GetTick();
    display=(status_leds_snapshot_t){.timestamp_ms=now}; /* All three start RED. */
    last_update_ms=now-STATUS_LEDS_UPDATE_PERIOD_MS;
    Motor_Unlock(key);
    for(unsigned i=0;i<STATUS_LED_COUNT;++i)
        StatusLeds_BoardWrite((status_led_id_t)i,STATUS_LED_RED);
}
void StatusLeds_GetSnapshot(status_leds_snapshot_t *out) {
    if(!out) return;
    uint32_t key=Motor_Lock(); *out=display; Motor_Unlock(key);
}
void StatusLeds_Service(void) {
    StatusLeds_BoardService();
    uint32_t now=HAL_GetTick();
    if((uint32_t)(now-last_update_ms)<STATUS_LEDS_UPDATE_PERIOD_MS) return;
    last_update_ms=now;
    status_led_color_t next[STATUS_LED_COUNT]={STATUS_LED_RED,STATUS_LED_RED,STATUS_LED_RED};
    comms_peer_status_t decoding; CommsSession_GetPeerStatus(&decoding);
    if(decoding.valid && decoding.state==STATUS_LEDS_DECODING_NORMAL_VALUE)
        next[STATUS_LED_DECODING]=STATUS_LED_GREEN;
    haptics_status_t haptics; Haptics_GetStatus(&haptics);
    if(Haptics_GetState()==HAPTICS_STREAMING && haptics.valid &&
       (uint32_t)(now-haptics.timestamp_ms)<STATUS_LEDS_HAPTICS_TIMEOUT_MS &&
       haptics.state==STATUS_LEDS_HAPTICS_NORMAL_VALUE)
        next[STATUS_LED_HAPTICS]=STATUS_LED_GREEN;
    if(ControllerStatus_Read()==0) next[STATUS_LED_CONTROLLER]=STATUS_LED_GREEN;
    battery_snapshot_t battery; Battery_GetSnapshot(&battery);
    if(battery.fast_warning) {
        now=HAL_GetTick();
        uint32_t origin=battery.shutdown_state==BATTERY_SHUTDOWN_COUNTDOWN ? battery.shutdown_started_ms : 0U;
        next[STATUS_LED_CONTROLLER]=((uint32_t)(now-origin)/BATTERY_FAST_BLINK_HALF_PERIOD_MS)&1U ?
            STATUS_LED_OFF : STATUS_LED_RED;
    }
    uint32_t changed=0,key=Motor_Lock();
    for(unsigned i=0;i<STATUS_LED_COUNT;++i) {
        if(display.color[i]!=next[i]) changed|=1U<<i;
        display.color[i]=next[i];
    }
    display.timestamp_ms=now; Motor_Unlock(key);
    for(unsigned i=0;i<STATUS_LED_COUNT;++i)
        if(changed&(1U<<i)) StatusLeds_BoardWrite((status_led_id_t)i,next[i]);
    StatusLeds_BoardService();
}
