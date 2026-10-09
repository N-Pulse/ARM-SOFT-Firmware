/*
 * tx.h
 *
 *  Created on: 5 Apr 2026
 *      Author: romch
 */

#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifndef INC_TX_H_
#define INC_TX_H_

#define TX_BUFFER_SIZE  128
#define DATA_REP_LENGTH  64

#ifdef __cplusplus
extern "C" {
#endif
bool SendHelloMessage(void);

// void SendDataMessage(void);

bool SendClassificationData(uint8_t input);
#ifdef __cplusplus
}
#endif

#endif /* INC_TX_H_ */
