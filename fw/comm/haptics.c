#include "haptics.h"
#include "handshake_protocol.h"
#include "motor_lock.h"
#include "force_sensor.h"
#include "encoder_bsp.h"
#include "controller_status.h"
#include <math.h>
#include <string.h>
#include <float.h>
#if FLT_RADIX != 2 || FLT_MANT_DIG != 24 || FLT_MAX_EXP != 128
#error Haptics telemetry requires IEEE754 binary32
#endif

static haptics_state_t state;
static haptics_parameters_t parameters;
static haptics_status_t status;
static uint8_t rx[HAPTICS_FRAME_MAX],hello_sequence,data_sequence;
static uint32_t rx_ms[HAPTICS_FRAME_MAX];
static size_t used;
static uint32_t first_rx_ms,last_rx_ms,hello_ms,next_data_ms;
static bool hello_sent;
static haptics_parameters_t local_parameters(void) {
    return (haptics_parameters_t){HAPTICS_PROTO_VER,HAPTICS_STATUS_LEN,HAPTICS_DATA_LEN,HAPTICS_HB_PERIOD_MS};
}
void Haptics_Init(void) {
    uint32_t key=Motor_Lock();
    state=HAPTICS_WAIT_SYNC; parameters=local_parameters();
    memset(&status,0,sizeof(status)); used=0; hello_sequence=data_sequence=0;
    first_rx_ms=last_rx_ms=hello_ms=next_data_ms=0; hello_sent=false;
    Motor_Unlock(key);
}
haptics_state_t Haptics_GetState(void) {
    uint32_t key=Motor_Lock(); haptics_state_t s=state; Motor_Unlock(key); return s;
}
void Haptics_GetParameters(haptics_parameters_t *out) {
    if(!out) return;
    uint32_t key=Motor_Lock(); *out=parameters; Motor_Unlock(key);
}
void Haptics_GetStatus(haptics_status_t *out) {
    if(!out) return;
    uint32_t key=Motor_Lock(); *out=status; Motor_Unlock(key);
}
static bool crc_valid(const uint8_t *f,size_t n) {
    return n>=5U && ((uint16_t)f[n-2U]|((uint16_t)f[n-1U]<<8U))==Handshake_Crc16(f,n-2U);
}
static bool usable(haptics_parameters_t p) {
    return p.hb_period_ms && p.data_len>=HAPTICS_TELEMETRY_BYTES &&
        (unsigned)p.status_len+p.data_len+2U<=255U;
}
static void handle(const uint8_t *f,size_t n,uint32_t now) {
    if(f[0]==HANDSHAKE_SYNC) {
        uint8_t expected[5]; Handshake_EncodeSync(expected,sizeof(expected));
        if(n!=sizeof(expected) || memcmp(f,expected,sizeof(expected))) return;
        uint32_t key=Motor_Lock();
        /* SYNC may arrive at any point, including while motors are working. */
        state=HAPTICS_SEND_HELLO; parameters=local_parameters();
        ++hello_sequence; hello_sent=false; data_sequence=0;
        status.valid=false;
        Motor_Unlock(key); return;
    }
    if(!crc_valid(f,n)) return;
    if((f[0]==HANDSHAKE_ACK || f[0]==HANDSHAKE_NACK) &&
       n==9U && f[2]==6U && state==HAPTICS_WAIT_REPLY && f[1]==hello_sequence) {
        haptics_parameters_t p={f[3],f[4],f[5],f[6]};
        uint32_t key=Motor_Lock();
        if(f[0]==HANDSHAKE_NACK) parameters=p;
        else if(memcmp(&p,&parameters,sizeof(p))) { Motor_Unlock(key); return; }
        /* Preserve received NACK values even when they cannot fit the current
         * provisional telemetry schema; never overrun or silently truncate it. */
        state=usable(parameters) ? HAPTICS_STREAMING : HAPTICS_INCOMPATIBLE;
        next_data_ms=now+(uint32_t)parameters.hb_period_ms;
        Motor_Unlock(key); return;
    }
    if(f[0]==0x10U && state==HAPTICS_STREAMING &&
       n>=5U+parameters.status_len && parameters.status_len) {
        uint32_t key=Motor_Lock();
        status.sequence=f[1]; status.length=parameters.status_len;
        memcpy(status.bytes,f+3U,status.length);
        status.state=status.bytes[0]; status.timestamp_ms=now;
        ++status.revision; status.valid=true;
        Motor_Unlock(key);
    }
}
static void consume(size_t n) {
    used-=n; memmove(rx,rx+n,used);
    memmove(rx_ms,rx_ms+n,used*sizeof(rx_ms[0]));
    if(used) first_rx_ms=rx_ms[0];
}
void Haptics_FeedByte(uint8_t byte,uint32_t now) {
    if(used && ((uint32_t)(now-last_rx_ms)>HAPTICS_RX_TIMEOUT_MS ||
                (uint32_t)(now-first_rx_ms)>HAPTICS_RX_TIMEOUT_MS)) used=0;
    if(!used) first_rx_ms=now;
    last_rx_ms=now;
    if(used==sizeof(rx)) { used=0; first_rx_ms=now; }
    rx_ms[used]=now;
    rx[used++]=byte;
    while(used) {
        uint8_t type=rx[0];
        if(type!=0 && type!=2 && type!=3 && type!=0x10U) { consume(1); continue; }
        if(used<3U) return;
        size_t n=type==0 ? 5U : (size_t)rx[2]+3U;
        if((type==0 && rx[2]!=1U) || ((type==2 || type==3) && rx[2]!=6U) || n<5U) {
            consume(1); continue;
        }
        if(used<n) return;
        bool valid;
        if(type==0) {
            uint8_t sync[5]; Handshake_EncodeSync(sync,sizeof(sync));
            valid=!memcmp(rx,sync,5);
        } else valid=crc_valid(rx,n);
        if(valid) { handle(rx,n,now); consume(n); }
        else consume(1);
    }
}
static void write_float(uint8_t *out,float value) {
    uint32_t bits; memcpy(&bits,&value,sizeof(bits));
    for(unsigned i=0;i<4;++i) out[i]=(uint8_t)(bits>>(8U*i));
}
static size_t encode_data(uint8_t *out) {
    size_t payload=(size_t)parameters.status_len+parameters.data_len;
    size_t n=payload+5U; memset(out,0,n);
    out[0]=0x10; out[1]=data_sequence; out[2]=(uint8_t)(payload+2U);
    uint8_t *data=out+3U+parameters.status_len; uint16_t valid=0;
    /* One coherent latest snapshot; no force history or pending DATA queue. */
    uint32_t key=Motor_Lock();
    for(unsigned i=0;i<MOTOR_COUNT;++i) {
        encoder_snapshot_t sample;
        bool ok=Encoder_BSP_GetSnapshot((motor_id_t)i,&sample) && sample.valid && isfinite(sample.position_rad);
        write_float(data+4U*i,ok ? sample.position_rad : NAN);
        if(ok) valid|=(uint16_t)(1U<<i);
    }
    static const motor_id_t ids[]={MOTOR_THUMB,MOTOR_INDEX,MOTOR_MIDDLE};
    for(unsigned i=0;i<3;++i) {
        force_sensor_sample_t sample;
        bool ok=ForceSensor_GetLatest(ids[i],&sample);
        write_float(data+4U*(MOTOR_COUNT+i),ok ? sample.voltage_v : NAN);
        if(ok) valid|=(uint16_t)(1U<<(MOTOR_COUNT+i));
    }
    Motor_Unlock(key);
    if(parameters.status_len) out[3]=ControllerStatus_Read();
    if(parameters.status_len>1) out[4]=(uint8_t)valid;
    if(parameters.status_len>2) out[5]=(uint8_t)(valid>>8U);
    uint16_t crc=Handshake_Crc16(out,n-2U); out[n-2U]=(uint8_t)crc; out[n-1U]=(uint8_t)(crc>>8U);
    return n;
}
void Haptics_Service(uint32_t now,haptics_send_fn send,void *context) {
    if(!send) return;
    uint8_t frame[HAPTICS_FRAME_MAX];
    if(state==HAPTICS_SEND_HELLO || (state==HAPTICS_WAIT_REPLY &&
       (!hello_sent || (uint32_t)(now-hello_ms)>=HAPTICS_HELLO_RETRY_MS))) {
        frame[0]=1; frame[1]=hello_sequence; frame[2]=6;
        frame[3]=parameters.proto_ver; frame[4]=parameters.status_len;
        frame[5]=parameters.data_len; frame[6]=parameters.hb_period_ms;
        uint16_t crc=Handshake_Crc16(frame,7); frame[7]=(uint8_t)crc; frame[8]=(uint8_t)(crc>>8U);
        if(send(frame,9,context)) {
            uint32_t key=Motor_Lock(); state=HAPTICS_WAIT_REPLY;
            hello_sent=true; hello_ms=now; Motor_Unlock(key);
        }
    } else if(state==HAPTICS_STREAMING && (int32_t)(now-next_data_ms)>=0) {
        size_t n=encode_data(frame);
        if(send(frame,n,context)) {
            uint32_t period=(uint32_t)parameters.hb_period_ms;
            next_data_ms=now+period-(uint32_t)(now-next_data_ms)%period;
            ++data_sequence;
        }
    }
}
