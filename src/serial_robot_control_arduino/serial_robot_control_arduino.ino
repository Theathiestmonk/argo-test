#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ---------------- OLED ----------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// -------- Motor Driver Pins (Two Sabertooth drivers) --------
// Right motor driver (driver 1)
#define RPWM1 5   // PWM for right wheel forward
#define LPWM1 6   // PWM for right wheel reverse
#define R_EN1 7   // Enable right driver
#define L_EN1 8   // Enable left driver (same driver, other channel)

// Left motor driver (driver 2)
#define RPWM2 9   // PWM for left wheel forward
#define LPWM2 10  // PWM for left wheel reverse
#define R_EN2 11  // Enable right driver (second board)
#define L_EN2 12  // Enable left driver (second board)

// -------- IR Encoder Pins (one per wheel) --------
const uint8_t irPinR = 2; // Right wheel encoder
const uint8_t irPinL = 3; // Left wheel encoder

// -------- Encoder Variables --------
volatile long totalTicksR = 0; // total edge counts for right wheel
volatile long totalTicksL = 0; // total edge counts for left wheel

volatile uint16_t ticksPerSecR = 0; // ticks counted in the last second
volatile uint16_t ticksPerSecL = 0;

unsigned long lastTickTimeR = 0;
unsigned long lastTickTimeL = 0;

const unsigned long debounceDelay = 5; // ms, shorter debounce for clean IR sensors

// Distance per **full wheel rotation** (meters). Adjust to match your wheel circumference.
const float wheelCircumference = 0.3833; // e.g., 0.3833 m per rotation

// Maximum expected speed in rad/s from ROS 2 (used to scale to 255 PWM).
// You may need to tune this so teleop speeds feel right.
const float MAX_RAD_S = 12.5; 

// Motor speed control (-255 to 255). Negative means reverse.
int pwmR = 0;
int pwmL = 0;

// Safety Watchdog (Heartbeat)
unsigned long lastCommandTime = 0;
const unsigned long commandTimeout = 500; // Stop motors if no command for 500ms

// -------- Serial Command Handling --------
// Expected command format: "m <right_speed> <left_speed>" where speeds are 0‑255.
// Example: "m 200 180" sets right motor to 200, left motor to 180.

void parseSerial();

// -------- Function Prototypes --------
void setMotors(int speedRight, int speedLeft);
void stopMotors();
void updateDisplay();
void computeAndPrintStats();

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);

  // Motor pins as outputs
  pinMode(RPWM1, OUTPUT);
  pinMode(LPWM1, OUTPUT);
  pinMode(R_EN1, OUTPUT);
  pinMode(L_EN1, OUTPUT);

  pinMode(RPWM2, OUTPUT);
  pinMode(LPWM2, OUTPUT);
  pinMode(R_EN2, OUTPUT);
  pinMode(L_EN2, OUTPUT);

  // Enable drivers (active‑high)
  digitalWrite(R_EN1, HIGH);
  digitalWrite(L_EN1, HIGH);
  digitalWrite(R_EN2, HIGH);
  digitalWrite(L_EN2, HIGH);

  // Ensure motors are stopped on startup
  stopMotors();

  // Encoder pins as inputs (use internal pull‑up if needed)
  pinMode(irPinR, INPUT);
  pinMode(irPinL, INPUT);

  // Attach interrupts for rising edges
  attachInterrupt(digitalPinToInterrupt(irPinR), countRight, RISING);
  attachInterrupt(digitalPinToInterrupt(irPinL), countLeft, RISING);

  // OLED init
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED init failed");
    while (true) {}
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("Robot System");
  display.println("Starting...");
  display.display();
  delay(1000);
}

// ---------------- INTERRUPT HANDLERS ----------------
void countRight() {
  unsigned long now = millis();
  if (now - lastTickTimeR > debounceDelay) {
    totalTicksR++;
    ticksPerSecR++;
    lastTickTimeR = now;
  }
}

void countLeft() {
  unsigned long now = millis();
  if (now - lastTickTimeL > debounceDelay) {
    totalTicksL++;
    ticksPerSecL++;
    lastTickTimeL = now;
  }
}

// ---------------- MAIN LOOP ----------------
unsigned long lastStatTime = 0;
void loop() {
  // Handle incoming serial commands (speed changes, stop, reset)
  if (Serial.available()) {
    parseSerial();
  }

  // Safety Watchdog: Stop motors if no command received within timeout
  if (millis() - lastCommandTime > commandTimeout) {
    stopMotors();
  }

  // Every second update OLED (don't print stats to Serial as it slows down ROS)
  unsigned long now = millis();
  if (now - lastStatTime >= 1000) {
    updateDisplay();
    ticksPerSecR = 0;
    ticksPerSecL = 0;
    lastStatTime = now;
  }
}

