import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    # Frames
    base_frame = LaunchConfiguration("base_frame")
    lidar_frame = LaunchConfiguration("lidar_frame")

    # Static TF base -> lidar (meters, radians)
    lidar_x = LaunchConfiguration("lidar_x")
    lidar_y = LaunchConfiguration("lidar_y")
    lidar_z = LaunchConfiguration("lidar_z")
    lidar_roll = LaunchConfiguration("lidar_roll")
    lidar_pitch = LaunchConfiguration("lidar_pitch")
    lidar_yaw = LaunchConfiguration("lidar_yaw")

    # LiDAR serial
    lidar_serial_port = LaunchConfiguration("lidar_serial_port")
    lidar_serial_baudrate = LaunchConfiguration("lidar_serial_baudrate")

    # Arduino serial (for keyboard teleop bridge)
    arduino_serial_port = LaunchConfiguration("arduino_serial_port")
    arduino_baud_rate = LaunchConfiguration("arduino_baud_rate")

    declare_args = [
        DeclareLaunchArgument("base_frame", default_value="base_footprint_link"),
        DeclareLaunchArgument("lidar_frame", default_value="laser"),
        DeclareLaunchArgument("lidar_x", default_value="0.0"),
        DeclareLaunchArgument("lidar_y", default_value="0.0"),
        DeclareLaunchArgument("lidar_z", default_value="0.2"),
        DeclareLaunchArgument("lidar_roll", default_value="0.0"),
        DeclareLaunchArgument("lidar_pitch", default_value="0.0"),
        DeclareLaunchArgument("lidar_yaw", default_value="0.0"),
        DeclareLaunchArgument("lidar_serial_port", default_value="/dev/ttyUSB0"),
        DeclareLaunchArgument("lidar_serial_baudrate", default_value="115200"),
        DeclareLaunchArgument("arduino_serial_port", default_value="/dev/ttyACM0"),
        DeclareLaunchArgument("arduino_baud_rate", default_value="115200"),
    ]

    # RPLidar A1 driver
    rplidar_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(
                get_package_share_directory("rplidar_ros"),
                "launch",
                "rplidar_a1_launch.py",
            )
        ),
        launch_arguments={
            "serial_port": lidar_serial_port,
            "serial_baudrate": lidar_serial_baudrate,
            "frame_id": lidar_frame,
        }.items(),
    )

    # Static TF: base_frame -> lidar_frame
    static_tf = Node(
        package="tf2_ros",
        executable="static_transform_publisher",
        name="base_to_lidar_tf",
        arguments=[
            lidar_x,
            lidar_y,
            lidar_z,
            lidar_roll,
            lidar_pitch,
            lidar_yaw,
            base_frame,
            lidar_frame,
        ],
        output="screen",
    )

    # Arduino bridge + open-loop odom publisher (subscribes /arduino_command)
    odom_bridge = Node(
        package="robot_control_pkg",
        executable="odom_publisher",
        name="odom_publisher",
        parameters=[
            {
                "serial_port": arduino_serial_port,
                "baud_rate": arduino_baud_rate,
                "command_topic": "/arduino_command",
            }
        ],
        output="screen",
    )

    # SLAM toolbox mapping (async)
    slam_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(
                get_package_share_directory("slam_pkg"),
                "launch",
                "slam_real.launch.py",
            )
        )
    )

    return LaunchDescription(
        declare_args
        + [
            rplidar_launch,
            static_tf,
            odom_bridge,
            slam_launch,
        ]
    )

