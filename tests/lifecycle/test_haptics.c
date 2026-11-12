#define TEST_STREAMING_FIXTURE
#include "test_handshake.c"
#include "haptics.h"
#include "force_sensor.h"
#include "controller_status.h"

static uint8_t sent[HAPTICS_FRAME_MAX];
static size_t sent_length;
static unsigned sends;
static bool busy;
static bool transmit(const uint8_t *f,size_t n,void *context) {
    (void)context;
    if(busy) return false;
    memcpy(sent,f,n); sent_length=n; ++sends; return true;
}
static bool source_ready;
bool ForceSensor_ReadVoltage(motor_id_t id,float *voltage,uint32_t *ms) {
    if(!source_ready || id!=MOTOR_THUMB) return false;
    source_ready=false; *voltage=0.04f; *ms=clock_ms; return true;
}
static void feed(const uint8_t *f,size_t n,uint32_t now) {
    for(size_t i=0;i<n;++i) Haptics_FeedByte(f[i],now);
}
static void sync_at(uint32_t now) {
    uint8_t f[5]; Handshake_EncodeSync(f,sizeof(f)); feed(f,sizeof(f),now);
}
static void reply(uint8_t type,haptics_parameters_t p,uint32_t now) {
    uint8_t f[]={type,sent[1],6,p.proto_ver,p.status_len,p.data_len,p.hb_period_ms,0,0};
    uint16_t crc=Handshake_Crc16(f,7); f[7]=(uint8_t)crc; f[8]=(uint8_t)(crc>>8);
    feed(f,sizeof(f),now);
}
static float read_float(const uint8_t *p) {
    uint32_t bits=(uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
    float f; memcpy(&f,&bits,4); return f;
}
int main(void) {
    reply_case(4); /* Decoding handshake has already entered WORK. */
    assert(CommsSession_GetState()==APP_WORK);
    uint32_t motor_epoch=epoch();
    for(unsigned i=0;i<MOTOR_COUNT;++i) positions[i]=(float)i/10.0f;
    assert(ForceSensor_PublishVoltage(MOTOR_THUMB,0.01f,clock_ms));
    assert(ForceSensor_PublishVoltage(MOTOR_INDEX,0.02f,clock_ms));
    assert(ForceSensor_PublishVoltage(MOTOR_MIDDLE,0.03f,clock_ms));
    Haptics_Init(); sends=0; busy=false;
    Haptics_Service(0,transmit,NULL); assert(sends==0);
    sync_at(10); Haptics_Service(10,transmit,NULL);
    assert(sent_length==9 && sent[0]==1 && sent[2]==6);
    assert(Haptics_GetState()==HAPTICS_WAIT_REPLY);
    assert(epoch()==motor_epoch && CommsSession_GetState()==APP_WORK);
    haptics_parameters_t p; Haptics_GetParameters(&p);
    reply(2,p,20); assert(Haptics_GetState()==HAPTICS_STREAMING);
    unsigned before=sends; uint32_t due=20+(uint32_t)p.hb_period_ms;
    Haptics_Service(due-1,transmit,NULL); assert(sends==before);
    Haptics_Service(due,transmit,NULL); assert(sends==before+1);
    assert(sent_length==p.status_len+p.data_len+5U && sent[0]==0x10);
    assert(sent[3]==CONTROLLER_STATUS_BATTERY_UNAVAILABLE && sent[4]==0xff);
    const uint8_t *data=sent+3+p.status_len;
    for(unsigned i=0;i<MOTOR_COUNT;++i) assert(read_float(data+i*4)==positions[i]);
    assert(read_float(data+32)==0.01f && read_float(data+36)==0.02f && read_float(data+40)==0.03f);
    assert(((uint16_t)sent[sent_length-2]|((uint16_t)sent[sent_length-1]<<8))==Handshake_Crc16(sent,sent_length-2));
    puts("PASS haptics SYNC during WORK -> HELLO -> ACK -> periodic 8 angles + 3 voltages, LE float32/CRC, independent decoding session");

    busy=true; before=sends;
    assert(ForceSensor_PublishVoltage(MOTOR_THUMB,0.025f,clock_ms));
    Haptics_Service(due+10,transmit,NULL); assert(sends==before);
    assert(ForceSensor_PublishVoltage(MOTOR_THUMB,0.035f,clock_ms));
    source_ready=true; ForceSensor_Service();
    busy=false; Haptics_Service(due+11,transmit,NULL); assert(sends==before+1);
    assert(read_float(sent+3+p.status_len+32)==0.04f);
    Haptics_Service(due+11,transmit,NULL); assert(sends==before+1);
    puts("PASS busy TX has no telemetry backlog; multiple force samples and board-reader service send only the latest voltage, no catch-up burst");

    sync_at(50); Haptics_Service(50,transmit,NULL);
    assert(Haptics_GetState()==HAPTICS_WAIT_REPLY && sent[0]==1);
    haptics_parameters_t remote={7,3,48,5}; reply(3,remote,51);
    Haptics_GetParameters(&p); assert(!memcmp(&p,&remote,sizeof(p)));
    before=sends; Haptics_Service(55,transmit,NULL); assert(sends==before);
    Haptics_Service(56,transmit,NULL); assert(sends==before+1 && sent_length==56);
    assert(CommsSession_GetState()==APP_WORK && epoch()==motor_epoch);
    puts("PASS new SYNC renegotiates during streaming; NACK adopts all four remote parameters and its 5 ms period");

    uint8_t status_frame[]={0x10,88,5,9,0xa5,0x5a,0,0};
    uint16_t crc=Handshake_Crc16(status_frame,6); status_frame[6]=(uint8_t)crc; status_frame[7]=(uint8_t)(crc>>8);
    feed(status_frame,4,57); haptics_status_t st; Haptics_GetStatus(&st); assert(!st.valid);
    feed(status_frame+4,4,58); Haptics_GetStatus(&st);
    assert(st.valid && st.state==9 && st.length==3 && st.bytes[1]==0xa5 && st.bytes[2]==0x5a && st.sequence==88);
    status_frame[7]^=1; feed(status_frame,sizeof(status_frame),59);
    Haptics_GetStatus(&st); assert(st.revision==1 && st.state==9);
    assert(CommsSession_GetState()==APP_WORK);
    puts("PASS fragmented haptics status saved as raw bytes + provisional first-byte state; bad CRC ignored, no state-dependent motor branch");

    /* Noise before a partial valid frame must not age that frame's deadline. */
    uint8_t noisy[]={0,7,0}; feed(noisy,sizeof(noisy),100);
    uint8_t recovery[5]; Handshake_EncodeSync(recovery,sizeof(recovery));
    feed(recovery,2,199); feed(recovery+2,3,201);
    assert(Haptics_GetState()==HAPTICS_SEND_HELLO);
    puts("PASS haptics suffix resynchronization tracks the surviving frame timestamp");

    sync_at(200); Haptics_Service(200,transmit,NULL);
    haptics_parameters_t unsupported={1,1,4,5}; reply(3,unsupported,201);
    Haptics_GetParameters(&p); assert(p.data_len==4 && Haptics_GetState()==HAPTICS_INCOMPATIBLE);
    before=sends; Haptics_Service(210,transmit,NULL); assert(sends==before);
    sync_at(300); Haptics_Service(300,transmit,NULL); before=sends;
    Haptics_Service(800,transmit,NULL); assert(sends==before+1 && sent[0]==1);
    puts("PASS incompatible NACK values preserved without overflowing/truncating telemetry; next SYNC and HELLO retry recover negotiation");

    ForceSensor_Init(); force_sensor_sample_t sample;
    assert(!ForceSensor_GetLatest(MOTOR_THUMB,&sample) && isnan(sample.voltage_v));
    assert(ForceSensor_PublishVoltage(MOTOR_INDEX,0.02f,clock_ms));
    assert(ForceSensor_PublishVoltage(MOTOR_INDEX,0.03f,clock_ms+1)==false);
    assert(ForceSensor_GetLatest(MOTOR_INDEX,&sample) && sample.voltage_v==0.02f);
    assert(!ForceSensor_PublishVoltage(MOTOR_INDEX,0.01f,clock_ms-1));
    assert(!ForceSensor_PublishVoltage(MOTOR_WRIST_X,0.02f,clock_ms));
    assert(ForceSensor_PublishVoltage(MOTOR_INDEX,MOTOR_FORCE_SOFT_LIMIT_V,clock_ms));
    uint32_t key=Motor_Lock();
    assert(MotorSafety_PwmLimitLocked(MOTOR_INDEX,100,clock_ms)==MOTOR_FORCE_REDUCED_SPEED);
    Motor_Unlock(key);
    assert(ForceSensor_PublishVoltage(MOTOR_INDEX,MOTOR_FORCE_HARD_LIMIT_V,clock_ms));
    assert(MotorSafety_GetStopCauses()&MOTOR_STOP_FORCE); assert_stopped();
    puts("PASS missing/future/unsupported force inputs are not fabricated; voltage hard threshold immediately stops all motors");

    boot_to_handshake();
    motor_feedback_t wrist={.id=MOTOR_WRIST_X,.force_voltage_v=MOTOR_FORCE_WRIST_HARD_LIMIT_V,
        .valid_fields=MOTOR_FB_FORCE_VOLTAGE};
    MotorSafety_OnFeedback(&wrist); assert_stopped();
    assert(MotorSafety_GetStopCauses()&MOTOR_STOP_FORCE);
    boot_to_handshake();
    assert(!ForceSensor_PublishVoltage(MOTOR_THUMB,NAN,clock_ms));
    assert(MotorSafety_GetStopCauses()&MOTOR_STOP_FEEDBACK); assert_stopped();
    puts("PASS direct voltage soft/wrist thresholds and invalid sensor voltage use the existing global safety interlock");
    return 0;
}
