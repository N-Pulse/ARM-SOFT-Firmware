#!/usr/bin/env python3

"""
Trajectory control node (handling the fingers dynamic) which subscribes to /trajectory_control and sends goals to the trajectory controller
"""
import rclpy
from rclpy.node import Node
from sensor_msgs.msg import JointState
from std_msgs.msg import Int8
from trajectory_msgs.msg import JointTrajectory, JointTrajectoryPoint
from builtin_interfaces.msg import Duration

class TrajectoryControlNode(Node):
    
    def __init__(self):
        super().__init__('trajectory_control')
        self.timer = None
        self.last_joint_states = None
        self.joint_states_subscription_ = self.create_subscription(
            JointState, '/joint_states', self.joint_states_callback, 10
        )
        
        self.arm_movement_subscription_ = self.create_subscription(
            JointTrajectory,
            'arm_delta_goals',
            self.handle_arm_delta_goals,
            10
        )
        
        self.pose_subscription_ = self.create_subscription(
            Int8,
            '/pose_goals',
            self.handle_pose_goals,
            10
        )
        
        self.trajectory_pub = self.create_publisher(
            JointTrajectory,
            '/joint_trajectory_controller/joint_trajectory',
            10
        )

        self.get_logger().info("Trajectory_control node initialized")

    def joint_states_callback(self, msg):
        self.last_joint_states = msg
        
        self.wrist_indexes = []
        self.thumb_indexes = []
        self.index_indexes = []
        self.middle_indexes = []
        self.ring_indexes = []
        self.little_indexes = []
        for i in range(len(self.last_joint_states.name)) :
            if 'wrist' in self.last_joint_states.name[i]:
                self.wrist_indexes.append(i)
            if 'thumb' in self.last_joint_states.name[i]:
                self.thumb_indexes.append(i)
            if 'index' in self.last_joint_states.name[i]:
                self.index_indexes.append(i)
            if 'middle' in self.last_joint_states.name[i]:
                self.middle_indexes.append(i)
            if 'ring' in self.last_joint_states.name[i]:
                self.ring_indexes.append(i)
            if 'little' in self.last_joint_states.name[i]:
                self.little_indexes.append(i)

    def send_trajectory(self, trajectory : JointTrajectory, goal_name="Goal"):
        """Publish trajectory directly to controller """    
        self.trajectory_pub.publish(trajectory)
        self.get_logger().info(f"Published: {goal_name}")

    #----------PREDEFINDED HAND POSES (BATCH/PARALLEL EXECUTION)-------------------------
    
    def hand_pose_0(self):
        """open hand - all fingers move in parallel"""
        trajectory = JointTrajectory()
        point_1 = JointTrajectoryPoint()
        point_1.time_from_start = Duration(sec=0, nanosec=int(2e8))

        for i in self.index_indexes:
            trajectory.joint_names.append(self.last_joint_states.name[i])
            point_1.positions.append(0)
        for i in self.middle_indexes:
            trajectory.joint_names.append(self.last_joint_states.name[i])
            point_1.positions.append(0)
        for i in self.ring_indexes:
            trajectory.joint_names.append(self.last_joint_states.name[i])
            point_1.positions.append(0)
        for i in self.little_indexes:
            trajectory.joint_names.append(self.last_joint_states.name[i])
            point_1.positions.append(0)
        # Thumb X
        trajectory.joint_names.append(self.last_joint_states.name[self.thumb_indexes[0]])
        point_1.positions.append(0)
        # Thumb Y 
        for i in self.thumb_indexes[1::]:
            trajectory.joint_names.append(self.last_joint_states.name[i])
            point_1.positions.append(0)
        
        trajectory.points.append(point_1)
        self.send_trajectory(trajectory, goal_name="Hand Open")

    def hand_pose_1(self):
        """close hand - all fingers move in parallel"""
        trajectory = JointTrajectory()
        
        # Add all joint names first
        for i in self.index_indexes:
            trajectory.joint_names.append(self.last_joint_states.name[i])
        for i in self.middle_indexes:
            trajectory.joint_names.append(self.last_joint_states.name[i])
        for i in self.ring_indexes:
            trajectory.joint_names.append(self.last_joint_states.name[i])
        for i in self.little_indexes:
            trajectory.joint_names.append(self.last_joint_states.name[i])
        trajectory.joint_names.append(self.last_joint_states.name[self.thumb_indexes[0]])
        
        # Point 1: Fingers close, thumb stays neutral
        point_1 = JointTrajectoryPoint()
        point_1.time_from_start = Duration(sec=0, nanosec=int(2e8))
        for i in self.index_indexes:
            point_1.positions.append(1.2)
        for i in self.middle_indexes:
            point_1.positions.append(1.2)
        for i in self.ring_indexes:
            point_1.positions.append(1.2)
        for i in self.little_indexes:
            point_1.positions.append(1.2)
        point_1.positions.append(0)  # Thumb X stays neutral
        trajectory.points.append(point_1)
        
        # Point 2: Fingers closed, thumb closes
        point_2 = JointTrajectoryPoint()
        point_2.time_from_start = Duration(sec=0, nanosec=int(6e8))
        for i in self.index_indexes:
            point_2.positions.append(1.2)
        for i in self.middle_indexes:
            point_2.positions.append(1.2)
        for i in self.ring_indexes:
            point_2.positions.append(1.2)
        for i in self.little_indexes:
            point_2.positions.append(1.2)
        point_2.positions.append(1.0)  # Thumb X closes
        trajectory.points.append(point_2)

        self.send_trajectory(trajectory, goal_name="Hand grasp")


    def hand_pose_2(self):
        """pinch - index and thumb close, others open (parallel execution)"""
        trajectory = JointTrajectory()
        point_1 = JointTrajectoryPoint()
        point_1.time_from_start = Duration(sec=0, nanosec=int(2e8))
        for i in self.index_indexes:
            trajectory.joint_names.append(self.last_joint_states.name[i])
            point_1.positions.append(1.)
        # Thumb Y
        for i in self.thumb_indexes[1::]:
            trajectory.joint_names.append(self.last_joint_states.name[i])
            point_1.positions.append(0.6)
        for i in self.middle_indexes:
            trajectory.joint_names.append(self.last_joint_states.name[i])
            point_1.positions.append(0)
        for i in self.ring_indexes:
            trajectory.joint_names.append(self.last_joint_states.name[i])
            point_1.positions.append(0)
        for i in self.little_indexes:
            trajectory.joint_names.append(self.last_joint_states.name[i])
            point_1.positions.append(0)
        trajectory.points.append(point_1)
        self.send_trajectory(trajectory, goal_name="Pinch")

    def hand_pose_3(self):
        """rotate wrist right"""
        trajectory = JointTrajectory()
        point_1 = JointTrajectoryPoint()
        point_1.time_from_start = Duration(sec=0, nanosec=int(2e8))
        trajectory.joint_names.append(self.last_joint_states.name[self.wrist_indexes[0]])
        point_1.positions.append(1.57)
        trajectory.points.append(point_1)
        self.send_trajectory(trajectory, goal_name="Wrist 90 deg to right")

    def hand_pose_4(self):
        """rotate wrist left"""
        trajectory = JointTrajectory()
        point_1 = JointTrajectoryPoint()
        point_1.time_from_start = Duration(sec=0, nanosec=int(2e8))
        trajectory.joint_names.append(self.last_joint_states.name[self.wrist_indexes[0]])
        point_1.positions.append(-1.57)
        trajectory.points.append(point_1)
        self.send_trajectory(trajectory, goal_name="Wrist 90 deg to left")

    def hand_pose_5(self):
        """rotate wrist up"""
        trajectory = JointTrajectory()
        point_1 = JointTrajectoryPoint()
        point_1.time_from_start = Duration(sec=0, nanosec=int(2e8))
        trajectory.joint_names.append(self.last_joint_states.name[self.wrist_indexes[1]])
        point_1.positions.append(1.57)
        trajectory.points.append(point_1)
        self.send_trajectory(trajectory, goal_name="Wrist 90 deg up")

    def hand_pose_6(self):
        """rotate wrist down"""
        trajectory = JointTrajectory()
        point_1 = JointTrajectoryPoint()
        point_1.time_from_start = Duration(sec=0, nanosec=int(2e8))
        trajectory.joint_names.append(self.last_joint_states.name[self.wrist_indexes[1]])
        point_1.positions.append(-1.57)
        trajectory.points.append(point_1)
        self.send_trajectory(trajectory, goal_name="Wrist 90 deg down")
    #------------------------------------------------------

    def handle_arm_delta_goals(self, msg):
        trajectory = JointTrajectory()
        trajectory.joint_names = msg.joint_names
        first_trajectory_point = JointTrajectoryPoint()
        msg_index = 0
        for joint_name in msg.joint_names :
            joint_states_index = self.last_joint_states.name.index(joint_name)
            last_position = self.last_joint_states.position[joint_states_index]
            delta = msg.points[0].positions[msg_index]
            new_position = last_position + delta
            first_trajectory_point.positions.append(new_position)
            msg_index += 1

        first_trajectory_point.time_from_start = Duration(sec=0, nanosec=0)
        trajectory.points.append(first_trajectory_point)
        self.send_trajectory(trajectory, 'Arm_delta')

    def handle_pose_goals(self, msg):
        poses = {
            0: self.hand_pose_0,
            1: self.hand_pose_1,
            2: self.hand_pose_2,
            3: self.hand_pose_3,
            4: self.hand_pose_4,
            5: self.hand_pose_5,
            6: self.hand_pose_6
        }
        if msg.data in poses:
            poses[msg.data]()

def main():
    rclpy.init()
    node = TrajectoryControlNode()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()


if __name__ == '__main__':
    main()
