#!/usr/bin/env python3

import rclpy
from rclpy.node import Node
from sensor_msgs.msg import Imu
from nav_msgs.msg import Odometry
from geometry_msgs.msg import TransformStamped
from std_msgs.msg import String, Float32
import re
import tf2_ros
import serial
import math


class OdomPublisher(Node):
    def __init__(self):
        super().__init__('odom_publisher')
        self.declare_parameter('serial_port', '/dev/ttyACM0')
        self.declare_parameter('baud_rate', 115200)
        self.declare_parameter('command_topic', '/arduino_command')

        serial_port = self.get_parameter('serial_port').get_parameter_value().string_value
        baud_rate = self.get_parameter('baud_rate').get_parameter_value().integer_value
        command_topic = self.get_parameter('command_topic').get_parameter_value().string_value

        self.odom_pub = self.create_publisher(Odometry, '/odom', 10)
        self.imu_pub = self.create_publisher(Imu, '/imu/data', 10)
        self.ultra_pub = self.create_publisher(Float32, '/ultrasonic', 10)
        self.tf_broadcaster = tf2_ros.TransformBroadcaster(self)
        self.subscription = self.create_subscription(
            String, command_topic, self.cmd_callback, 10)
        self.ser = serial.Serial(serial_port, int(baud_rate), timeout=1)
        self.get_logger().info(
            f"Arduino serial: port={serial_port} baud={baud_rate} topic={command_topic}"
        )
        self.timer = self.create_timer(0.05, self.read_serial)

        # State for IMU-based velocity integration
        self.last_time = self.get_clock().now()
        self.vx_imu = 0.0

    def cmd_callback(self, msg):
        self.ser.write(msg.data.encode())
        self.get_logger().info(f"Sent to Arduino: {msg.data}")

    def read_serial(self):
        if self.ser.in_waiting:
            try:
                line = self.ser.readline().decode(errors='ignore').strip()
                # Odometry line from Arduino starts with 'o ' followed by 9 floats,
                # but the Arduino may also print extra debug text on the same line.
                if line.startswith('o '):
                    # Extract numeric fields robustly: first 8 float-like tokens
                    nums = re.findall(r"[-+]?\d*\.?\d+(?:[eE][-+]?\d+)?", line)
                    # Arduino prints: x, y, theta, linearVel, angularVel, gyro_z, accel_x, accel_y
                    if len(nums) >= 8:
                        x, y, theta = map(float, nums[0:3])
                        vx, wz = map(float, nums[3:5])
                        gz, ax, ay = map(float, nums[5:8])
                        self.publish_data(x, y, theta, vx, wz, gz, ax, ay)
                    else:
                        self.get_logger().warn(f"Bad odom line: {line}")
                elif line.startswith('u '):
                    # Ultrasonic line: 'u <distance>'
                    nums = re.findall(r"[-+]?\d*\.?\d+(?:[eE][-+]?\d+)?", line)
                    if nums:
                        try:
                            dist = float(nums[0])
                            msg = Float32()
                            msg.data = dist
                            self.ultra_pub.publish(msg)
                        except ValueError:
                            self.get_logger().warn(f"Bad ultrasonic line: {line}")
            except Exception as e:
                self.get_logger().error(f"Serial Error: {e}")

    def publish_data(self, x, y, theta, vx, wz, gz, ax, ay):
        now_time = self.get_clock().now()
        dt = (now_time - self.last_time).nanoseconds / 1e9
        if dt <= 0.0:
            dt = 1e-3
        self.last_time = now_time

        # Integrate IMU linear acceleration to estimate forward speed
        self.vx_imu += ax * dt

        now = now_time.to_msg()

        t = TransformStamped()
        t.header.stamp = now
        t.header.frame_id = 'odom'
        t.child_frame_id = 'base_footprint_link'
        t.transform.translation.x = x
        t.transform.translation.y = y
        t.transform.rotation.z = math.sin(theta / 2)
        t.transform.rotation.w = math.cos(theta / 2)
        self.tf_broadcaster.sendTransform(t)

        odom = Odometry()
        odom.header.stamp = now
        odom.header.frame_id = 'odom'
        odom.child_frame_id = 'base_footprint_link'
        odom.pose.pose.position.x = x
        odom.pose.pose.position.y = y
        odom.pose.pose.orientation.z = math.sin(theta / 2)
        odom.pose.pose.orientation.w = math.cos(theta / 2)
        # Use IMU-derived speed and gyro yaw rate
        odom.twist.twist.linear.x = self.vx_imu
        odom.twist.twist.angular.z = gz
        self.odom_pub.publish(odom)

        imu = Imu()
        imu.header.stamp = now
        imu.header.frame_id = 'imu_link'
        imu.linear_acceleration.x = ax
        imu.linear_acceleration.y = ay
        imu.linear_acceleration.z = 9.81
        imu.angular_velocity.z = gz
        imu.angular_velocity_covariance[8] = 0.001
        imu.linear_acceleration_covariance[0] = 0.01
        imu.linear_acceleration_covariance[4] = 0.01
        self.imu_pub.publish(imu)

def main():
    rclpy.init()
    rclpy.spin(OdomPublisher())

if __name__ == '__main__':
    main()