#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// IMPORTANT: Install "NewPing" and "Adafruit MPU6050" libraries in Arduino IDE
#include <NewPing.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// ---------------- OLED ----------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

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

// ---------- ULTRASONIC SENSOR PINS ----------
#define TRIG_C 22
#define ECHO_C 23

#define TRIG_L 27
#define ECHO_L 25

#define TRIG_R 26
#define ECHO_R 24

#define MAX_DISTANCE 400

NewPing sonarC(TRIG_C, ECHO_C, MAX_DISTANCE);
NewPing sonarL(TRIG_L, ECHO_L, MAX_DISTANCE);
NewPing sonarR(TRIG_R, ECHO_R, MAX_DISTANCE);

// ---------- BUZZER PINS ----------
#define BUZZER_C 30
#define BUZZER_R 31
#define BUZZER_L 32

// ---------- AUTONOMOUS VARIABLES ----------
int distC = 400;
int distL = 400;
int distR = 400;
const int threshold = 50;

// Timer for non-blocking ping
unsigned long pingTimer[3]; 
uint8_t currentSensor = 0;
String currentAction = "STANDBY";
char lastCommand = 'X';

// -------- Function Prototypes --------
void setMotors(int speedRight, int speedLeft);
void stopMotors();
void updateDisplay();
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

  // Buzzers
  pinMode(BUZZER_C, OUTPUT);
  pinMode(BUZZER_L, OUTPUT);
  pinMode(BUZZER_R, OUTPUT);

  // OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED init failed");
  } else {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.println("Autonomous Mode");
    display.display();
  }

  // IMU Init
  if (!mpu.begin()) {
    Serial.println("MPU6050 init failed");
  } else {
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  }

  // Ping timers (every 33ms per sensor, approx 100ms cycle)
  pingTimer[0] = millis() + 33;
  pingTimer[1] = pingTimer[0] + 33;
  pingTimer[2] = pingTimer[1] + 33;

  lastOdomTime        = millis();
  lastOdomPublishTime = millis();
}

// ---------- NON-BLOCKING PING ----------
void echoCheck() {
  if (currentSensor == 0) {
    if (sonarC.check_timer()) distC = sonarC.ping_result / US_ROUNDTRIP_CM;
  } else if (currentSensor == 1) {
    if (sonarL.check_timer()) distL = sonarL.ping_result / US_ROUNDTRIP_CM;
  } else if (currentSensor == 2) {
    if (sonarR.check_timer()) distR = sonarR.ping_result / US_ROUNDTRIP_CM;
  }
}

// ---------------- MAIN LOOP ----------------
void loop() {
  unsigned long now = millis();

  // Still allow resetting from ROS2 (encoder reset if needed)
  if (Serial.available()) {
    parseSerial();
  }

  // Handle Pings Asynchronously
  for (uint8_t i = 0; i < 3; i++) {
    if (now >= pingTimer[i]) {
      pingTimer[i] += 100; // Schedule next ping roughly 100ms from now
      if (i == 0) {
        currentSensor = 0;
        sonarC.ping_timer(echoCheck);
      } else if (i == 1) {
        currentSensor = 1;
        sonarL.ping_timer(echoCheck);
      } else if (i == 2) {
        currentSensor = 2;
        sonarR.ping_timer(echoCheck);
      }
    }
  }

  // If 0, it means timeout / max distance
  if (distC == 0) distC = 400;
  if (distL == 0) distL = 400;
  if (distR == 0) distR = 400;

  // Update Buzzers
  digitalWrite(BUZZER_C, distC < threshold);
  digitalWrite(BUZZER_L, distL < threshold);
  digitalWrite(BUZZER_R, distR < threshold);

  // AUTONOMOUS NAVIGATION LOGIC
  bool blockedC = distC < threshold;
  bool blockedL = distL < threshold;
  bool blockedR = distR < threshold;

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

  // Update OLED Display (1Hz)
  if (now - lastStatTime >= 1000) {
    updateDisplay();
    lastStatTime = now;
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

// ---------------- DISPLAY ----------------
void updateDisplay() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print(currentAction);
  display.print("  ");
  display.print(distL); display.print("-");
  display.print(distC); display.print("-");
  display.println(distR);

  display.print("X:"); display.print(posX, 1);
  display.print(" Y:"); display.println(posY, 1);
  display.print("Th:"); display.println(theta * 57.3, 0);
  display.display();
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
