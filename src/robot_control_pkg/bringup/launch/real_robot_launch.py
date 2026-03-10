import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch_ros.actions import Node
from launch.actions import IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource

def generate_launch_description():

    pkg_share = get_package_share_directory('robot_control_pkg')

    # Launch diffbot (ros2_control + robot state publisher)
    diffbot = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(pkg_share, 'launch', 'diffbot.launch.py')
        )
    )

    # Launch LiDAR
    lidar = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(
                get_package_share_directory('rplidar_ros'),
                'launch',
                'rplidar_a1_launch.py'
            )
        )
    )

    # Dead reckoning odom node
    odom_node = Node(
        package='robot_control_pkg',
        executable='odom_publisher.py',
        name='odom_publisher',
        output='screen'
    )

    # IMU node
    imu_node = Node(
        package='robot_control_pkg',
        executable='imu_publisher.py',
        name='imu_publisher',
        output='screen'
    )

    # EKF fusion node
    ekf_node = Node(
        package='robot_localization',
        executable='ekf_node',
        name='ekf_filter_node',
        output='screen',
        parameters=[os.path.join(pkg_share, 'config', 'ekf.yaml')]
    )

    return LaunchDescription([
        diffbot,
        lidar,
        odom_node,
        imu_node,
        ekf_node,
    ])