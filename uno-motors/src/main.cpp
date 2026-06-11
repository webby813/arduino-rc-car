// ===========================================================================
//  Arduino Uno  —  L298N motor driver
//  Listens for single-char commands from the ESP32-CAM and drives the motors.
//    F = forward   B = back   L = left   R = right   S = stop
//    T = turbo speed (PWM 255)   N = normal speed (PWM 200, boot default)
//
//  Failsafe: while moving, if no UART byte arrives for WATCHDOG_MS the motors
//  stop. The ESP32 re-sends the active drive command every 300 ms, so a
//  healthy link never trips this; repeated characters are a no-op refresh.
//
//  Wiring to ESP32-CAM:
//    ESP32 GPIO13 (TX) -> Uno pin 2 (SoftwareSerial RX)
//    ESP32 GND         -> Uno GND   (common ground is REQUIRED)
//  We use SoftwareSerial on pins 2/3 so the USB serial (pins 0/1) stays free
//  for uploading and debugging.
// ===========================================================================

#include <Arduino.h>
#include <SoftwareSerial.h>

// --- L298N pins (your original layout) ---
const int IN1 = 12;  // left motor
const int IN2 = 11;
const int IN3 = 10;  // right motor
const int IN4 = 9;
const int ENA = 6;   // enable / speed (PWM)
const int ENB = 5;   // second enable — wire to L298N ENB (or jumper it on the board)

const int SPEED_NORMAL = 200; // 0-255 PWM duty
const int SPEED_TURBO  = 255;

const unsigned long WATCHDOG_MS = 1000; // stop if UART silent this long while moving

SoftwareSerial espSerial(2, 3); // RX = 2 (from ESP32 TX), TX = 3 (unused)

int speedLevel = SPEED_NORMAL;
char moveState = 'S';           // current motion (S = stopped)
unsigned long lastRxMs = 0;     // last time any UART byte arrived

void stopMotors() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
  analogWrite(ENA, 0);    analogWrite(ENB, 0);
  moveState = 'S';
}

void drive(bool l1, bool l2, bool r1, bool r2) {
  digitalWrite(IN1, l1); digitalWrite(IN2, l2);
  digitalWrite(IN3, r1); digitalWrite(IN4, r2);
  analogWrite(ENA, speedLevel); analogWrite(ENB, speedLevel);
}

void forward()  { drive(LOW, HIGH, HIGH, LOW); }
void backward() { drive(HIGH, LOW, LOW, HIGH); }
void left()     { drive(HIGH, LOW, HIGH, LOW); }  // spin in place
void right()    { drive(LOW, HIGH, LOW, HIGH); }

// Re-apply PWM so a speed change takes effect on motion already in progress.
void applySpeed() {
  if (moveState != 'S') {
    analogWrite(ENA, speedLevel);
    analogWrite(ENB, speedLevel);
  }
}

void handle(char c) {
  switch (c) {
    case 'F': moveState = c; forward();  break;
    case 'B': moveState = c; backward(); break;
    case 'L': moveState = c; left();     break;
    case 'R': moveState = c; right();    break;
    case 'T': speedLevel = SPEED_TURBO;  applySpeed(); break;
    case 'N': speedLevel = SPEED_NORMAL; applySpeed(); break;
    case 'S':
    default:  stopMotors(); break;
  }
}

void setup() {
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  pinMode(ENA, OUTPUT); pinMode(ENB, OUTPUT);
  stopMotors();

  Serial.begin(9600);     // USB debug
  espSerial.begin(9600);  // from ESP32
  Serial.println("Uno motor controller ready.");
}

void loop() {
  if (espSerial.available()) {
    char c = espSerial.read();
    handle(c);
    lastRxMs = millis();
    Serial.print("got: "); Serial.println(c);
  }

  // Watchdog: ESP32 gone quiet while we're moving -> stop.
  if (moveState != 'S' && (millis() - lastRxMs) > WATCHDOG_MS) {
    stopMotors();
    Serial.println("watchdog: UART silent, motors stopped");
  }
}
