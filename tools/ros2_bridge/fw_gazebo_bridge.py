#!/usr/bin/env python3
"""Show the CURRENT firmware (SIM v2) moving the Gazebo hand.

Two programs, so ROS can never slow down the firmware's timing loop:
  terminal A:  python3 tools/sim_fingers.py --gazebo
               firmware C (this PC, or a board with --port) + rotor model + console
  terminal B:  python3 tools/ros2_bridge/fw_gazebo_bridge.py   (this file, ROS only)
               rotor angles (UDP 48170) -> JointTrajectory -> Gazebo hand
               /firmware_intent (Int8)  -> commands back to the console (UDP 48171)

Gazebo only DISPLAYS the rotor angles (position controller): it is not a physics or
force test of the hand. Type commands in terminal A, or from any terminal:
    ros2 topic pub --once /firmware_intent std_msgs/msg/Int8 "{data: 1}"
    (0 open, 1 close, 2 pinch, 3 wrist right, 4 wrist left,
     10..17 motor N to 30 deg, 20..27 motor N back to 0 deg)
"""
import argparse
import json
import socket
import threading

GAZEBO_PORT = 48170
MOTORS = ('thumb', 'index', 'middle', 'ring', 'little', 'wrist_x', 'wrist_y', 'palm')
INTENTS = ('OPEN_HAND', 'CLOSE_HAND', 'PINCH', 'ROTATE_WRIST_R', 'ROTATE_WRIST_L')
# Firmware motor order (motor_id_t) -> Gazebo joints driven by that motor (same
# mapping as sim_bridge_v2.py). The thumb/palm split is still provisional.
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
# URDF limits (prosthesis.urdf.xacro): fingers/thumb/palm 0..1.57, wrist -1.57..1.57.
LIMITS = [(0.0, 1.57)] * 5 + [(-1.57, 1.57)] * 2 + [(0.0, 1.57)]


def joint_targets(positions):
    """Rotor angles (rad) -> (joint names, clamped joint positions)."""
    names, values = [], []
    for motor, joints in enumerate(MOTOR_TO_JOINTS):
        low, high = LIMITS[motor]
        angle = min(high, max(low, float(positions[motor])))
        for joint in joints:
            names.append(joint); values.append(angle)
    return names, values


def intent_text(value):
    """/firmware_intent numbers (same as sim_bridge_v2.py) -> console command."""
    if 0 <= value < len(INTENTS):
        return f'intent {INTENTS[value]}'
    if 10 <= value <= 17:
        return f'finger {MOTORS[value - 10]} 30'
    if 20 <= value <= 27:
        return f'finger {MOTORS[value - 20]} 0'
    return None


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--topic', default='/joint_trajectory_controller/joint_trajectory')
    parser.add_argument('--rate', type=float, default=20.0, help='Gazebo commands per second')
    args = parser.parse_args()

    import rclpy
    from rclpy.node import Node
    from std_msgs.msg import Int8
    from trajectory_msgs.msg import JointTrajectory, JointTrajectoryPoint
    from builtin_interfaces.msg import Duration

    latest = {'position': None}
    receiver = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    receiver.bind(('127.0.0.1', GAZEBO_PORT))

    def receive():
        while True:
            data, _ = receiver.recvfrom(4096)
            try:
                position = json.loads(data)['position']
                if len(position) == 8:
                    latest['position'] = position
            except (ValueError, KeyError, TypeError):
                continue
    threading.Thread(target=receive, daemon=True).start()
    sender = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    rclpy.init()
    node = Node('fw_gazebo_bridge')
    publisher = node.create_publisher(JointTrajectory, args.topic, 10)
    period = 1.0 / args.rate
    reach_ns = int(period * 2e9)  # reach each sample within two publish periods
    state = {'warned': False}

    def publish():
        if latest['position'] is None:
            if not state['warned']:
                node.get_logger().info('Waiting for rotor angles: is "sim_fingers.py --gazebo" running?')
                state['warned'] = True
            return
        names, values = joint_targets(latest['position'])
        point = JointTrajectoryPoint()
        point.positions = values
        point.time_from_start = Duration(sec=reach_ns // 1_000_000_000, nanosec=reach_ns % 1_000_000_000)
        message = JointTrajectory()
        message.joint_names = names
        message.points = [point]
        publisher.publish(message)

    def on_intent(msg):
        text = intent_text(msg.data)
        if text is None:
            node.get_logger().warn(f'Unknown /firmware_intent {msg.data}: use 0-4, 10-17 or 20-27')
            return
        sender.sendto(text.encode(), ('127.0.0.1', GAZEBO_PORT + 1))
        node.get_logger().info(f'Sent to firmware console: {text}')

    node.create_timer(period, publish)
    node.create_subscription(Int8, '/firmware_intent', on_intent, 10)
    node.get_logger().info(f'Rotor angles (UDP {GAZEBO_PORT}) -> {args.topic} at {args.rate:g} Hz')
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        if rclpy.ok():
            rclpy.shutdown()


if __name__ == '__main__':
    main()