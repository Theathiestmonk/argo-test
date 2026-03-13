#!/usr/bin/env python3

import rclpy
from rclpy.node import Node
from std_msgs.msg import String
import sys
import tty
import termios

class TeleopNode(Node):
    def __init__(self):
        super().__init__('teleop_keyboard')
        self.publisher_ = self.create_publisher(String, '/arduino_command', 10)
        self.get_logger().info("Teleop ready.")

    def send_cmd(self, key):
        msg = String()
        msg.data = key
        self.publisher_.publish(msg)

def getKey():
    fd = sys.stdin.fileno()
    old = termios.tcgetattr(fd)
    try:
        tty.setraw(fd)
        ch = sys.stdin.read(1)
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, old)
    return ch

def main():
    rclpy.init()
    node = TeleopNode()

    print("\nW=Forward  S=Reverse  A=Left  D=Right  X=Stop  Q=Quit\n")

    try:
        while rclpy.ok():
            key = getKey().upper()
            if key == 'Q':
                node.send_cmd('X')
                break
            if key in ['W', 'A', 'S', 'D', 'X']:
                print(f"Sent: {key}")
                node.send_cmd(key)
    except KeyboardInterrupt:
        node.send_cmd('X')
    finally:
        node.destroy_node()
        rclpy.shutdown()

if __name__ == '__main__':
    main()