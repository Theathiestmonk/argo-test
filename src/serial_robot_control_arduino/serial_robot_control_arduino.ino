#include <Wire.h>

// IMPORTANT: Install "Adafruit MPU6050" library in Arduino IDE
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// ---------------- IMU ----------------
Adafruit_MPU6050 mpu;
bool imu_ok = false;

// -------- Motor Driver Pins --------
#define RPWM1 5
#define LPWM1 6
#define R_EN1 7
#define L_EN1 8

#define RPWM2 9
#define LPWM2 10
#define R_EN2 11
#define L_EN2 12

// -------- Ultrasonic Sensor Pins (3 sensors) --------
// HC-SR04 style sensors: TRIG = output, ECHO = input
// Center sensor
#define TRIG_C 22
#define ECHO_C 23
// Left sensor
#define TRIG_L 27
#define ECHO_L 25
// Right sensor
#define TRIG_R 26
#define ECHO_R 24

// -------- Buzzer Pins (3 buzzers) --------
// Each buzzer goes HIGH when its corresponding sensor is below threshold
#define BUZZER_C 30
#define BUZZER_L 32
#define BUZZER_R 31

// -------- Wheel / Robot Physical Parameters --------
const float WHEEL_RADIUS       = 0.055;   // meters
const float WHEEL_BASE         = 0.5;     // meters
const float WHEEL_CIRCUMFERENCE = 0.3454; // meters
const float MAX_RPM            = 70.0;    // RPM
const float MAX_RAD_S          = 12.5;
const bool ENABLE_DIRECT_MOTOR_TEST = false;
const bool ENABLE_CMD_DEBUG = true;

// -------- Motor PWM --------
int pwmR = 0;
int pwmL = 0;
const int PWM_MIN_THRESHOLD = 85;

// -------- Ultrasonic + Buzzer State --------
float distC = 0.0f;
float distL = 0.0f;
float distR = 0.0f;
const float ULTRA_THRESHOLD = 0.30f;  // meters

// -------- Dead Reckoning State --------
float posX  = 0.0;
float posY  = 0.0;
float theta = 0.0; // heading in radians
unsigned long lastOdomTime = 0;

// -------- Timing --------
unsigned long lastStatTime  = 0;
unsigned long lastOdomPublishTime = 0;
unsigned long lastUltraTime = 0;
unsigned long lastDebugPrintTime = 0;

// ---------- CONTROL STATE ----------
String currentAction = "STANDBY";
char lastCommand = 'X';

// Commanded velocities from ROS (Twist-style)
float cmd_vx = 0.0;   // linear velocity (m/s)
float cmd_wz = 0.0;   // angular velocity (rad/s)
bool use_cmd_vel = false;
unsigned long lastCmdVelTime = 0;
const unsigned long CMD_VEL_TIMEOUT_MS = 350;

// -------- Function Prototypes --------
void setMotors(int speedRight, int speedLeft);
void stopMotors();
void updateDeadReckoning();
void publishOdom();
void parseSerial();
float getDistance(int trigPin, int echoPin);
void updateUltrasonicAndBuzzers();
int applyPwmDeadband(int pwm);

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);
  Serial.setTimeout(5);  // Keep parseSerial() non-blocking on partial lines.

  // Motors
  pinMode(RPWM1, OUTPUT); pinMode(LPWM1, OUTPUT);
  pinMode(R_EN1, OUTPUT); pinMode(L_EN1, OUTPUT);
  pinMode(RPWM2, OUTPUT); pinMode(LPWM2, OUTPUT);
  pinMode(R_EN2, OUTPUT); pinMode(L_EN2, OUTPUT);

  // Ultrasonic sensors
  pinMode(TRIG_C, OUTPUT);
  pinMode(ECHO_C, INPUT);
  pinMode(TRIG_L, OUTPUT);
  pinMode(ECHO_L, INPUT);
  pinMode(TRIG_R, OUTPUT);
  pinMode(ECHO_R, INPUT);

  // Buzzers
  pinMode(BUZZER_C, OUTPUT);
  pinMode(BUZZER_L, OUTPUT);
  pinMode(BUZZER_R, OUTPUT);
  digitalWrite(BUZZER_C, LOW);
  digitalWrite(BUZZER_L, LOW);
  digitalWrite(BUZZER_R, LOW);

  digitalWrite(R_EN1, HIGH); digitalWrite(L_EN1, HIGH);
  digitalWrite(R_EN2, HIGH); digitalWrite(L_EN2, HIGH);
  stopMotors();

  // IMU Init
  if (!mpu.begin()) {
    Serial.println("MPU6050 init failed");
    imu_ok = false;
  } else {
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
    imu_ok = true;
  }

  lastOdomTime        = millis();
  lastOdomPublishTime = millis();
  lastUltraTime       = millis();
  lastCmdVelTime      = millis();
}

