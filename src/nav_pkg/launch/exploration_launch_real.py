import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.actions import Node


def generate_launch_description():
    nav_pkg_share = get_package_share_directory('nav_pkg')

    # LiDAR driver (assumes rplidar_ros package and A1 launch)
    lidar = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(
                get_package_share_directory('rplidar_ros'),
                'launch',
                'rplidar_a1_launch.py',
            )
        )
    )

    # SLAM Toolbox for real robot
    slam = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(nav_pkg_share, 'launch', 'slam_real.launch.py')
        )
    )

    # Nav2 navigation stack for real robot
    nav2 = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(nav_pkg_share, 'launch', 'navigation_launch_real.py')
        )
    )

    # Dead-reckoning / IMU-based odometry (from robot_control_pkg)
    odom_node = Node(
        package='robot_control_pkg',
        executable='odom_publisher',
        name='odom_publisher',
        output='screen',
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

