#!/usr/bin/env python3

import rclpy
from rclpy.node import Node
from geometry_msgs.msg import Twist
import serial


class CmdVelToSerial(Node):
    def __init__(self) -> None:
        super().__init__('cmd_vel_to_serial')

        self.declare_parameter('serial_port', '/dev/ttyUSB0')
        self.declare_parameter('baud_rate', 115200)
        self.declare_parameter('cmd_vel_topic', '/cmd_vel')

        serial_port = (
            self.get_parameter('serial_port')
            .get_parameter_value()
            .string_value
        )
        baud_rate = (
            self.get_parameter('baud_rate')
            .get_parameter_value()
            .integer_value
        )
        cmd_vel_topic = (
            self.get_parameter('cmd_vel_topic')
            .get_parameter_value()
            .string_value
        )

        self.get_logger().info(
            f'Opening serial {serial_port} at {baud_rate} for cmd_vel bridge'
        )
        self.ser = serial.Serial(serial_port, int(baud_rate), timeout=1)

        self.subscription = self.create_subscription(
            Twist, cmd_vel_topic, self.cmd_callback, 10
        )

    def cmd_callback(self, msg: Twist) -> None:
        vx = msg.linear.x
        wz = msg.angular.z
        # Protocol understood by Arduino: "C vx wz\n"
        line = f'C {vx:.3f} {wz:.3f}\n'
        try:
            self.ser.write(line.encode('ascii'))
        except Exception as exc:
            self.get_logger().error(f'Error writing to serial: {exc}')


def main(args=None) -> None:
    rclpy.init(args=args)
    node = CmdVelToSerial()
    try:
        rclpy.spin(node)
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()

