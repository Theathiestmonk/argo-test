import rclpy
from rclpy.node import Node
from sensor_msgs.msg import Imu
import smbus2
import math

# MPU6050 registers
MPU6050_ADDR = 0x68
PWR_MGMT_1   = 0x6B
GYRO_ZOUT_H  = 0x47
ACCEL_XOUT_H = 0x3B

class ImuPublisher(Node):
    def __init__(self):
        super().__init__('imu_publisher')
        self.pub = self.create_publisher(Imu, '/imu/data', 10)
        self.bus = smbus2.SMBus(1)
        
        # Wake up MPU6050
        self.bus.write_byte_data(MPU6050_ADDR, PWR_MGMT_1, 0)
        
        self.timer = self.create_timer(0.02, self.read_imu)  # 50Hz

    def read_raw(self, reg):
        high = self.bus.read_byte_data(MPU6050_ADDR, reg)
        low  = self.bus.read_byte_data(MPU6050_ADDR, reg + 1)
        val  = (high << 8) | low
        return val - 65536 if val > 32767 else val

    def read_imu(self):
        ax = self.read_raw(ACCEL_XOUT_H) / 16384.0 * 9.81
        ay = self.read_raw(ACCEL_XOUT_H + 2) / 16384.0 * 9.81
        az = self.read_raw(ACCEL_XOUT_H + 4) / 16384.0 * 9.81
        gz = self.read_raw(GYRO_ZOUT_H) / 131.0 * (math.pi / 180)

        msg = Imu()
        msg.header.stamp = self.get_clock().now().to_msg()
        msg.header.frame_id = 'imu_link'
        msg.linear_acceleration.x = ax
        msg.linear_acceleration.y = ay
        msg.linear_acceleration.z = az
        msg.angular_velocity.z = gz
        
        # Tell EKF we trust gyro more than accel for rotation
        msg.angular_velocity_covariance[8] = 0.01
        msg.linear_acceleration_covariance[0] = 0.1
        
        self.pub.publish(msg)

def main():
    rclpy.init()
    rclpy.spin(ImuPublisher())
