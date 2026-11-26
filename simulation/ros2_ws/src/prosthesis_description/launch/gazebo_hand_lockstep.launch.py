import os
import xacro
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.actions import Node


def generate_launch_description():
    share = get_package_share_directory('prosthesis_description')
    config = os.path.join(share, 'config', 'prosthesis_hand_lockstep.yaml')
    description = xacro.process_file(os.path.join(share, 'model', 'prosthesis_hand_lockstep.urdf.xacro')).toxml()
    publisher = Node(package='robot_state_publisher', executable='robot_state_publisher',
                     parameters=[{'robot_description': description, 'use_sim_time': True}], output='screen')
    gazebo = IncludeLaunchDescription(PythonLaunchDescriptionSource(os.path.join(
        get_package_share_directory('ros_gz_sim'), 'launch', 'gz_sim.launch.py')),
        launch_arguments={'gz_args': '-r ' + os.path.join(share, 'worlds', 'prosthesis_hand_zero_gravity.sdf')}.items())
    spawn = Node(package='ros_gz_sim', executable='create',
                 arguments=['-topic', 'robot_description', '-name', 'prosthesis',
                            '-x', '0.001', '-y', '-0.002', '-z', '0.065'],
                 parameters=[{'use_sim_time': True}], output='screen')
    bridge = Node(package='ros_gz_bridge', executable='parameter_bridge',
                  arguments=['--ros-args', '-p', 'config_file:=' + os.path.join(share, 'config', 'gz_bridge.yaml')],
                  parameters=[{'use_sim_time': True}], output='screen')

    def controller(name, inactive=False):
        args = [name, '--controller-manager', '/controller_manager', '--param-file', config,
                '--controller-manager-timeout', '120']
        if inactive:
            args.append('--inactive')
        return Node(package='controller_manager', executable='spawner', arguments=args, output='screen')

    # No trajectory_control.py, no original full-hand position controller.
    # Effort controller starts with zero action until a fresh enabled run arrives.
    return LaunchDescription([publisher, gazebo, spawn, bridge,
        controller('joint_state_broadcaster'),
        controller('fw_hand_controller')])
