import os

from ament_index_python.packages import (
    get_package_share_directory,
    get_package_share_path,
)
from launch import LaunchDescription
from launch.actions import ExecuteProcess, IncludeLaunchDescription, TimerAction
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import Command
from launch_ros.actions import Node
from launch_ros.parameter_descriptions import ParameterValue


def generate_launch_description():
    nav_pkg_share = get_package_share_directory('nav_pkg')
    slam_pkg_share = get_package_share_directory('slam_pkg')
    urdf_path = get_package_share_path('robot_control_pkg') / 'urdf' / 'robot_argo.urdf'
    robot_description = {
        'robot_description': ParameterValue(
            Command(['xacro ', str(urdf_path)]), value_type=str
        )
    }
    robot_state_pub = Node(
        package='robot_state_publisher',
        executable='robot_state_publisher',
        name='robot_state_publisher',
        output='screen',
        parameters=[robot_description],
    )

    # LiDAR: CP210x (Silicon Labs) -> /dev/ttyUSB0 on the Pi
    lidar = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(
                get_package_share_directory('rplidar_ros'),
                'launch',
                'rplidar_a1_launch.py',
            )
        ),
        launch_arguments={
            'serial_port': '/dev/ttyUSB0',
            'serial_baudrate': '115200',
        }.items(),
    )

    # SLAM Toolbox for real robot
    slam = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(slam_pkg_share, 'launch', 'slam_real.launch.py')
        )
    )

    # Nav2 navigation stack for real robot
    nav2 = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(nav_pkg_share, 'launch', 'navigation_launch_real.py')
        )
    )

    # Arduino Mega 2560 (CDC ACM) -> /dev/ttyACM0
    odom_node = Node(
        package='robot_control_pkg',
        executable='odom_publisher',
        name='odom_publisher',
        output='screen',
        parameters=[
            {
                'serial_port': '/dev/ttyACM0',
                'baud_rate': 115200,
            }
        ],
    )

    # Frontier exploration using explore_lite
    explore_node = Node(
        package='explore_lite',
        executable='explore',
        name='explore',
        output='screen',
        parameters=[
            {
                'robot_base_frame': 'base_link',
                'costmap_topic': '/global_costmap/costmap',
                'costmap_updates_topic': '/global_costmap/costmap_updates',
                'visualize': True,
                'planner_frequency': 0.5,
                'progress_timeout': 30.0,
                'potential_scale': 3.0,
                'gain_scale': 1.0,
                'transform_tolerance': 0.5,
                'min_frontier_size': 0.5,
                'use_sim_time': False,
            }
        ],
        remappings=[
            ('/map', '/map'),
            ('/tf', '/tf'),
            ('/tf_static', '/tf_static'),
        ],
    )

    # Ensure slam_toolbox is explicitly configured and activated.
    slam_configure = TimerAction(
        period=4.0,
        actions=[
            ExecuteProcess(
                cmd=['ros2', 'lifecycle', 'set', '/slam_toolbox', 'configure'],
                output='screen',
            )
        ],
    )

    slam_activate = TimerAction(
        period=6.0,
        actions=[
            ExecuteProcess(
                cmd=['ros2', 'lifecycle', 'set', '/slam_toolbox', 'activate'],
                output='screen',
            )
        ],
    )

    # Start Nav2 and frontier after TF + SLAM have had time to initialize.
    delayed_navigation_stack = TimerAction(
        period=12.0,
        actions=[nav2, explore_node],
    )

    return LaunchDescription(
        [
            odom_node,
            robot_state_pub,
            lidar,
            slam,
            slam_configure,
            slam_activate,
            delayed_navigation_stack,
        ]
    )

