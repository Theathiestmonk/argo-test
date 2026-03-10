import sys
import tty
import termios
import serial
import time

# Arduino Connection
SERIAL_PORT = '/dev/ttyUSB0' # Updated to match your odom_publisher
BAUD_RATE = 115200          # Updated to match your odom_publisher

print("Connecting to Arduino...")
try:
    arduino = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=0.1)
    time.sleep(2)
    connected = True
    print(f"Connected to {SERIAL_PORT} at {BAUD_RATE}")
except Exception as e:
    connected = False
    print(f"Error: {e}")
    print("Arduino NOT connected (Test Mode)")

def getKey():
    fd = sys.stdin.fileno()
    old_settings = termios.tcgetattr(fd)
    try:
        tty.setraw(sys.stdin.fileno())
        ch = sys.stdin.read(1)
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)
    return ch

print("\n--- Robot Control & Mapping Teleop ---")
print("W : Forward")
print("S : Reverse")
print("A : Left")
print("D : Right")
print("X : STOP")
print("Q : Quit Program\n")

try:
    while True:
        key = getKey().upper()

        if key == 'Q':
            if connected: arduino.write(b'X') # Send stop before quitting
            print("\nStopping Program...")
            break

        if key in ['W','A','S','D','X']:
            print(f"Key Pressed: {key}")
            if connected:
                arduino.write(key.encode())
        
        # Briefly check serial to clear buffer so mapping code isn't blocked
        if connected and arduino.in_waiting:
            arduino.readline()

except KeyboardInterrupt:
    if connected: arduino.write(b'X')
    print("\nProgram Interrupted")
finally:
    if connected: arduino.close()
