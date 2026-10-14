#!/usr/bin/env python3
"""
sim_bridge.py - fixed version of tools/ros2_bridge/serial_bridge.py for OUR simulation.

Changes compared to the original:
  * publishes a JointTrajectory to /joint_trajectory_controller/joint_trajectory
    (the original sent to /forward_position_controller/commands, which our sim doesn't have)
  * maps the firmware's 8 motors to the sim's joint names (MOTOR_TO_JOINTS below)
  * sends feedback with motor IDs the firmware understands, at 50 Hz (not 780 Hz)
  * fixes a logger call that crashed at startup and a wrong joint count in the feedback

Two modes:
  * with fake_board.py (virtual cable):
        python3 sim_bridge.py --port /tmp/pc_side
  * with the REAL board (flashed in SIM_MODE=1):
        python3 sim_bridge.py --port /dev/ttyACM0 --board
    In --board mode the bridge:
      - does NOT send feedback/acks (this firmware branch can't read them yet:
        incoming bytes all go to the protobuf parser, so they would only get in the way)
      - lets you send intents to the board:
            ros2 topic pub --once /firmware_intent std_msgs/msg/Int8 "{data: 1}"
        (0 open, 1 close, 2 pinch, 3 wrist right, 4 wrist left)
        debug, one motor at a time: 10..17 = motor N to 30 deg, 20..27 = back to 0
        (motor order: 0 thumb, 1 index, 2 middle, 3 ring, 4 little, 5 wrist_x, 6 wrist_y, 7 palm)
      - prints target vs measured (from /joint_states) once per second
"""
import argparse
import math
import threading

import rclpy
from rclpy.node import Node
from sensor_msgs.msg import JointState
from std_msgs.msg import Int8
from trajectory_msgs.msg import JointTrajectory, JointTrajectoryPoint
from builtin_interfaces.msg import Duration
import serial

SYNC, VERSION = 0xAA, 0x01
MSG_SET_TARGETS, MSG_ALL_STOP = 0x01, 0x02
MSG_JOINT_FEEDBACK, MSG_HEARTBEAT_ACK = 0x10, 0x12
RANGE_RAD = 3.1415926

# Firmware motor order (motor_id_t) -> sim joints driven by that motor.
# Every joint of a motor gets the same angle (simple first version).
# The FIRST joint of each list is the one read back as the "measured" motor angle.
# ⚠ thumb/palm split is a guess: check with the mechanical/firmware team.
MOTOR_TO_JOINTS = [
    ["joint_thumb_2", "joint_thumb_3"],                       # 0 thumb
    ["joint_index_1", "joint_index_2", "joint_index_3"],      # 1 index
    ["joint_middle_1", "joint_middle_2", "joint_middle_3"],   # 2 middle
    ["joint_ring_1", "joint_ring_2", "joint_ring_3"],         # 3 ring
    ["joint_little_1", "joint_little_2", "joint_little_3"],   # 4 little
    ["joint_wrist_x"],                                        # 5 wrist_x
    ["joint_wrist_y"],                                        # 6 wrist_y
    ["joint_thumb_1_x"],                                      # 7 palm (guess: thumb opposition)
]


def to_q15(rad):
    n = max(-1.0, min(1.0, rad / RANGE_RAD))
    return int(round(n * 32767))


def from_q15(raw):
    return raw / 32767.0 * RANGE_RAD


def crc8(data):
    crc = 0xFF
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1D) & 0xFF if crc & 0x80 else (crc << 1) & 0xFF
    return crc


def make_frame(msg_id, payload):
    body = bytes([VERSION, msg_id, len(payload)]) + payload
    return bytes([SYNC]) + body + bytes([crc8(body)])


MOTOR_NAMES = ["thumb", "index", "middle", "ring", "little", "wrist_x", "wrist_y", "palm"]
INTENT_NAMES = {0: "OPEN", 1: "CLOSE", 2: "PINCH", 3: "WRIST_R", 4: "WRIST_L"}
# Debug intents (need the matching debug block in fw/app/intent_router.c):
#   10..17 = move ONE motor to 30 deg, 20..27 = move that motor back to 0 deg
for _i, _name in enumerate(MOTOR_NAMES):
    INTENT_NAMES[10 + _i] = f"DEBUG {_name} -> 30 deg"
    INTENT_NAMES[20 + _i] = f"DEBUG {_name} -> 0 deg"


