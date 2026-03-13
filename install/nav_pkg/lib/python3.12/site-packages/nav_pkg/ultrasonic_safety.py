#!/usr/bin/env python3

import rclpy
from rclpy.node import Node
from geometry_msgs.msg import Twist
from std_msgs.msg import Float32


class UltrasonicSafety(Node):
    def __init__(self):
        super().__init__('ultrasonic_safety')

        self.declare_parameter('nav_cmd_topic', '/cmd_vel_nav')
        self.declare_parameter('output_cmd_topic', '/cmd_vel')
        self.declare_parameter('ultrasonic_topic', '/ultrasonic')
        self.declare_parameter('stop_distance', 0.3)
        self.declare_parameter('recovery_ang_vel', 0.4)

        nav_cmd_topic = self.get_parameter(
            'nav_cmd_topic').get_parameter_value().string_value
        output_cmd_topic = self.get_parameter(
            'output_cmd_topic').get_parameter_value().string_value
        ultrasonic_topic = self.get_parameter(
            'ultrasonic_topic').get_parameter_value().string_value

        self.stop_distance = (
            self.get_parameter('stop_distance')
            .get_parameter_value()
            .double_value
        )
        self.recovery_ang_vel = (
            self.get_parameter('recovery_ang_vel')
            .get_parameter_value()
            .double_value
        )

        self.current_nav_cmd = Twist()
        self.current_range = float('inf')

        self.cmd_sub = self.create_subscription(
            Twist, nav_cmd_topic, self.nav_cmd_callback, 10
        )
        self.ultra_sub = self.create_subscription(
            Float32, ultrasonic_topic, self.ultrasonic_callback, 10
        )
        self.cmd_pub = self.create_publisher(Twist, output_cmd_topic, 10)

        timer_period = 0.05
        self.timer = self.create_timer(timer_period, self.control_loop)

        self.get_logger().info(
            f'UltrasonicSafety listening on {nav_cmd_topic} and '
            f'{ultrasonic_topic}, publishing safe cmd to {output_cmd_topic}'
        )

    def nav_cmd_callback(self, msg: Twist) -> None:
        self.current_nav_cmd = msg

    def ultrasonic_callback(self, msg: Float32) -> None:
        self.current_range = msg.data

    def control_loop(self) -> None:
        safe_cmd = Twist()

        if self.current_range < self.stop_distance:
            safe_cmd.linear.x = 0.0
            safe_cmd.angular.z = self.recovery_ang_vel
        else:
            safe_cmd = self.current_nav_cmd

        self.cmd_pub.publish(safe_cmd)


def main(args=None):
    rclpy.init(args=args)
    node = UltrasonicSafety()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()

