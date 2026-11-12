#include "handshake_protocol.h"
#include "handshake_config.h"

uint16_t Handshake_Crc16(const uint8_t *bytes, size_t length)
{
    /* CRC-16/CCITT-FALSE: poly=0x1021, init=0xffff, no reflection/xorout. */
    uint16_t crc=0xffffU;
    for(size_t i=0;i<length;++i) {
        crc^=(uint16_t)bytes[i]<<8U;
        for(unsigned bit=0;bit<8;++bit)
            crc=(crc&0x8000U) ? (uint16_t)((crc<<1U)^0x1021U) : (uint16_t)(crc<<1U);
    }
    return crc;
}
size_t Handshake_EncodeSync(uint8_t *out, size_t capacity)
{
    if(!out || capacity<HANDSHAKE_SYNC_SIZE) return 0;
    out[0]=HANDSHAKE_SYNC; out[1]=0; out[2]=1;
    uint16_t crc=Handshake_Crc16(out,3);
    /* SYNC uses MSB first, exactly 00 00 01 DC BD in the supplied layout. */
    out[3]=(uint8_t)(crc>>8U); out[4]=(uint8_t)crc;
    return HANDSHAKE_SYNC_SIZE;
}
bool Handshake_DecodeHello(const uint8_t *frame, size_t length, bool *matches)
{
    if(!frame || !matches || length!=HANDSHAKE_HELLO_SIZE ||
       frame[0]!=HANDSHAKE_HELLO || frame[2]!=6U) return false;
    uint16_t crc=(uint16_t)frame[7]|((uint16_t)frame[8]<<8U);
    if(crc!=Handshake_Crc16(frame,7)) return false;
    *matches=frame[3]==HANDSHAKE_PROTO_VER && frame[4]==HANDSHAKE_STATUS_LEN &&
             frame[5]==HANDSHAKE_DATA_LEN && frame[6]==HANDSHAKE_HB_PERIOD_MS;
    return true;
}
size_t Handshake_EncodeReply(uint8_t *out, size_t capacity, uint8_t seq, bool matches)
{
    if(!out || capacity<HANDSHAKE_HELLO_SIZE) return 0;
    out[0]=matches ? HANDSHAKE_ACK : HANDSHAKE_NACK; out[1]=seq; out[2]=6;
    /* Both replies advertise the local version, including on disagreement. */
    out[3]=HANDSHAKE_PROTO_VER; out[4]=HANDSHAKE_STATUS_LEN;
    out[5]=HANDSHAKE_DATA_LEN; out[6]=HANDSHAKE_HB_PERIOD_MS;
    uint16_t crc=Handshake_Crc16(out,7);
    out[7]=(uint8_t)crc; out[8]=(uint8_t)(crc>>8U);
    return HANDSHAKE_HELLO_SIZE;
}
