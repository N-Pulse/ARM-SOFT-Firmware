/*
 * tx.h
 *
 *  Created on: 5 Apr 2026
 *      Author: romch
 */

#pragma once
#include <cstdint>

#include "stm32g4xx_hal.h"
#include "stm32g4xx_nucleo.h"

#ifndef INC_TX_H_
#define INC_TX_H_

#define TX_BUFFER_SIZE  128
#define DATA_REP_LENGTH  64

void SendStartByte(void);

void SendHelloMessage(void);

// void SendDataMessage(void);

void SendClassificationData(uint8_t input);

#endif /* INC_TX_H_ */
