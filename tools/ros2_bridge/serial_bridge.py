#!/usr/bin/env python3
"""
ROS2 bridge for the N-Pulse STM32 simulation transport.

Translates serial frames from the firmware into ROS2 controller commands and
feeds joint/FSR feedback back to the microcontroller.
"""

import argparse
import threading
import time
from typing import List, Tuple

import rclpy
from rclpy.node import Node
from sensor_msgs.msg import JointState
from std_msgs.msg import Float64MultiArray, Float32MultiArray

import serial  # type: ignore

FRAME_SYNC = 0xAA
FRAME_VERSION = 0x01
MSG_SET_TARGETS = 0x01
MSG_ALL_STOP = 0x02
MSG_JOINT_FEEDBACK = 0x10
MSG_FSR_FEEDBACK = 0x11
MSG_HEARTBEAT_ACK = 0x12

SIM_POSITION_RANGE_RAD = 3.1415926


def float_to_q15(value: float) -> int:
    normalized = max(-1.0, min(1.0, value / SIM_POSITION_RANGE_RAD))
    return int(round(normalized * 32767.0))


def q15_to_float(raw: int) -> float:
    return (raw / 32767.0) * SIM_POSITION_RANGE_RAD


def crc8_update(crc: int, data: int) -> int:
    crc ^= data
    for _ in range(8):
        if crc & 0x80:
            crc = ((crc << 1) ^ 0x1D) & 0xFF
        else:
            crc = (crc << 1) & 0xFF
    return crc


def compute_crc(version: int, msg_id: int, length: int, payload: bytes) -> int:
    crc = 0xFF
    crc = crc8_update(crc, version)
    crc = crc8_update(crc, msg_id)
    crc = crc8_update(crc, length)
    for byte in payload:
        crc = crc8_update(crc, byte)
    return crc


class FrameParser:
    """Byte-wise parser for simulation frames."""

    def __init__(self, callback):
        self.state = "SYNC"
        self.version = 0
        self.msg_id = 0
        self.length = 0
        self.payload = bytearray()
        self.callback = callback

    def feed(self, byte: int):
        if self.state == "SYNC":
            if byte == FRAME_SYNC:
                self.state = "HEADER"
                self.payload.clear()
                self.version = 0
                self.msg_id = 0
                self.length = 0
                self.header_index = 0
        elif self.state == "HEADER":
            if self.header_index == 0:
                self.version = byte
            elif self.header_index == 1:
                self.msg_id = byte
            elif self.header_index == 2:
                self.length = byte
                if self.length == 0:
                    self.state = "CRC"
                    self.header_index = 0
                    return
                self.state = "PAYLOAD"
            self.header_index += 1
        elif self.state == "PAYLOAD":
            self.payload.append(byte)
            if len(self.payload) >= self.length:
                self.state = "CRC"
        elif self.state == "CRC":
            crc = compute_crc(self.version, self.msg_id, self.length, self.payload)
            if crc == byte:
                self.callback(self.msg_id, bytes(self.payload))
            self.state = "SYNC"
            self.payload.clear()