// ---------------- MOTOR CONTROL ----------------
void setMotors(int speedRight, int speedLeft) {
  // Constrain speeds to safe PWM range
  speedRight = constrain(speedRight, -255, 255);
  speedLeft = constrain(speedLeft, -255, 255);

  // Right Motor
  if (speedRight >= 0) {
    analogWrite(RPWM1, speedRight);
    analogWrite(LPWM1, 0);
  } else {
    analogWrite(RPWM1, 0);
    analogWrite(LPWM1, -speedRight);
  }

  // Left Motor (Notice LPWM2 is forward based on your original moveForward)
  if (speedLeft >= 0) {
    analogWrite(RPWM2, 0);
    analogWrite(LPWM2, speedLeft);
  } else {
    analogWrite(RPWM2, -speedLeft);
    analogWrite(LPWM2, 0);
  }
}

void stopMotors() {
  analogWrite(RPWM1, 0);
  analogWrite(LPWM1, 0);
  analogWrite(RPWM2, 0);
  analogWrite(LPWM2, 0);
}

// ---------------- DISPLAY & STATS ----------------
void computeAndPrintStats() {
  // RPM calculation (ticks per second -> revolutions per minute)
  float revR = ticksPerSecR / 2.0; // assuming one tick per half‑rotation
  float revL = ticksPerSecL / 2.0;
  float rpmR = revR * 60.0;
  float rpmL = revL * 60.0;

  // Distance travelled (total ticks -> rotations -> meters)
  float distR = (totalTicksR / 2.0) * wheelCircumference;
  float distL = (totalTicksL / 2.0) * wheelCircumference;

  // Heading estimate (difference in distance / wheel base)
  const float wheelBase = 0.45; // meters between wheels
  float thetaDeg = ((distR - distL) / wheelBase) * 57.2958;

  Serial.println("--- Stats ---");
  Serial.print("Right ticks: "); Serial.println(totalTicksR);
  Serial.print("Left  ticks: "); Serial.println(totalTicksL);
  Serial.print("RPM Right: "); Serial.println(rpmR);
  Serial.print("RPM Left : "); Serial.println(rpmL);
  Serial.print("Dist Right (m): "); Serial.println(distR);
  Serial.print("Dist Left  (m): "); Serial.println(distL);
  Serial.print("Heading (deg): "); Serial.println(thetaDeg);
}

void updateDisplay() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("R ticks:"); display.println(totalTicksR);
  display.print("L ticks:"); display.println(totalTicksL);
  display.print("R rpm:"); display.println(ticksPerSecR * 30); // (ticks/2)*60 per sec
  display.print("L rpm:"); display.println(ticksPerSecL * 30);
  display.display();
}

// ---------------- SERIAL COMMAND PARSER ----------------
void parseSerial() {
  String line = Serial.readStringUntil('\n');
  line.trim();
  if (line.length() == 0) return;

  if (line.charAt(0) == 'm') {
    int firstSpace = line.indexOf(' ');
    if (firstSpace == -1) return;
    int secondSpace = line.indexOf(' ', firstSpace + 1);
    if (secondSpace == -1) return;
    
    // As per robot_control_pkg/arduino_comms.hpp, it sends "m <left> <right>\n"
    String sL = line.substring(firstSpace + 1, secondSpace);
    String sR = line.substring(secondSpace + 1);
    
    // ROS 2 sends float values in rad/s
    float target_rad_s_L = sL.toFloat();
    float target_rad_s_R = sR.toFloat();
    
    // Map rad/s to PWM (-255 to 255)
    pwmL = (target_rad_s_L / MAX_RAD_S) * 255;
    pwmR = (target_rad_s_R / MAX_RAD_S) * 255;
    
    // Update last command time for safety watchdog
    lastCommandTime = millis();
    
    setMotors(pwmR, pwmL);
    
    // ROS 2 Hardware Interface expects a response line, send 'm' to acknowledge
    Serial.println("m"); 
  } else if (line.charAt(0) == 'e') {
    // Print encoder counts for ROS 2 Control: "<left> <right>"
    Serial.print(totalTicksL);
    Serial.print(" ");
    Serial.println(totalTicksR);
  } else if (line == "stop") {
    stopMotors();
    Serial.println("s");
  } else if (line == "reset") {
    noInterrupts();
    totalTicksR = totalTicksL = 0;
    interrupts();
    Serial.println("r");
  }
}