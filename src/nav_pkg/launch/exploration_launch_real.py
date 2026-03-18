import os

from ament_index_python.packages import (
    get_package_share_directory,
    get_package_share_path,
)
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, TimerAction
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
        # FIX 1: respawn in case URDF parse fails on a slow Pi boot
        respawn=True,
        respawn_delay=3.0,
    )

    # FIX 2: Use YOUR custom rplidar launch file, not the stock rplidar_a1_launch.py
    # The stock file never received scan_mode='Sensitivity' correctly.
    lidar = Node(
        package='rplidar_ros',
        executable='rplidar_node',
        name='rplidar_node',
        output='screen',
        parameters=[{
            'channel_type':     'serial',
            'serial_port':      '/dev/ttyUSB0',
            'serial_baudrate':  115200,
            'frame_id':         'laser',
            'inverted':         False,
            'angle_compensate': True,
            'scan_mode':        'Sensitivity',  # FIX 3: was never passed before
        }],
        # FIX 4: auto-restart on USB timeout (exit code 255)
        respawn=True,
        respawn_delay=3.0,
    )

    # Arduino Mega -> /dev/ttyACM0
    odom_node = Node(
        package='robot_control_pkg',
        executable='odom_publisher',
        name='odom_publisher',
        output='screen',
        parameters=[{
            'serial_port': '/dev/ttyACM0',
            'baud_rate':   115200,
        }],
        # FIX 5: respawn if Arduino resets or USB drops
        respawn=True,
        respawn_delay=3.0,
    )

    # FIX 6: SLAM now starts 5s after lidar+odom — gives hardware time to be ready
    # Previously SLAM started at t=0 and raced against the lidar, causing the
    # "map frame does not exist" error that froze your entire Nav2 stack.
    delayed_slam = TimerAction(
        period=5.0,
        actions=[
            IncludeLaunchDescription(
                PythonLaunchDescriptionSource(
                    os.path.join(slam_pkg_share, 'launch', 'slam_real.launch.py')
                )
            )
        ],
    )

    # Nav2 navigation stack
    # FIX 7: Nav2 remaps cmd_vel so it goes THROUGH ultrasonic_safety, not directly to robot.
    # Previously Nav2 published to /cmd_vel directly, completely bypassing your safety node.
    nav2 = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(nav_pkg_share, 'launch', 'navigation_launch_real.py')
        ),
        launch_arguments={
            'cmd_vel_topic': '/cmd_vel_nav',  # Nav2 outputs here
        }.items(),
    )

    # Ultrasonic safety: reads /cmd_vel_nav, writes safe /cmd_vel to Arduino
    ultrasonic_safety = Node(
        package='nav_pkg',
        executable='ultrasonic_safety',
        name='ultrasonic_safety',
        output='screen',
        parameters=[{
            'nav_cmd_topic':    '/cmd_vel_nav',
            'output_cmd_topic': '/cmd_vel',
            'ultrasonic_topic': '/ultrasonic',
            'stop_distance':    0.3,
            'recovery_ang_vel': 0.4,
        }],
        respawn=True,
        respawn_delay=2.0,
    )

    # Frontier exploration
    explore_node = Node(
        package='explore_lite',
        executable='explore',
        name='explore',
        output='screen',
        parameters=[{
            'robot_base_frame':      'base_link',
            'costmap_topic':         '/global_costmap/costmap',
            'costmap_updates_topic': '/global_costmap/costmap_updates',
            'visualize':             True,
            'planner_frequency':     0.5,
            'progress_timeout':      30.0,
            'potential_scale':       3.0,
            'gain_scale':            1.0,
            'transform_tolerance':   0.5,
            'min_frontier_size':     0.5,
            'use_sim_time':          False,
        }],
        remappings=[
            ('/map',       '/map'),
            ('/tf',        '/tf'),
            ('/tf_static', '/tf_static'),
        ],
    )

    # FIX 8: Delay increased from 12s → 20s.
    # On a Pi with undervoltage, SLAM needs ~15s to build enough map for
    # the NavFn planner to find a valid path from (0,0).
    # 20s gives SLAM a comfortable margin after its own 5s delayed start.
    delayed_nav_stack = TimerAction(
        period=20.0,
        actions=[nav2, ultrasonic_safety, explore_node],
    )

    return LaunchDescription([
        odom_node,        # t=0  — Arduino serial bridge
        robot_state_pub,  # t=0  — publishes TF from URDF
        lidar,            # t=0  — RPLidar starts scanning
        delayed_slam,     # t=5  — SLAM starts once scan+odom exist
        delayed_nav_stack,# t=20 — Nav2+safety+explore start once map exists
    ])