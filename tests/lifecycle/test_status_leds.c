#define TEST_STREAMING_FIXTURE
#include "test_handshake.c"
#include "status_leds.h"
#include "status_leds_config.h"
#include "haptics.h"
#include "battery.h"
#include "battery_config.h"

static status_led_color_t physical[STATUS_LED_COUNT];
static unsigned writes[STATUS_LED_COUNT],board_inits;
void StatusLeds_BoardInit(void) { assert(!primask); ++board_inits; }
void StatusLeds_BoardWrite(status_led_id_t id,status_led_color_t color) {
    assert(!primask && (unsigned)id<STATUS_LED_COUNT);
    assert(color==STATUS_LED_RED || color==STATUS_LED_GREEN);
    physical[id]=color; ++writes[id];
}
static status_led_color_t color(status_led_id_t id) {
    status_leds_snapshot_t s; StatusLeds_GetSnapshot(&s); assert(s.color[id]==physical[id]); return s.color[id];
}
static void show(void) { clock_ms+=STATUS_LEDS_UPDATE_PERIOD_MS; StatusLeds_Service(); }
static void decoder(uint8_t state,bool corrupt,bool invalid_intent) {
    uint8_t f[]={0x10,44,10,state,0x5a,0x12,4,0x0a,2,8,1,0,0};
    if(invalid_intent) f[10]=99;
    uint16_t crc=Handshake_Crc16(f,sizeof(f)-2); f[11]=(uint8_t)crc; f[12]=(uint8_t)(crc>>8);
    if(corrupt) f[12]^=1;
    inject(f,sizeof(f)); drain(); tx_all();
}
static uint8_t haptic_tx[HAPTICS_FRAME_MAX];
static bool haptic_send(const uint8_t *f,size_t n,void *context) {
    (void)context; memcpy(haptic_tx,f,n); return true;
}
static void haptic_feed(const uint8_t *f,size_t n) {
    for(size_t i=0;i<n;++i) Haptics_FeedByte(f[i],clock_ms);
}
static void haptic_connect(void) {
    uint8_t sync[5]; Handshake_EncodeSync(sync,sizeof(sync));
    haptic_feed(sync,sizeof(sync)); Haptics_Service(clock_ms,haptic_send,NULL);
    uint8_t reply[]={2,haptic_tx[1],6,HAPTICS_PROTO_VER,HAPTICS_STATUS_LEN,HAPTICS_DATA_LEN,HAPTICS_HB_PERIOD_MS,0,0};
    uint16_t crc=Handshake_Crc16(reply,7); reply[7]=(uint8_t)crc; reply[8]=(uint8_t)(crc>>8);
    haptic_feed(reply,sizeof(reply)); assert(Haptics_GetState()==HAPTICS_STREAMING);
}
static void haptic_status(uint8_t state,bool corrupt) {
    uint8_t f[]={0x10,55,4,state,0x5a,0,0};
    uint16_t crc=Handshake_Crc16(f,5); f[5]=(uint8_t)crc; f[6]=(uint8_t)(crc>>8);
    if(corrupt) f[6]^=1;
    haptic_feed(f,sizeof(f));
}
int main(void) {
    assert(STATUS_LED_CONTROLLER==0 && STATUS_LED_HAPTICS==1 && STATUS_LED_DECODING==2);
    boot_to_handshake();
    assert(board_inits && color(STATUS_LED_DECODING)==STATUS_LED_RED &&
        color(STATUS_LED_CONTROLLER)==STATUS_LED_RED && color(STATUS_LED_HAPTICS)==STATUS_LED_RED);
    uint8_t h[9]; make_hello(h,1,4); inject(h,sizeof(h)); drain(); tx_all(); show();
    assert(color(STATUS_LED_DECODING)==STATUS_LED_RED); /* Handshake is not remote status. */
    assert(color(STATUS_LED_CONTROLLER)==STATUS_LED_RED); /* Battery unknown. */
    puts("PASS three independent LEDs default RED; handshake without status and missing battery remain RED; board hooks run outside motor lock");

    decoder(0,false,false); show(); assert(color(STATUS_LED_DECODING)==STATUS_LED_GREEN);
    comms_peer_status_t peer; CommsSession_GetPeerStatus(&peer);
    assert(peer.valid && peer.length==2 && peer.bytes[1]==0x5a && peer.sequence==44);
    decoder(9,false,false); show(); assert(color(STATUS_LED_DECODING)==STATUS_LED_RED);
    decoder(0,false,false); show(); assert(color(STATUS_LED_DECODING)==STATUS_LED_GREEN);
    decoder(0,false,false); decoder(1,true,false); show(); assert(color(STATUS_LED_DECODING)==STATUS_LED_GREEN);
    decoder(0,false,true); show(); assert(color(STATUS_LED_DECODING)==STATUS_LED_GREEN);
    puts("PASS decoding Status is CRC-checked and retained independently from intent; 0 GREEN/nonzero RED, corrupt packet ignored");

    clock_ms+=HANDSHAKE_HB_PERIOD_MS; show(); assert(color(STATUS_LED_DECODING)==STATUS_LED_RED);
    uint32_t key=Motor_Lock(); CommsSession_PollLocked(clock_ms); Motor_Unlock(key);
    drain(); tx_all(); inject(h,sizeof(h)); drain(); tx_all(); show();
    assert(color(STATUS_LED_DECODING)==STATUS_LED_RED);
    decoder(0,false,false); show(); assert(color(STATUS_LED_DECODING)==STATUS_LED_GREEN);
    puts("PASS decoding timeout/reconnect cannot reuse previous-session GREEN status");

    haptic_connect(); show(); assert(color(STATUS_LED_HAPTICS)==STATUS_LED_RED);
    haptic_status(0,false); show(); assert(color(STATUS_LED_HAPTICS)==STATUS_LED_GREEN);
    haptic_status(2,false); show(); assert(color(STATUS_LED_HAPTICS)==STATUS_LED_RED);
    haptic_status(0,false); show(); assert(color(STATUS_LED_HAPTICS)==STATUS_LED_GREEN);
    haptic_status(1,true); show(); assert(color(STATUS_LED_HAPTICS)==STATUS_LED_GREEN);
    clock_ms+=STATUS_LEDS_HAPTICS_TIMEOUT_MS; show(); assert(color(STATUS_LED_HAPTICS)==STATUS_LED_RED);
    haptic_status(0,false); show(); assert(color(STATUS_LED_HAPTICS)==STATUS_LED_GREEN);
    uint8_t sync[5]; Handshake_EncodeSync(sync,sizeof(sync)); haptic_feed(sync,sizeof(sync));
    show(); assert(color(STATUS_LED_HAPTICS)==STATUS_LED_RED);
    puts("PASS haptics 0/nonzero, CRC rejection, status timeout and renewed SYNC select LED2 independently");

    assert(Battery_PublishVoltage(14.8f,clock_ms)); show();
    assert(color(STATUS_LED_CONTROLLER)==STATUS_LED_GREEN);
    for(unsigned i=0;i<11;++i) { clock_ms+=100; assert(Battery_PublishVoltage(13.0f,clock_ms)); StatusLeds_Service(); }
    assert(color(STATUS_LED_CONTROLLER)==STATUS_LED_RED && !MotorSafety_IsFaultActive());
    for(unsigned i=0;i<11;++i) { clock_ms+=100; assert(Battery_PublishVoltage(14.8f,clock_ms)); StatusLeds_Service(); }
    assert(color(STATUS_LED_CONTROLLER)==STATUS_LED_GREEN);
    clock_ms+=BATTERY_SAMPLE_TIMEOUT_MS+1; show(); assert(color(STATUS_LED_CONTROLLER)==STATUS_LED_RED);
    assert(Battery_PublishVoltage(14.8f,clock_ms)); show(); assert(color(STATUS_LED_CONTROLLER)==STATUS_LED_GREEN);
    clock_ms+=1; assert(!Battery_PublishVoltage(NAN,clock_ms)); show(); assert(color(STATUS_LED_CONTROLLER)==STATUS_LED_RED);
    clock_ms+=1; assert(Battery_PublishVoltage(14.8f,clock_ms)); show();
    MotorSafety_RequestStopCause(MOTOR_STOP_FORCE,MOTOR_INDEX); show();
    assert(color(STATUS_LED_CONTROLLER)==STATUS_LED_RED);
    assert(!Motor_IsEnabled(MOTOR_INDEX));
    puts("PASS controller LED follows battery NORMAL/LOW/recovery/stale/invalid and local safety fault, without adding motor control behavior");

    unsigned old_writes[3]; memcpy(old_writes,writes,sizeof(writes));
    StatusLeds_Service(); StatusLeds_Service();
    assert(!memcmp(old_writes,writes,sizeof(writes)));
    show(); assert(!memcmp(old_writes,writes,sizeof(writes)));
    puts("PASS periodic updates write only color changes; no blink, blocking wait, or redundant GPIO writes");
    return 0;
}
