=import rclpy
from rclpy.node import Node
from sensor_msgs.msg import Imu
from nav_msgs.msg import Odometry
from geometry_msgs.msg import TransformStamped
from std_msgs.msg import String
import tf2_ros
import serial
import math

class OdomPublisher(Node):
    def __init__(self):
        super().__init__('odom_publisher')
        self.odom_pub = self.create_publisher(Odometry, '/odom', 10)
        self.imu_pub  = self.create_publisher(Imu, '/imu/data', 10)
        self.tf_broadcaster = tf2_ros.TransformBroadcaster(self)
        self.subscription = self.create_subscription(
            String, '/arduino_command', self.cmd_callback, 10)
        self.ser = serial.Serial('/dev/ttyUSB0', 115200, timeout=1)
        self.timer = self.create_timer(0.05, self.read_serial)

    def cmd_callback(self, msg):
        self.ser.write(msg.data.encode())
        self.get_logger().info(f"Sent to Arduino: {msg.data}")

    def read_serial(self):
        if self.ser.in_waiting:
            try:
                line = self.ser.readline().decode().strip()
                if line.startswith('o '):
                    parts = line.split()
                    if len(parts) >= 9:
                        x, y, theta = float(parts[1]), float(parts[2]), float(parts[3])
                        vx, wz      = float(parts[4]), float(parts[5])
                        gz, ax, ay  = float(parts[6]), float(parts[7]), float(parts[8])
                        self.publish_data(x, y, theta, vx, wz, gz, ax, ay)
            except Exception as e:
                self.get_logger().error(f"Serial Error: {e}")

    def publish_data(self, x, y, theta, vx, wz, gz, ax, ay):
        now = self.get_clock().now().to_msg()

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
        odom.twist.twist.linear.x = vx
        odom.twist.twist.angular.z = wz
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