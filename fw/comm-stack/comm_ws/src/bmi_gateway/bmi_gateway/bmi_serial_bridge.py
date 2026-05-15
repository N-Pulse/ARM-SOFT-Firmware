import rclpy
from rclpy.node import Node
from std_msgs.msg import Int8

import serial
import time

from . import protocol_messages_pb2
# from crc import Calculator, Crc16


class BmiSerialBridge(Node):

    def __init__(self):
        super().__init__('bmi_serial_bridge')

        # Parameters (optional but useful)
        self.declare_parameter('port', '/dev/ttyACM0')
        self.declare_parameter('baudrate', 115200)

        port = self.get_parameter('port').get_parameter_value().string_value
        baudrate = self.get_parameter('baudrate').get_parameter_value().integer_value

        # Publisher
        self.publisher_ = self.create_publisher(Int8, '/pose_goals', 10)

        # Serial
        try:
            self.ser = serial.Serial(port, baudrate, timeout=0.1)
            self.get_logger().info(f"Opened serial port {port}")
        except Exception as e:
            self.get_logger().error(f"Failed to open serial port: {e}")
            raise

        # self.calculator = Calculator(Crc16.IBM_3740)

        # Timer instead of while True
        self.timer = self.create_timer(0.01, self.read_serial_callback)

    # ---------------- UART ----------------

    def get_uart_message(self, timeout=0.5):
        start_time = time.time()

        # Wait for header 0xAA
        while True:
            if time.time() - start_time > timeout:
                return None

            if self.ser.in_waiting:
                if self.ser.read(1) == b'\xAA':
                    break

        def read_with_timeout(n):
            data = bytearray()
            while len(data) < n:
                if time.time() - start_time > timeout:
                    raise TimeoutError

                if self.ser.in_waiting:
                    data += self.ser.read(n - len(data))
            return data

        try:
            length = int.from_bytes(read_with_timeout(1), "little")
            message = read_with_timeout(length)
            return message

        except TimeoutError:
            self.get_logger().warn("Timeout during message reception")
            return None

    # ---------------- PROTO ----------------

    def deserialize_device_message(self, message_bytes):
        proto_message = protocol_messages_pb2.DeviceMessage()
        proto_message.ParseFromString(message_bytes)

        if proto_message.HasField('hello'):
            self.get_logger().info("Hello message received")

            self.send_config_message()

        elif proto_message.HasField('data'):
            data_msg = proto_message.data

            if data_msg.HasField('instruction'):
                instruction = data_msg.instruction
                action = instruction.action

                # Publish to ROS topic
                msg = Int8()
                msg.data = action
                self.publisher_.publish(msg)

                self.get_logger().info(f"Published action: {action}")

            else:
                self.get_logger().warn("No instruction field in Data message")

    # ---------------- SEND CONFIG ----------------

    def send_config_message(self):
        config_msg = protocol_messages_pb2.Config()
        config_msg.selected_mode = 1

        prosthesis_msg = protocol_messages_pb2.ProsthesisMessage()
        prosthesis_msg.config.CopyFrom(config_msg)

        message_bytes = prosthesis_msg.SerializeToString()

        self.ser.write(b'\xAA')
        self.ser.write(len(message_bytes).to_bytes(1, 'little'))
        self.ser.write(message_bytes)

        self.get_logger().info("Sent Config message")

    # ---------------- TIMER CALLBACK ----------------

    def read_serial_callback(self):
        message = self.get_uart_message()

        if message:
            try:
                self.deserialize_device_message(message)
            except Exception as e:
                self.get_logger().error(f"Deserialization error: {e}")


# ---------------- MAIN ----------------

def main(args=None):
    rclpy.init(args=args)

    node = BmiSerialBridge()

    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass

    node.destroy_node()
    rclpy.shutdown()


if __name__ == '__main__':
    main()