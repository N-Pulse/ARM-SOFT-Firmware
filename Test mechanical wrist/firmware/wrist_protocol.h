#ifndef WRIST_PROTOCOL_H
#define WRIST_PROTOCOL_H
#include <stdint.h>
void Protocol_Init(uint32_t now);
void Protocol_Byte(uint8_t byte, uint32_t now);
void Protocol_Service(uint32_t now);
#endif
