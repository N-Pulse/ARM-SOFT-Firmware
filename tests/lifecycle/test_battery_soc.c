#define TEST_STREAMING_FIXTURE
#include "test_handshake.c"
#include "battery_config.h"
#include "controller_status.h"
#include "status_leds.h"
#include "status_leds_codec.h"
#include "status_leds_config.h"

static unsigned power_requests;
static bool accept_poweroff;
static bool soc_source_ready;
bool Battery_ReadStateOfCharge(float *percent,uint32_t *ms) {
    if(!soc_source_ready) return false;
    soc_source_ready=false; clock_ms+=5;
    *percent=9.0f; *ms=clock_ms; return true;
}
static status_led_color_t colors[STATUS_LED_COUNT];
void StatusLeds_BoardWrite(status_led_id_t id,status_led_color_t color) {
    assert(!primask); colors[id]=color;
}
bool Battery_BoardRequestPowerOff(void) {
    assert(!primask && MotorSafety_StopComplete());
    for(unsigned i=0;i<MOTOR_COUNT;++i) assert(!Motor_IsEnabled((motor_id_t)i));
    ++power_requests; return accept_poweroff;
}

static battery_snapshot_t snapshot(void) {
    battery_snapshot_t s; Battery_GetSnapshot(&s); return s;
}
static void input_case(void) {
    reply_case(4);
    assert(!snapshot().soc_valid && isnan(snapshot().state_of_charge_percent));
    assert(Battery_PublishStateOfCharge(20.0f,clock_ms));
    assert(snapshot().state==BATTERY_NORMAL && !snapshot().low_warning);
    assert(ControllerStatus_Read()==0); /* SOC is sufficient without volts. */
    assert(!Battery_PublishStateOfCharge(19.0f,clock_ms));
    assert(!Battery_PublishStateOfCharge(19.0f,clock_ms+1));
    ++clock_ms; assert(Battery_PublishStateOfCharge(19.9f,clock_ms));
    assert(snapshot().low_warning && snapshot().warning_revision==1);
    uint32_t epoch_before=epoch();
    App_Task(); assert(!MotorSafety_IsFaultActive() && epoch()==epoch_before);
    clock_ms+=BATTERY_SOC_SAMPLE_TIMEOUT_MS+1;
    assert(!snapshot().soc_valid && snapshot().state==BATTERY_STALE && snapshot().low_warning);
    ++clock_ms; assert(!Battery_PublishStateOfCharge(NAN,clock_ms));
    assert(snapshot().state==BATTERY_INVALID && snapshot().low_warning);
    ++clock_ms; assert(!Battery_PublishStateOfCharge(101.0f,clock_ms));
    ++clock_ms; assert(!Battery_PublishStateOfCharge(-1.0f,clock_ms));
    ++clock_ms; assert(Battery_PublishStateOfCharge(25.0f,clock_ms));
    assert(snapshot().state==BATTERY_NORMAL && !snapshot().low_warning && snapshot().warning_revision==2);
    puts("PASS actual SOC input: strict 20-percent boundary, timestamp/range checks, stale warning retention and recovery");
}
static void countdown_case(bool wrap,bool accepted) {
    reply_case(4); power_requests=0; accept_poweroff=accepted;
    if(wrap) clock_ms=UINT32_MAX-15000U;
    assert(Battery_PublishStateOfCharge(10.0f,clock_ms));
    assert(snapshot().shutdown_state==BATTERY_SHUTDOWN_IDLE);
    ++clock_ms; assert(Battery_PublishStateOfCharge(9.9f,clock_ms));
    uint32_t origin=clock_ms;
    StatusLeds_Service(); assert(colors[STATUS_LED_CONTROLLER]==STATUS_LED_RED);
    clock_ms=origin+100U; StatusLeds_Service();
    assert(colors[STATUS_LED_CONTROLLER]==STATUS_LED_OFF && !MotorSafety_IsFaultActive());
    assert(Battery_PublishStateOfCharge(9.0f,clock_ms));
    assert(snapshot().shutdown_started_ms==origin);
    clock_ms=origin+200U; assert(Battery_PublishStateOfCharge(80.0f,clock_ms));
    StatusLeds_Service(); assert(colors[STATUS_LED_CONTROLLER]==STATUS_LED_RED);
    assert(snapshot().shutdown_state==BATTERY_SHUTDOWN_COUNTDOWN && snapshot().low_warning);
    clock_ms=origin+29999U; Battery_Service();
    assert(snapshot().shutdown_state==BATTERY_SHUTDOWN_COUNTDOWN && !MotorSafety_IsFaultActive());
    assert(!snapshot().soc_valid && power_requests==0); /* Stale cannot cancel. */
    clock_ms=origin+30000U; Battery_Service(); App_Task();
    assert(snapshot().shutdown_state==BATTERY_SHUTDOWN_STOPPED && !snapshot().fast_warning);
    assert(MotorSafety_GetStopCauses()&MOTOR_STOP_BATTERY);
    assert(CommsSession_GetState()==APP_FAULT && power_requests==1);
    assert(snapshot().power_off_accepted==accepted && !Motor_RecoverFromFault());
    for(unsigned i=0;i<MOTOR_COUNT;++i) assert(!Motor_Enable((motor_id_t)i));
    for(unsigned i=0;i<5;++i) { ++clock_ms; Battery_Service(); }
    assert(power_requests==1);
    puts("PASS critical SOC: red/off 5 Hz, one 30-second deadline, stale/recovered SOC cannot extend it, all-motor stop before optional power-off, no recovery; including tick wrap");
}
static void codec_case(void) {
    status_led_color_t c[3]={STATUS_LED_RED,STATUS_LED_GREEN,STATUS_LED_OFF};
    uint8_t wire[STATUS_LEDS_WIRE_BYTES]; StatusLeds_Encode(c,wire);
    const uint8_t expected[9]={0,STATUS_LEDS_BRIGHTNESS,0,STATUS_LEDS_BRIGHTNESS,0,0,0,0,0};
    for(unsigned i=0;i<STATUS_LEDS_WIRE_BYTES;++i) {
        unsigned cells[10]={1,0,0,0,0,0,0,0,0,0};
        for(unsigned j=0;j<8;++j) cells[j+1]=!((wire[i]>>j)&1U);
        for(unsigned k=0;k<2;++k) {
            unsigned bit=(expected[i/4]>>(7U-((i%4U)*2U+k)))&1U;
            for(unsigned j=0;j<5;++j) assert(cells[k*5U+j]==(j<(bit ? 4U : 1U)));
        }
    }
    puts("PASS actual inverted UART start/data/stop cells encode 24-bit GRB MSB first and OFF within Dialight pulse-width limits");
}
static void reader_case(void) {
    reply_case(4); soc_source_ready=true;
    Battery_Service();
    assert(snapshot().shutdown_state==BATTERY_SHUTDOWN_COUNTDOWN && !MotorSafety_IsFaultActive());
    assert(snapshot().shutdown_started_ms==clock_ms);
    puts("PASS SOC reader clock advance cannot underflow the shutdown timer into an immediate stop");
}
int main(void) { input_case(); countdown_case(false,false); countdown_case(true,true); reader_case(); codec_case(); return 0; }
