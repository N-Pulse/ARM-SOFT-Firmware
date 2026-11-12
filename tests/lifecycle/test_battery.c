#define TEST_STREAMING_FIXTURE
#include "test_handshake.c"
#include "battery.h"
#include "battery_config.h"
static unsigned warning_on,warning_off;
static bool board_ready;
static float board_voltage;
static uint32_t board_ms;
void App_OnBatteryWarning(bool active,const battery_snapshot_t *s) {
    assert(s->low_warning==active);
    if(active) ++warning_on; else ++warning_off;
}
bool Battery_ReadPackVoltage(float *v,uint32_t *ms) {
    if(!board_ready) return false;
    *v=board_voltage; *ms=board_ms; return true;
}
static battery_snapshot_t snapshot(void) {
    battery_snapshot_t s; Battery_GetSnapshot(&s); return s;
}
static void publish(float v,unsigned elapsed) {
    clock_ms+=elapsed; assert(Battery_PublishVoltage(v,clock_ms));
    Battery_Service(); App_Task();
}
int main(void) {
    reply_case(4); warning_on=warning_off=0;
    assert(snapshot().state==BATTERY_UNKNOWN && !snapshot().voltage_valid && isnan(snapshot().pack_voltage_v));
#ifndef TEST_BATTERY_CONFIGURED
    assert(!Battery_PublishRaw(2000,clock_ms));
    publish(14.8f,1); assert(snapshot().state==BATTERY_UNCONFIGURED);
    publish(10.0f,1000); publish(10.0f,1000);
    assert(!snapshot().low_warning && warning_on==0 && !MotorSafety_IsFaultActive());
    puts("PASS production defaults never guess ADC divider, warning threshold or charge percentage; explicit voltage stays available as UNCONFIGURED");
#else
    uint32_t original_epoch=epoch();
    publish(14.8f,1); assert(snapshot().state==BATTERY_NORMAL);
    publish(13.0f,100); publish(14.0f,100); /* Brief motor-load sag. */
    assert(!snapshot().low_warning && warning_on==0);
    publish(BATTERY_LOW_WARNING_V,100);
    for(unsigned i=0;i<9;++i) publish(BATTERY_LOW_WARNING_V,100);
    assert(!snapshot().low_warning);
    publish(BATTERY_LOW_WARNING_V,100); assert(snapshot().state==BATTERY_LOW && warning_on==1);
    App_Task(); assert(warning_on==1);
    assert(CommsSession_GetState()==APP_WORK && Motor_IsEnabled(MOTOR_INDEX) && epoch()==original_epoch);
    puts("PASS sustained low voltage raises one warning, transient sag does not; warning does not stop motors or change the work/handshake state");
    publish(13.4f,100); assert(snapshot().low_warning);
    publish(BATTERY_WARNING_CLEAR_V,100);
    for(unsigned i=0;i<9;++i) publish(BATTERY_WARNING_CLEAR_V,100);
    assert(snapshot().low_warning);
    publish(BATTERY_WARNING_CLEAR_V,100); assert(!snapshot().low_warning && warning_off==1);
    puts("PASS separate recovery threshold and sustained-clear period prevent warning chatter; clear interface called once");
    publish(13.0f,100); uint32_t stamp=snapshot().timestamp_ms;
    for(unsigned i=0;i<11;++i) {
        clock_ms+=100; assert(!Battery_PublishVoltage(13.0f,stamp)); Battery_Service(); App_Task();
    }
    assert(snapshot().state==BATTERY_STALE && !snapshot().voltage_valid && !snapshot().low_warning);
    assert(warning_on==1);
    publish(13.0f,1); assert(!snapshot().low_warning);
    for(unsigned i=0;i<10;++i) publish(13.0f,100);
    assert(snapshot().low_warning && warning_on==2);
    clock_ms+=BATTERY_SAMPLE_TIMEOUT_MS+1; Battery_Service(); App_Task();
    assert(snapshot().state==BATTERY_STALE && snapshot().low_warning);
    assert(warning_off==1);
    puts("PASS duplicate/stale samples cannot confirm LOW; fresh streak resets after a sample gap; lost feedback cannot silently clear a low warning");
    clock_ms+=1; assert(!Battery_PublishVoltage(NAN,clock_ms));
    assert(snapshot().state==BATTERY_INVALID && snapshot().low_warning && !MotorSafety_IsFaultActive());
    assert(!Battery_PublishVoltage(14.8f,clock_ms+1));
    assert(!Battery_PublishVoltage(14.8f,clock_ms-1));
    clock_ms+=1; assert(Battery_PublishRaw(3000,clock_ms));
    assert(fabsf(snapshot().pack_voltage_v-3000.0f*3.3f/4095.0f*6.0f)<0.00001f);
    clock_ms+=1; assert(!Battery_PublishRaw(4095,clock_ms)); assert(snapshot().state==BATTERY_INVALID);
    puts("PASS raw-to-pack conversion uses explicit Vref/divider; invalid/future/old/saturated samples do not fabricate battery level");
    Battery_Init(); board_ready=true; board_voltage=14.7f; board_ms=clock_ms;
    Battery_Service(); assert(snapshot().voltage_valid && snapshot().pack_voltage_v==14.7f);
    uint32_t sequence=snapshot().sequence;
    clock_ms+=BATTERY_READ_PERIOD_MS; Battery_Service(); assert(snapshot().sequence==sequence);
    board_ready=false;
    Battery_Init(); clock_ms=UINT32_MAX-500U;
    assert(Battery_PublishVoltage(13.0f,clock_ms));
    for(unsigned i=0;i<10;++i) { clock_ms+=100; assert(Battery_PublishVoltage(13.0f,clock_ms)); }
    assert(snapshot().low_warning);
    puts("PASS board reader hook and HAL tick-wrap timing");
#endif
    return 0;
}
