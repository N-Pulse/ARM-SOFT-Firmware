from time import sleep

import serial
import argparse
import protocol_messages_pb2
from crc import Calculator, Crc16
import time


def get_uart_message(ser, timeout=5.0):
    calculator = Calculator(Crc16.IBM_3740)
    start_time = time.time()

    # Wait for 0xAA
    while True:
        if time.time() - start_time > timeout:
            print("Timeout waiting for header")
            return None

        if ser.in_waiting:
            if ser.read(1) == b'\xAA':
                break

    # Read rest with timeout
    def read_with_timeout(n):
        data = bytearray()
        while len(data) < n:
            if time.time() - start_time > timeout:
                raise TimeoutError

            if ser.in_waiting:
                data += ser.read(n - len(data))
        return data

    try:
        length = int.from_bytes(read_with_timeout(1), "little")
        message = read_with_timeout(length)
        return message
        '''crc_expected = int.from_bytes(read_with_timeout(2), "little")

        crc_actual = calculator.checksum(message)

        if crc_expected == crc_actual:
            return message
        else:
            print(f"CRC error: expected {crc_expected}, got {crc_actual}")
            return None'''

    except TimeoutError:
        print("Timeout during message reception")
        return None


def deserialize_device_message(message_bytes):
    proto_message = protocol_messages_pb2.DeviceMessage()
    proto_message.ParseFromString(message_bytes)

    # print(proto_message)

    # Check if the message has the 'hello' field
    if proto_message.HasField('hello'):
        print("Hello message received")
        print("Device ID: " + str(proto_message.hello.device_id))
        print("Proto version: " + str(proto_message.hello.proto_ver))
        print("Supported modes: " + str(proto_message.hello.supported_modes))

        # After receiving Hello, send a Config message
        send_config_message(ser)

    # Check if the message has the 'data' field
    elif proto_message.HasField('data'):
        print("Data message received")

        data_msg = proto_message.data

        # Check if instruction (ClassificationData) is present
        if data_msg.HasField('instruction'):
            instruction = data_msg.instruction

            action = instruction.action

            # Convert enum to readable string
            action_name = protocol_messages_pb2.ClassificationData.Action.Name(action)

            print(f"Action (enum): {action}")
            print(f"Action (name): {action_name}")

        else:
            print("No instruction field set in Data message")

    else:
        print("Unknown message received")


# Function to send a Config message
def send_config_message(ser):
    # Create a new Config message (this depends on your protocol message definition)
    config_msg = protocol_messages_pb2.Config()
    config_msg.selected_mode = 1  # Example configuration, set mode as needed

    # Create a DeviceMessage that wraps the config message
    prosthesis_msg = protocol_messages_pb2.ProsthesisMessage()
    prosthesis_msg.config.CopyFrom(config_msg)

    # Serialize the message to a byte string
    message_bytes = prosthesis_msg.SerializeToString()

    # Send the start byte (0xAA)
    ser.write(b'\xAA')

    # Send the length of the message (1 byte)
    ser.write(len(message_bytes).to_bytes(1, 'little'))

    # Send the actual message
    ser.write(message_bytes)

    print("\nSent Config message: ")
    print(config_msg)



if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('-c', '--com', default="/dev/ttyACM0", help='The desired comport to open')
    args = parser.parse_args()

    while True:
        # Try to open the serial port with the default baud rate.
        with serial.Serial(args.com, 115200, timeout=1) as ser:
            while True:
                message = get_uart_message(ser)
                if message:
                    deserialize_device_message(message)    
                    print()          
                    # print(f"Error: {e}")
    
        sleep(3)
                