// ---------------- MAIN LOOP ----------------
void loop() {
  unsigned long now = millis();

  if (ENABLE_DIRECT_MOTOR_TEST) {
    setMotors(180, 180);
    delay(3000);
    setMotors(0, 0);
    delay(2000);
    return;
  }

  // Handle incoming teleop commands over serial
  if (Serial.available()) {
    parseSerial();
  }

  // PWM Mapping (0 - 255)
  int forwardSpeed = 50; 
  int turnSpeed = 40;

  // If we have received numeric cmd_vel from ROS, use that.
  // Otherwise fall back to keyboard W/A/S/D logic.
  if (use_cmd_vel && (now - lastCmdVelTime > CMD_VEL_TIMEOUT_MS)) {
    // Safety watchdog: stop robot if cmd stream drops.
    cmd_vx = 0.0f;
    cmd_wz = 0.0f;
    lastCommand = 'X';
    use_cmd_vel = false;
  }

  if (use_cmd_vel) {
    // Differential drive kinematics:
    // v_r = v + w * (WHEEL_BASE / 2)
    // v_l = v - w * (WHEEL_BASE / 2)
    float v_r = cmd_vx + cmd_wz * (WHEEL_BASE / 2.0f);
    float v_l = cmd_vx - cmd_wz * (WHEEL_BASE / 2.0f);

    // Convert wheel linear velocity (m/s) back to PWM using the same model
    // used in updateDeadReckoning/publishOdom.
    float max_lin = (MAX_RPM / 60.0f) * WHEEL_CIRCUMFERENCE; // m/s at full PWM
    if (max_lin < 1e-4) {
      pwmR = 0;
      pwmL = 0;
    } else {
      pwmR = (int)constrain((v_r / max_lin) * 255.0f, -255.0f, 255.0f);
      pwmL = (int)constrain((v_l / max_lin) * 255.0f, -255.0f, 255.0f);
    }
    pwmR = applyPwmDeadband(pwmR);
    pwmL = applyPwmDeadband(pwmL);
    if (ENABLE_CMD_DEBUG && now - lastDebugPrintTime > 300) {
      lastDebugPrintTime = now;
      Serial.print("DBG CMD vx=");
      Serial.print(cmd_vx, 3);
      Serial.print(" wz=");
      Serial.print(cmd_wz, 3);
      Serial.print(" pwmR=");
      Serial.print(pwmR);
      Serial.print(" pwmL=");
      Serial.println(pwmL);
    }
    currentAction = "CMD_VEL";
  } else {
    // KEYBOARD CONTROL LOGIC (W/A/S/D/X)
    if (lastCommand == 'W') {
      currentAction = "FORWARD";
      pwmL = forwardSpeed; pwmR = forwardSpeed;
    } 
    else if (lastCommand == 'S') {
      currentAction = "REVERSE";
      pwmL = -forwardSpeed; pwmR = -forwardSpeed;
    } 
    else if (lastCommand == 'A') {
      currentAction = "LEFT";
      pwmL = -turnSpeed; pwmR = turnSpeed;
    } 
    else if (lastCommand == 'D') {
      currentAction = "RIGHT";
      pwmL = turnSpeed; pwmR = -turnSpeed;
    } 
    else {
      currentAction = "STOP";
      pwmL = 0; pwmR = 0;
    }
  }

  setMotors(pwmR, pwmL);

  // Update dead reckoning continuously using exactly what the motors are doing
  updateDeadReckoning();

  // Publish Odom over Serial to ROS2 (20Hz)
  if (now - lastOdomPublishTime >= 50) {
    publishOdom();
    lastOdomPublishTime = now;
  }

  // Ultrasonic sensing and buzzer control (10 Hz)
  if (now - lastUltraTime >= 100) {
    updateUltrasonicAndBuzzers();
    lastUltraTime = now;
  }
}

// ---------------- DEAD RECKONING ----------------
void updateDeadReckoning() {
  unsigned long now = millis();
  float dt = (now - lastOdomTime) / 1000.0;
  lastOdomTime = now;

  if (dt <= 0 || dt > 0.5) return;

  float velR = (pwmR / 255.0) * (MAX_RPM / 60.0) * WHEEL_CIRCUMFERENCE;
  float velL = (pwmL / 255.0) * (MAX_RPM / 60.0) * WHEEL_CIRCUMFERENCE;

  float linearVel  = (velR + velL) / 2.0;
  float angularVel = (velR - velL) / WHEEL_BASE;

  theta += angularVel * dt;
  while (theta >  PI) theta -= 2 * PI;
  while (theta < -PI) theta += 2 * PI;

  posX += linearVel * cos(theta) * dt;
  posY += linearVel * sin(theta) * dt;
}

