#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ---------------- OLED ----------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// -------- Motor Driver Pins --------
#define RPWM1 5
#define LPWM1 6
#define R_EN1 7
#define L_EN1 8

#define RPWM2 9
#define LPWM2 10
#define R_EN2 11
#define L_EN2 12

// -------- IR Encoder Pins --------
const uint8_t irPinR = 2;
const uint8_t irPinL = 3;

// -------- Encoder Variables --------
volatile long totalTicksR = 0;
volatile long totalTicksL = 0;

unsigned long lastTickTimeR = 0;
unsigned long lastTickTimeL = 0;
const unsigned long debounceDelay = 5;

// -------- Motor Control --------
const float MAX_RAD_S = 12.5;
int pwmR = 0;
int pwmL = 0;

// -------- Safety Watchdog --------
unsigned long lastCommandTime = 0;
const unsigned long commandTimeout = 500;

// -------- Serial Buffer --------
static char buffer[32];
static int bufPos = 0;

// -------- Function Prototypes --------
void setMotors(int speedRight, int speedLeft);
void stopMotors();
void processCommand(char* cmd);

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);

  // Motor pins
  pinMode(RPWM1, OUTPUT); pinMode(LPWM1, OUTPUT);
  pinMode(R_EN1, OUTPUT); pinMode(L_EN1, OUTPUT);
  pinMode(RPWM2, OUTPUT); pinMode(LPWM2, OUTPUT);
  pinMode(R_EN2, OUTPUT); pinMode(L_EN2, OUTPUT);

  // Enable drivers
  digitalWrite(R_EN1, HIGH); digitalWrite(L_EN1, HIGH);
  digitalWrite(R_EN2, HIGH); digitalWrite(L_EN2, HIGH);

  // Motors OFF on startup
  stopMotors();

  // Encoder pins — DISABLED (no encoders connected)
  // Floating pins generate noise that confuses the ROS 2 controller
  // pinMode(irPinR, INPUT);
  // pinMode(irPinL, INPUT);
  // attachInterrupt(digitalPinToInterrupt(irPinR), countRight, RISING);
  // attachInterrupt(digitalPinToInterrupt(irPinL), countLeft, RISING);

  // OLED: show startup screen ONCE, then never touch it again
  if (display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.println("Robot Ready");
    display.println("Waiting for");
    display.println("ROS 2 commands...");
    display.display();
  }
  delay(500);
}

// ---------------- INTERRUPTS ----------------
void countRight() {
  unsigned long now = millis();
  if (now - lastTickTimeR > debounceDelay) {
    totalTicksR++;
    lastTickTimeR = now;
  }
}

void countLeft() {
  unsigned long now = millis();
  if (now - lastTickTimeL > debounceDelay) {
    totalTicksL++;
    lastTickTimeL = now;
  }
}

// ---------------- MAIN LOOP ----------------
// This loop MUST be as fast as possible. No OLED, no Serial.print debug output.
// The Pi sends commands at 50Hz. We must respond within a few ms.
void loop() {
  // 1. Read and process ALL available serial bytes immediately
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      buffer[bufPos] = '\0';
      processCommand(buffer);
      bufPos = 0;
    } else if (bufPos < 31) {
      buffer[bufPos++] = c;
    }
  }

  // 2. Safety: stop motors if no command received recently
  if (millis() - lastCommandTime > commandTimeout) {
    stopMotors();
  }
}

// ---------------- MOTOR CONTROL ----------------
void setMotors(int speedRight, int speedLeft) {
  speedRight = constrain(speedRight, -255, 255);
  speedLeft  = constrain(speedLeft, -255, 255);

  // Right Motor
  if (speedRight >= 0) {
    analogWrite(RPWM1, speedRight);
    analogWrite(LPWM1, 0);
  } else {
    analogWrite(RPWM1, 0);
    analogWrite(LPWM1, -speedRight);
  }

  // Left Motor
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

// ---------------- COMMAND PROCESSOR ----------------
void processCommand(char* cmd) {
  if (cmd[0] == 'm') {
    double valL, valR;
    if (sscanf(cmd, "m %lf %lf", &valL, &valR) == 2) {
      pwmL = (valL / MAX_RAD_S) * 255;
      pwmR = (valR / MAX_RAD_S) * 255;
      lastCommandTime = millis();
      setMotors(pwmR, pwmL);
    }
    Serial.println("m");
  } else if (cmd[0] == 'e') {
    Serial.print(totalTicksL);
    Serial.print(" ");
    Serial.println(totalTicksR);
  } else if (cmd[0] == 'p') {
    Serial.println("p");
  } else if (strcmp(cmd, "stop") == 0) {
    stopMotors();
    Serial.println("s");
  } else if (strcmp(cmd, "reset") == 0) {
    noInterrupts();
    totalTicksR = totalTicksL = 0;
    interrupts();
    Serial.println("r");
  } else {
    Serial.println("ok");
  }
}
