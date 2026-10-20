#!/usr/bin/env python3

'''Moves the prosthesis near the object as if the user wanted to grab it based
on the received position and orientation of the objet'''

import rclpy
from rclpy.node import Node
from trajectory_msgs.msg import JointTrajectory
from std_msgs.msg import Int8


class CvArmMoverNode(Node):

    def __init__(self):
        super().__init__('cv_arm_mover')

        self.base_pose = None
        self.hand_pose = None

        self.received_base_pose = False
        self.received_hand_pose = False

        self.base_timer = None
        self.hand_timer = None

        self.cv_base_subscription_ = self.create_subscription(
            JointTrajectory,
            '/cv/base/pose',
            self.cv_base_callback,
            10
        )

        self.cv_hand_subscription_ = self.create_subscription(
            Int8,
            '/cv/hand/pose',
            self.cv_hand_callback,
            10
        )

        self.base_publisher_ = self.create_publisher(
            JointTrajectory,
            '/joint_trajectory_controller/joint_trajectory',
            10
        )

        self.hand_publisher_ = self.create_publisher(
            Int8,
            '/pose_goals',
            10
        )

        self.get_logger().info("cv_arm_mover node initialized")

    def cv_base_callback(self, msg):
        
        self.base_pose = msg
        self.received_base_pose = True

        self.try_send_commands()

    def cv_hand_callback(self, msg):
        self.hand_pose = msg
        self.received_hand_pose = True

        self.try_send_commands()

    def try_send_commands(self):

        # Wait until both messages are received
        if not (self.received_base_pose and self.received_hand_pose):
            return

        # Create timers only once
        if self.base_timer is None:
            self.base_timer = self.create_timer(3.0, self.publish_base_pose)
            self.get_logger().info("first timer started")
        if self.hand_timer is None:
            self.hand_timer = self.create_timer(4.0, self.publish_hand_pose)
            self.get_logger().info("second timer started")

    def publish_hand_pose(self):

        self.get_logger().info("Publishing hand pose")
        self.hand_publisher_.publish(self.hand_pose)
        self.hand_timer.cancel()
    
    def publish_base_pose(self):
        
        self.get_logger().info("Publishing base trajectory")
        self.base_publisher_.publish(self.base_pose)
        self.base_timer.cancel()

def main():
    rclpy.init()
    node = CvArmMoverNode()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()


if __name__ == '__main__':
    main()