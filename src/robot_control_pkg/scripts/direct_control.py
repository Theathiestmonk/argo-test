#!/usr/bin/env python3

import rclpy
from rclpy.node import Node
from geometry_msgs.msg import TwistStamped
from rcl_interfaces.msg import ParameterDescriptor

class DirectControl(Node):

    def __init__(self):
        super().__init__('direct_control')
        self.publisher_ = self.create_publisher(TwistStamped, '/cmd_vel_joy', 10)
        self.timer = self.create_timer(0.1, self.timer_callback)
        self.count = 0
        self.get_logger().info('Direct Control Node started. Moving forward for 2 seconds...')

    def timer_callback(self):
        msg = TwistStamped()
        msg.header.stamp = self.get_clock().now().to_msg()
        msg.header.frame_id = 'base_link'
        
        # Move forward for 2 seconds (20 cycles at 0.1s each)
        if self.count < 20:
            msg.twist.linear.x = 0.3
            msg.twist.angular.z = 0.0
            self.publisher_.publish(msg)
            self.count += 1
        elif self.count < 25:
            # Stop for 0.5 seconds
            msg.twist.linear.x = 0.0
            msg.twist.angular.z = 0.0
            self.publisher_.publish(msg)
            self.count += 1
        else:
            self.get_logger().info('Test complete. Shutting down.')
            raise SystemExit

def main(args=None):
    rclpy.init(args=args)
    node = DirectControl()
    try:
        rclpy.spin(node)
    except SystemExit:
        pass
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()

if __name__ == '__main__':
    main()