// ---------------- PUBLISH ----------------
void publishOdom() {
  float velR = (pwmR / 255.0) * (MAX_RPM / 60.0) * WHEEL_CIRCUMFERENCE;
  float velL = (pwmL / 255.0) * (MAX_RPM / 60.0) * WHEEL_CIRCUMFERENCE;
  float linearVel  = (velR + velL) / 2.0;
  float angularVel = (velR - velL) / WHEEL_BASE;
  float gyro_z = 0.0f;
  float accel_x = 0.0f;
  float accel_y = 0.0f;

  Serial.print("o ");
  Serial.print(posX, 4);       Serial.print(" ");
  Serial.print(posY, 4);       Serial.print(" ");
  Serial.print(theta, 4);      Serial.print(" ");
  Serial.print(linearVel, 4);  Serial.print(" ");
  Serial.print(angularVel, 4); Serial.print(" ");
  
  // Add IMU data for mapping: gyro_z, accel_x, accel_y.
  // If IMU is absent/fails, publish zeros so ROS odom/TF still works.
  if (imu_ok) {
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
    gyro_z = g.gyro.z;
    accel_x = a.acceleration.x;
    accel_y = a.acceleration.y;
  }
  Serial.print(gyro_z, 4);   Serial.print(" ");
  Serial.print(accel_x, 4);  Serial.print(" ");
  Serial.println(accel_y, 4);
}

// ---------------- ULTRASONIC + BUZZERS ----------------
float getDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 30000UL); // timeout 30 ms
  if (duration <= 0) {
    return 9.99f;  // treat as "far away" (meters)
  }

  float distance_cm = duration * 0.0343f / 2.0f;
  return distance_cm / 100.0f;  // convert to meters
}

void updateUltrasonicAndBuzzers() {
  // Measure distances in meters
  distC = getDistance(TRIG_C, ECHO_C);
  delay(20);
  distL = getDistance(TRIG_L, ECHO_L);
  delay(20);
  distR = getDistance(TRIG_R, ECHO_R);
  delay(20);

  // Buzzers: HIGH when closer than threshold
  digitalWrite(BUZZER_C, distC < ULTRA_THRESHOLD);
  digitalWrite(BUZZER_L, distL < ULTRA_THRESHOLD);
  digitalWrite(BUZZER_R, distR < ULTRA_THRESHOLD);

  // Send center distance to ROS over serial for ultrasonic_safety node
  Serial.print("u ");
  Serial.println(distC, 3);  // meters
}

// ---------------- MOTOR CONTROL ----------------
void setMotors(int speedRight, int speedLeft) {
  speedRight = constrain(speedRight, -255, 255);
  speedLeft  = constrain(speedLeft,  -255, 255);

  // Right Motor
  if (speedRight >= 0) {
    analogWrite(RPWM1, speedRight); analogWrite(LPWM1, 0);
  } else {
    analogWrite(RPWM1, 0); analogWrite(LPWM1, -speedRight);
  }

  // Left Motor
  if (speedLeft >= 0) {
    analogWrite(RPWM2, 0); analogWrite(LPWM2, speedLeft);
  } else {
    analogWrite(RPWM2, -speedLeft); analogWrite(LPWM2, 0);
  }
}

int applyPwmDeadband(int pwm) {
  if (pwm == 0) return 0;
  if (pwm > 0 && pwm < PWM_MIN_THRESHOLD) return PWM_MIN_THRESHOLD;
  if (pwm < 0 && pwm > -PWM_MIN_THRESHOLD) return -PWM_MIN_THRESHOLD;
  return pwm;
}

void stopMotors() {
  analogWrite(RPWM1, 0); analogWrite(LPWM1, 0);
  analogWrite(RPWM2, 0); analogWrite(LPWM2, 0);
}

// ---------------- SERIAL COMMAND PARSER ----------------
// Supports:
//  - Single-char keyboard commands: W/A/S/D/X/r  (for teleop)
//  - Numeric velocity commands from ROS: "C vx wz\n"
//      where vx is linear x [m/s], wz is angular z [rad/s]
void parseSerial() {
  if (Serial.available() <= 0) {
    return;
  }

  // Peek the first non-whitespace character to decide the mode
  char first = Serial.peek();

  if (first == 'C') {
    // Read the whole line "C vx wz"
    String line = Serial.readStringUntil('\n');

    // Parse with sscanf-style logic
    char cmdChar;
    float vx, wz;
    int parsed = sscanf(line.c_str(), "%c %f %f", &cmdChar, &vx, &wz);
    if (parsed == 3 && cmdChar == 'C') {
      cmd_vx = vx;
      cmd_wz = wz;
      use_cmd_vel = true;
      lastCmdVelTime = millis();
      if (ENABLE_CMD_DEBUG) {
        Serial.print("ACK C vx=");
        Serial.print(vx, 3);
        Serial.print(" wz=");
        Serial.println(wz, 3);
      }
    }
  } else {
    // Fallback: single-character teleop commands
    char cmd = Serial.read();

    // Teleop keys override cmd_vel mode
    if (cmd == 'W' || cmd == 'A' || cmd == 'S' || cmd == 'D' || cmd == 'X') {
      lastCommand = cmd;
      use_cmd_vel = false;
    }

    // Reset pose if needed
    if (cmd == 'r') {
      posX = 0; posY = 0; theta = 0;
    }
  }
}
