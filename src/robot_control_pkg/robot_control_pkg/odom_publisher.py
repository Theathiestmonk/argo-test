import rclpy
from rclpy.node import Node
from nav_msgs.msg import Odometry
from geometry_msgs.msg import TransformStamped
import tf2_ros
import serial
import math

class OdomPublisher(Node):
    def __init__(self):
        super().__init__('odom_publisher')
        self.pub = self.create_publisher(Odometry, '/odom', 10)
        self.tf_broadcaster = tf2_ros.TransformBroadcaster(self)
        
        # Open serial port to Arduino
        self.ser = serial.Serial('/dev/ttyUSB0', 115200, timeout=1)
        
        self.timer = self.create_timer(0.05, self.read_serial)  # 20Hz

    def read_serial(self):
        if self.ser.in_waiting:
            try:
                line = self.ser.readline().decode().strip()
                if line.startswith('o '):
                    parts = line.split()
                    x     = float(parts[1])
                    y     = float(parts[2])
                    theta = float(parts[3])
                    vx    = float(parts[4])
                    wz    = float(parts[5])
                    self.publish_odom(x, y, theta, vx, wz)
            except:
                pass

    def publish_odom(self, x, y, theta, vx, wz):
        now = self.get_clock().now().to_msg()

        # Publish TF
        t = TransformStamped()
        t.header.stamp = now
        t.header.frame_id = 'odom'
        t.child_frame_id = 'base_footprint_link'
        t.transform.translation.x = x
        t.transform.translation.y = y
        t.transform.rotation.z = math.sin(theta / 2)
        t.transform.rotation.w = math.cos(theta / 2)
        self.tf_broadcaster.sendTransform(t)

        # Publish Odometry message
        odom = Odometry()
        odom.header.stamp = now
        odom.header.frame_id = 'odom'
        odom.child_frame_id = 'base_footprint_link'
        odom.pose.pose.position.x = x
        odom.pose.pose.position.y = y
        odom.pose.pose.orientation.z = math.sin(theta / 2)
        odom.pose.pose.orientation.w = math.cos(theta / 2)
        odom.twist.twist.linear.x = vx
        odom.twist.twist.angular.z = wz
        self.pub.publish(odom)

def main():
    rclpy.init()
    rclpy.spin(OdomPublisher())

if __name__ == '__main__':
    main()