class SerialBridge(Node):
    """ROS2 node that bridges between serial frames and ros2_control topics."""

    def __init__(self, port: str, baudrate: int, joint_names: List[str], fsr_topics: List[str]):
        super().__init__("n_pulse_serial_bridge")
        self.serial = serial.Serial(port, baudrate=baudrate, timeout=0.01)
        self.joint_names = joint_names
        self.fsr_topics = fsr_topics
        self.writer_lock = threading.Lock()
        self.parser = FrameParser(self.handle_frame)

        self.cmd_pub = self.create_publisher(Float64MultiArray, "/forward_position_controller/commands", 10)
        self.stop_pub = self.create_publisher(Float64MultiArray, "/forward_position_controller/commands", 10)
        self.joint_state_sub = self.create_subscription(JointState, "/joint_states", self.handle_joint_state, 10)
        self.fsr_subs = [
            self.create_subscription(Float32MultiArray, topic, self.handle_fsr_feedback, 10)
            for topic in fsr_topics
        ]

        self.reader_thread = threading.Thread(target=self.read_loop, daemon=True)
        self.reader_thread.start()
        self.get_logger().info("Serial bridge ready on %s @ %d baud", port, baudrate)

    # ---- Serial RX ----
    def read_loop(self):
        while rclpy.ok():
            data = self.serial.read(1)
            if data:
                self.parser.feed(data[0])

    def handle_frame(self, msg_id: int, payload: bytes):
        if msg_id == MSG_SET_TARGETS:
            self.process_set_targets(payload)
        elif msg_id == MSG_ALL_STOP:
            self.publish_stop()
        else:
            self.get_logger().debug("Unhandled frame id=0x%02X", msg_id)

    def process_set_targets(self, payload: bytes):
        if len(payload) < 3:
            return
        heartbeat = payload[0] | (payload[1] << 8)
        joint_count = payload[2]
        offset = 3
        entries: List[Tuple[int, float, int]] = []
        for _ in range(joint_count):
            if offset + 4 > len(payload):
                break
            motor_id = payload[offset]
            pos_raw = payload[offset + 1] | (payload[offset + 2] << 8)
            speed = payload[offset + 3]
            offset += 4
            entries.append((motor_id, q15_to_float(pos_raw if pos_raw < 0x8000 else pos_raw - 0x10000), speed))

        msg = Float64MultiArray()
        msg.data = [pos for (_, pos, _) in entries]
        self.cmd_pub.publish(msg)
        self.send_heartbeat_ack(heartbeat)

    def publish_stop(self):
        msg = Float64MultiArray()
        msg.data = [0.0] * len(self.joint_names)
        self.stop_pub.publish(msg)

    # ---- ROS -> Serial ----
    def handle_joint_state(self, msg: JointState):
        payload = bytearray()
        payload.append(len(self.joint_names))
        for name in self.joint_names:
            try:
                idx = msg.name.index(name)
            except ValueError:
                continue
            payload.append(self.joint_names.index(name))
            pos_q15 = float_to_q15(msg.position[idx])
            vel_q15 = float_to_q15(msg.velocity[idx] if msg.velocity else 0.0)
            payload.append(pos_q15 & 0xFF)
            payload.append((pos_q15 >> 8) & 0xFF)
            payload.append(vel_q15 & 0xFF)
            payload.append((vel_q15 >> 8) & 0xFF)
        self.send_frame(MSG_JOINT_FEEDBACK, payload)

    def handle_fsr_feedback(self, msg: Float32MultiArray):
        payload = bytearray()
        payload.append(len(msg.data))
        for idx, value in enumerate(msg.data):
            payload.append(idx)
            force_mn = int(max(0.0, min(65535.0, value)))
            payload.append(force_mn & 0xFF)
            payload.append((force_mn >> 8) & 0xFF)
        self.send_frame(MSG_FSR_FEEDBACK, payload)

    # ---- Serial TX helpers ----
    def send_heartbeat_ack(self, heartbeat: int):
        payload = bytearray()
        payload.append(heartbeat & 0xFF)
        payload.append((heartbeat >> 8) & 0xFF)
        self.send_frame(MSG_HEARTBEAT_ACK, payload)

    def send_frame(self, msg_id: int, payload: bytes):
        frame = bytearray()
        frame.append(FRAME_SYNC)
        frame.append(FRAME_VERSION)
        frame.append(msg_id)
        frame.append(len(payload))
        frame.extend(payload)
        frame.append(compute_crc(FRAME_VERSION, msg_id, len(payload), payload))
        with self.writer_lock:
            self.serial.write(frame)


def main():
    parser = argparse.ArgumentParser(description="ROS2 <-> STM32 serial bridge")
    parser.add_argument("--port", default="/dev/ttyACM0", help="Serial port of the Nucleo VCP")
    parser.add_argument("--baudrate", type=int, default=921600, help="Serial baudrate")

    parser.add_argument(
        "--joint-names",
        nargs="+",
        default=[
            "thumb",
            "index",
            "middle",
            "ring",
            "little",
            "wrist_x",
            "wrist_y",
            "palm",
        ],
        help="Ordered joint names matching motor_id_t enumeration",
    )
    parser.add_argument(
        "--fsr-topics",
        nargs="*",
        default=["/n_pulse/fsr"],
        help="Topics (Float32MultiArray) providing per-finger force estimates",
    )
    args = parser.parse_args()

    rclpy.init()
    bridge = SerialBridge(args.port, args.baudrate, args.joint_names, args.fsr_topics)
    try:
        rclpy.spin(bridge)
    except KeyboardInterrupt:
        pass
    finally:
        bridge.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    main()

