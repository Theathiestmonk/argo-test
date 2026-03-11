#include <Wire.h>

// IMPORTANT: Install "Adafruit MPU6050" library in Arduino IDE
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// ---------------- IMU ----------------
Adafruit_MPU6050 mpu;

// -------- Motor Driver Pins --------
#define RPWM1 5
#define LPWM1 6
#define R_EN1 7
#define L_EN1 8

#define RPWM2 9
#define LPWM2 10
#define R_EN2 11
#define L_EN2 12

// -------- Wheel / Robot Physical Parameters --------
const float WHEEL_RADIUS       = 0.055;   // meters
const float WHEEL_BASE         = 0.5;     // meters
const float WHEEL_CIRCUMFERENCE = 0.3454; // meters
const float MAX_RPM            = 70.0;    // RPM
const float MAX_RAD_S          = 12.5;

// -------- Motor PWM --------
int pwmR = 0;
int pwmL = 0;

// -------- Dead Reckoning State --------
float posX  = 0.0;
float posY  = 0.0;
float theta = 0.0; // heading in radians
unsigned long lastOdomTime = 0;

// -------- Timing --------
unsigned long lastStatTime  = 0;
unsigned long lastOdomPublishTime = 0;

// ---------- CONTROL STATE ----------
String currentAction = "STANDBY";
char lastCommand = 'X';

// -------- Function Prototypes --------
void setMotors(int speedRight, int speedLeft);
void stopMotors();
void updateDeadReckoning();
void publishOdom();
void parseSerial(); 

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);

  // Motors
  pinMode(RPWM1, OUTPUT); pinMode(LPWM1, OUTPUT);
  pinMode(R_EN1, OUTPUT); pinMode(L_EN1, OUTPUT);
  pinMode(RPWM2, OUTPUT); pinMode(LPWM2, OUTPUT);
  pinMode(R_EN2, OUTPUT); pinMode(L_EN2, OUTPUT);

  digitalWrite(R_EN1, HIGH); digitalWrite(L_EN1, HIGH);
  digitalWrite(R_EN2, HIGH); digitalWrite(L_EN2, HIGH);
  stopMotors();

  // IMU Init
  if (!mpu.begin()) {
    Serial.println("MPU6050 init failed");
  } else {
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  }

  lastOdomTime        = millis();
  lastOdomPublishTime = millis();
}

// ---------------- MAIN LOOP ----------------
void loop() {
  unsigned long now = millis();

  // Handle incoming teleop commands over serial
  if (Serial.available()) {
    parseSerial();
  }

  // PWM Mapping (0 - 255)
  int forwardSpeed = 90; 
  int turnSpeed = 70;

  // KEYBOARD CONTROL LOGIC
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

  setMotors(pwmR, pwmL);

  // Update dead reckoning continuously using exactly what the motors are doing
  updateDeadReckoning();

  // Publish Odom over Serial to ROS2 (20Hz)
  if (now - lastOdomPublishTime >= 50) {
    publishOdom();
    lastOdomPublishTime = now;
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

  Serial.print("o ");
  Serial.print(posX, 4);       Serial.print(" ");
  Serial.print(posY, 4);       Serial.print(" ");
  Serial.print(theta, 4);      Serial.print(" ");
  Serial.print(linearVel, 4);  Serial.print(" ");
  Serial.print(angularVel, 4); Serial.print(" ");
  
  // Add IMU data for mapping: gyro_z, accel_x, accel_y
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  Serial.print(g.gyro.z, 4);   Serial.print(" ");
  Serial.print(a.acceleration.x, 4); Serial.print(" ");
  Serial.println(a.acceleration.y, 4);
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

void stopMotors() {
  analogWrite(RPWM1, 0); analogWrite(LPWM1, 0);
  analogWrite(RPWM2, 0); analogWrite(LPWM2, 0);
}

// ---------------- SERIAL COMMAND PARSER ----------------
void parseSerial() {
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    
    // Check if it's one of our control keys
    if (cmd == 'W' || cmd == 'A' || cmd == 'S' || cmd == 'D' || cmd == 'X') {
      lastCommand = cmd;
    }
    
    // Still support reset if needed
    if (cmd == 'r') {
       posX = 0; posY = 0; theta = 0;
    }
  }
}
