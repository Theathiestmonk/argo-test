import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.actions import Node


def generate_launch_description():
    nav_pkg_share = get_package_share_directory('nav_pkg')
    slam_pkg_share = get_package_share_directory('slam_pkg')

    # LiDAR: CP210x (Silicon Labs) -> /dev/ttyUSB1 on the Pi
    lidar = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(
                get_package_share_directory('rplidar_ros'),
                'launch',
                'rplidar_a1_launch.py',
            )
        ),
        launch_arguments={
            'serial_port': '/dev/ttyUSB1',
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

    # Ultrasonic safety layer: /cmd_vel_nav -> /cmd_vel
    ultrasonic_safety = Node(
        package='nav_pkg',
        executable='ultrasonic_safety',
        name='ultrasonic_safety',
        output='screen',
        parameters=[
            {
                'nav_cmd_topic': '/cmd_vel_nav',
                'output_cmd_topic': '/cmd_vel',
                'ultrasonic_topic': '/ultrasonic',
                'stop_distance': 0.3,
                'recovery_ang_vel': 0.4,
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
                'planner_frequency': 0.5,
                'progress_timeout': 30.0,
                'visualize': True,
            }
        ],
        remappings=[
            ('/map', '/map'),
            ('/tf', '/tf'),
            ('/tf_static', '/tf_static'),
        ],
    )

    return LaunchDescription(
        [
            lidar,
            slam,
            nav2,
            odom_node,
            ultrasonic_safety,
            explore_node,
        ]
    )