def intent_frame(action):
    """Same bytes as tools/send_action.py: [0xAA][len][DeviceMessage{data{instruction{action}}}].
    Protobuf by hand: ClassificationData.action = field 1, Data.instruction = field 1,
    DeviceMessage.data = field 2. In proto3, action 0 (OPEN) is simply not written."""
    cls = bytes([0x08, action]) if action else b""
    data = bytes([0x0A, len(cls)]) + cls
    dev = bytes([0x12, len(data)]) + data
    return bytes([SYNC, len(dev)]) + dev


class SimBridge(Node):
    def __init__(self, port, baud, board_mode, verbose=False):
        super().__init__("sim_bridge")
        self.verbose = verbose
        self.bad_crc = 0
        self.ser = serial.Serial(port, baud, timeout=0.01)
        self.board_mode = board_mode
        self.lock = threading.Lock()
        self.last_js = None          # latest /joint_states
        self.last_targets = None     # latest motor targets from the firmware {motor_id: rad}
        self.buf = bytearray()

        self.pub = self.create_publisher(
            JointTrajectory, "/joint_trajectory_controller/joint_trajectory", 10)
        self.create_subscription(JointState, "/joint_states", self.on_joint_states, 10)
        self.create_timer(0.05, self.publish_command)  # 20 Hz commands to the sim
        if board_mode:
            self.create_subscription(Int8, "/firmware_intent", self.on_intent, 10)
            self.create_timer(1.0, self.print_status)
        else:
            self.create_timer(0.02, self.send_feedback)   # 50 Hz feedback to the board

        threading.Thread(target=self.read_loop, daemon=True).start()
        mode = "REAL BOARD mode" if board_mode else "fake board mode"
        self.get_logger().info(f"Bridge ready on {port} @ {baud} ({mode})")

    # ---------- intents (board mode) ----------
    def on_intent(self, msg):
        if msg.data not in INTENT_NAMES:
            self.get_logger().warn(f"Unknown intent {msg.data} (use 0-4, or 10-17 / 20-27 for debug)")
            return
        self.write(intent_frame(msg.data))
        self.get_logger().info(f"Sent intent {INTENT_NAMES[msg.data]} to the board")

    def print_status(self):
        with self.lock:
            targets, js = self.last_targets, self.last_js
        if not targets or js is None:
            return
        parts = []
        for motor_id in sorted(targets):
            joint = MOTOR_TO_JOINTS[motor_id][0] if motor_id < len(MOTOR_TO_JOINTS) else None
            meas = js.position[js.name.index(joint)] if joint in js.name else float("nan")
            parts.append(f"{MOTOR_NAMES[motor_id]} {math.degrees(targets[motor_id]):.1f}/"
                         f"{math.degrees(meas):.1f}")
        self.get_logger().info("target/measured deg: " + "  ".join(parts))

    # ---------- board -> sim ----------
    def read_loop(self):
        while rclpy.ok():
            data = self.ser.read(256)
            if data:
                self.buf += data
                self.extract_frames()

    def extract_frames(self):
        while True:
            start = self.buf.find(bytes([SYNC]))
            if start < 0:
                self.buf.clear()
                return
            del self.buf[:start]
            if len(self.buf) < 5:
                return
            length = self.buf[3]
            total = 4 + length + 1
            if len(self.buf) < total:
                return
            body, crc = bytes(self.buf[1:4 + length]), self.buf[4 + length]
            if crc8(body) != crc:
                self.bad_crc += 1
                if self.verbose and body[1] not in range(0xB0, 0xB8):  # guard beacons have a fixed (wrong) CRC
                    self.get_logger().warn(f"Bad CRC on frame type 0x{body[1]:02X} (total bad: {self.bad_crc})")
                del self.buf[:1]
                continue
            del self.buf[:total]
            msg_id, payload = body[1], body[3:]
            if msg_id == MSG_SET_TARGETS:
                self.on_set_targets(payload)
            elif msg_id == MSG_ALL_STOP:
                self.get_logger().warn("ALL_STOP received from firmware")
            elif msg_id == 0xC1:
                self.get_logger().info("Board: intent received and queued")
            elif msg_id == 0xC3:
                self.get_logger().warn("Board: got a frame but could not decode it")
            elif 0xB0 <= msg_id <= 0xB7:
                self.get_logger().info(
                    f"Board: stall guard cut motor {MOTOR_NAMES[msg_id - 0xB0]} "
                    "(normal with no real motor attached)")

    def on_set_targets(self, payload):
        if len(payload) < 3:
            return
        heartbeat = payload[0] | (payload[1] << 8)
        count, off = payload[2], 3
        targets = {}
        for _ in range(count):
            if off + 4 > len(payload):
                break
            motor_id = payload[off]
            raw = int.from_bytes(payload[off + 1:off + 3], "little", signed=True)
            targets[motor_id] = from_q15(raw)
            off += 4
        if self.verbose:
            txt = "  ".join(f"{MOTOR_NAMES[m] if m < len(MOTOR_NAMES) else m}={math.degrees(a):.1f}"
                            for m, a in targets.items())
            self.get_logger().info(f"RX SET_TARGETS hb={heartbeat} ({len(targets)} motors): {txt}")
        with self.lock:
            if self.last_targets is None:
                self.get_logger().info("First command received from the board")
                self.last_targets = {}
            self.last_targets.update(targets)  # a frame may contain only some motors
        if not self.board_mode:
            self.write(make_frame(MSG_HEARTBEAT_ACK, bytes([heartbeat & 0xFF, heartbeat >> 8])))

    def publish_command(self):
        with self.lock:
            targets, js = self.last_targets, self.last_js
        if targets is None or js is None:
            return
        # Start from the current position of ALL joints (controller wants them all),
        # then overwrite the joints driven by the firmware's motors.
        positions = dict(zip(js.name, js.position))
        for motor_id, angle in targets.items():
            if motor_id < len(MOTOR_TO_JOINTS):
                for joint in MOTOR_TO_JOINTS[motor_id]:
                    if joint in positions:
                        positions[joint] = angle
        point = JointTrajectoryPoint()
        point.positions = [positions[n] for n in js.name]
        point.time_from_start = Duration(sec=0, nanosec=300_000_000)  # 0.3 s
        cmd = JointTrajectory()
        cmd.joint_names = list(js.name)
        cmd.points = [point]
        self.pub.publish(cmd)

    # ---------- sim -> board ----------
    def on_joint_states(self, msg):
        with self.lock:
            self.last_js = msg

    def send_feedback(self):
        with self.lock:
            js = self.last_js
        if js is None:
            return
        entries = bytearray()
        count = 0
        for motor_id, joints in enumerate(MOTOR_TO_JOINTS):
            if joints[0] not in js.name:
                continue
            i = js.name.index(joints[0])
            pos = to_q15(js.position[i]) & 0xFFFF
            vel = to_q15(js.velocity[i] if js.velocity else 0.0) & 0xFFFF
            entries += bytes([motor_id, pos & 0xFF, pos >> 8, vel & 0xFF, vel >> 8])
            count += 1
        self.write(make_frame(MSG_JOINT_FEEDBACK, bytes([count]) + entries))

    def write(self, frame):
        with self.lock:
            self.ser.write(frame)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="/tmp/pc_side")
    ap.add_argument("--baud", type=int, default=115200)  # the board's LPUART1 runs at 115200
    ap.add_argument("--board", action="store_true", help="talking to the real STM32 board")
    ap.add_argument("--verbose", action="store_true", help="print every SET_TARGETS frame received")
    args = ap.parse_args()
    rclpy.init()
    node = SimBridge(args.port, args.baud, args.board, args.verbose)
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    node.destroy_node()
    rclpy.shutdown()


if __name__ == "__main__":
    main()
