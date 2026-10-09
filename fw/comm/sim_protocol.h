#ifndef SIM_PROTOCOL_H
#define SIM_PROTOCOL_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
/* Deliberately incompatible with HW protobuf and legacy SIM v1. */
#define SIM_VERSION 2U
#define SIM_PAYLOAD_MAX 144U
#define SIM_FRAME_MAX (SIM_PAYLOAD_MAX + 7U)
#define SIM_COMMON_SIZE 16U
#define SIM_FRAME_TIMEOUT_MS 50U
#define SIM_INTERBYTE_TIMEOUT_MS 15U
enum { SIM_HELLO=1, SIM_HEARTBEAT, SIM_CONFIG, SIM_RECOVER, SIM_ARM,
       SIM_ENABLE, SIM_TARGET, SIM_STOP, SIM_FEEDBACK,
       SIM_HELLO_ACK=0x81, SIM_STATUS, SIM_OUTPUT };
typedef bool (*sim_frame_fn)(uint8_t, const uint8_t *, size_t, uint32_t, uint32_t, void *);
typedef struct {
    uint8_t bytes[SIM_FRAME_MAX];
    uint32_t byte_ms[SIM_FRAME_MAX];
    size_t used;
    uint32_t first_ms, last_ms, epoch;
    uint32_t invalid, timeouts, frames;
} sim_parser_t;
uint16_t Sim_Crc16(const uint8_t *, size_t);
size_t Sim_Encode(uint8_t type, const uint8_t *, size_t, uint8_t *, size_t);
void SimParser_Init(sim_parser_t *);
void SimParser_Reset(sim_parser_t *);
void SimParser_Poll(sim_parser_t *, uint32_t);
void SimParser_Feed(sim_parser_t *, uint8_t, uint32_t, uint32_t, sim_frame_fn, void *);
bool Sim_SequenceNewer(uint32_t incoming, uint32_t previous);
uint16_t Sim_Read16(const uint8_t *);
uint32_t Sim_Read32(const uint8_t *);
uint64_t Sim_Read64(const uint8_t *);
float Sim_ReadFloat(const uint8_t *);
void Sim_Write16(uint8_t *, uint16_t);
void Sim_Write32(uint8_t *, uint32_t);
void Sim_Write64(uint8_t *, uint64_t);
void Sim_WriteFloat(uint8_t *, float);
#endif
