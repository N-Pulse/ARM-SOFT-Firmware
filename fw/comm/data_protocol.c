#include "data_protocol.h"
#include "handshake_config.h"
#include "handshake_protocol.h"
#include "comms_config.h"
#if HANDSHAKE_STATUS_LEN + HANDSHAKE_DATA_LEN > COMMS_PAYLOAD_MAX || HANDSHAKE_STATUS_LEN + HANDSHAKE_DATA_LEN + 2U > 255U
#error DATA fields exceed the frame capacity
#endif

bool Data_Decode(const uint8_t *f,size_t n,const uint8_t **data,size_t *length)
{
    const size_t payload=HANDSHAKE_STATUS_LEN+HANDSHAKE_DATA_LEN;
    if(!f || !data || !length || n!=payload+5U ||
       f[0]!=DATA_PACKET_TYPE || f[2]!=payload+2U || payload>COMMS_PAYLOAD_MAX)
        return false;
    uint16_t crc=(uint16_t)f[n-2U]|((uint16_t)f[n-1U]<<8U);
    if(crc!=Handshake_Crc16(f,n-2U)) return false;
    *data=f+3U+HANDSHAKE_STATUS_LEN; *length=HANDSHAKE_DATA_LEN;
    return true;
}
