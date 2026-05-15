/*
 * tx.cpp
 *
 *  Created on: 5 Apr 2026
 *      Author: romch
 */

#include <stdio.h>
#include <cstdint>
#include <WriteBufferFixedSize.h>
#include <ReadBufferFixedSize.h>

#include "tx.h"
#include "protocol_messages.h"

#ifndef SRC_TX_CPP_
#define SRC_TX_CPP_


void SendStartByte(void){
	uint8_t start_byte = 0xAA;
	HAL_UART_Transmit(&hcom_uart[COM1], &start_byte, 1, HAL_MAX_DELAY);
}

/**
  * @brief  Build and transmit a Hello message over the VCP (LPUART1 via BSP COM1).
  */
void SendHelloMessage(void)
{
  /* 1. Fill the Hello fields */
  Hello hello_msg;
  hello_msg.set_device_id(1);
  hello_msg.set_proto_ver(1);
  hello_msg.set_supported_modes(0b00000001);

  /* 2. Wrap it in a DeviceMessage (oneof -> hello) */
  DeviceMessage dev_msg;
  dev_msg.set_hello(hello_msg);

  /* 3. Serialize into a fixed-size write buffer */
  EmbeddedProto::WriteBufferFixedSize<TX_BUFFER_SIZE> buffer_out;
  auto err = dev_msg.serialize(buffer_out);

  /* 4. Transmit over VCP if serialization succeeded.
   *    hcom_uart[] is the BSP's internal UART handle array;
   *    COM1 index gives us the LPUART1 handle that BSP_COM_Init configured.
   */
  if (EmbeddedProto::Error::NO_ERRORS == err)
  {
	uint8_t msg_length = static_cast<uint8_t>(buffer_out.get_size()); // cast to 1 byte

	SendStartByte();

    // Send length byte
    HAL_UART_Transmit(&hcom_uart[COM1], &msg_length, 1, HAL_MAX_DELAY);
    extern UART_HandleTypeDef hcom_uart[COMn];
    HAL_UART_Transmit(&hcom_uart[COM1],
                      const_cast<uint8_t*>(buffer_out.get_data()),
                      buffer_out.get_size(),
                      HAL_MAX_DELAY);
  }
}


void SendClassificationData(uint8_t input)
{
  /* 1. Create ClassificationData */
  ClassificationData class_msg;

  // Map input → enum
  ClassificationData::Action action;

  switch(input)
  {
    case 0: action = ClassificationData::Action::OPEN_HAND; break;
    case 1: action = ClassificationData::Action::CLOSE_HAND; break;
    case 2: action = ClassificationData::Action::PINCH; break;
    case 3: action = ClassificationData::Action::ROTATE_WRIST_R; break;
    case 4: action = ClassificationData::Action::ROTATE_WRIST_L; break;

    default:
      action = ClassificationData::Action::OPEN_HAND;
      break;
  }

  class_msg.set_action(action);

  /* 2. Wrap into Data (oneof -> instruction) */
  Data data_msg;
  data_msg.set_instruction(class_msg);

  /* 3. Wrap into DeviceMessage */
  DeviceMessage dev_msg;
  dev_msg.set_data(data_msg);   // <-- THIS is the key change

  /* 4. Serialize */
  EmbeddedProto::WriteBufferFixedSize<TX_BUFFER_SIZE> buffer_out;
  auto err = dev_msg.serialize(buffer_out);

  /* 5. Send over UART */
  if (EmbeddedProto::Error::NO_ERRORS == err)
  {
    uint8_t msg_length = static_cast<uint8_t>(buffer_out.get_size());

    SendStartByte();

    HAL_UART_Transmit(&hcom_uart[COM1], &msg_length, 1, HAL_MAX_DELAY);

    HAL_UART_Transmit(&hcom_uart[COM1],
                      const_cast<uint8_t*>(buffer_out.get_data()),
                      buffer_out.get_size(),
                      HAL_MAX_DELAY);
  }
}


//void SendDataMessage(void)
//{
//  /* 1. Fill the Data fields */
//  Data<DATA_REP_LENGTH> data_msg;
//
//  // Example: add some values to the repeated field
//  data_msg.add_data(10);
//  data_msg.add_data(20);
//  data_msg.add_data(30);
//
//  // Alternatively (indexed):
//  // data_msg.set_data(0, 10);
//  // data_msg.set_data(1, 20);
//  // data_msg.set_data(2, 30);
//
//  /* 2. Wrap it in a DeviceMessage (oneof -> data) */
//  DeviceMessage<DATA_REP_LENGTH> dev_msg;
//  dev_msg.set_data(data_msg);
//
//  /* 3. Serialize into a fixed-size write buffer */
//  EmbeddedProto::WriteBufferFixedSize<TX_BUFFER_SIZE> buffer_out;
//  auto err = dev_msg.serialize(buffer_out);
//
//  /* 4. Transmit over UART if serialization succeeded */
//  if (EmbeddedProto::Error::NO_ERRORS == err)
//  {
//	SendStartByte();
//    uint8_t msg_length = static_cast<uint8_t>(buffer_out.get_size());
//
//    // Send length byte
//    HAL_UART_Transmit(&hcom_uart[COM1], &msg_length, 1, HAL_MAX_DELAY);
//
//    // Send payload
//    HAL_UART_Transmit(&hcom_uart[COM1],
//                      const_cast<uint8_t*>(buffer_out.get_data()),
//                      buffer_out.get_size(),
//                      HAL_MAX_DELAY);
//  }
//}


#endif /* SRC_TX_CPP_ */
