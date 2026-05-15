/*
 * uart.h
 *
 *  Created on: 5 Apr 2026
 *      Author: romch
 */

#ifndef INC_UART_H_
#define INC_UART_H_


// uart.h
#ifndef UART_H
#define UART_H

extern UART_HandleTypeDef hcom_uart[COMn];

void UART_Init(void);

#endif


#endif /* INC_UART_H_ */
