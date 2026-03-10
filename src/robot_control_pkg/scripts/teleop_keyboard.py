import rclpy
from rclpy.node import Node
from std_msgs.msg import String
import sys
import tty
import termios
import time

class TeleopNode(Node):
    def __init__(self):
        super().__init__('teleop_keyboard')
        self.publisher_ = self.create_publisher(String, '/arduino_command', 10)
        self.get_logger().info("Teleop Node Started. Use W, A, S, D, X. Q to quit.")

    def send_cmd(self, key):
        msg = String()
        msg.data = key
        self.publisher_.publish(msg)

def getKey():
    fd = sys.stdin.fileno()
    old_settings = termios.tcgetattr(fd)
    try:
        tty.setraw(sys.stdin.fileno())
        ch = sys.stdin.read(1)
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)
    return ch

def main():
    rclpy.init()
    node = TeleopNode()

    print("\n--- Robot Control & Mapping Teleop (ROS 2 Mode) ---")
    print("W : Forward")
    print("S : Reverse")
    print("A : Left")
    print("D : Right")
    print("X : STOP")
    print("Q : Quit Program\n")

    try:
        while rclpy.ok():
            key = getKey().upper()

            if key == 'Q':
                node.send_cmd('X')
                print("\nStopping Program...")
                break

            if key in ['W','A','S','D','X']:
                print(f"Key Pressed: {key}")
                node.send_cmd(key)
            
    except KeyboardInterrupt:
        node.send_cmd('X')
    finally:
        node.destroy_node()
        rclpy.shutdown()

if __name__ == '__main__':
    main()
