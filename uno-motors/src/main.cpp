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

// --- L298N pins ---
const int IN1 = 12;  // right motor (OUT1/OUT2)
const int IN2 = 11;
const int IN3 = 10;  // left motor (OUT3/OUT4)
const int IN4 = 9;
const int ENA = 6;   // enable / speed (PWM)
const int ENB = 5;

const int SPEED_NORMAL = 200; // 0-255 PWM duty (boot default)
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

// --- Motor direction --------------------------------------------------------
// The two gear motors are mounted FACING EACH OTHER (mirror image), so the same
// electrical polarity spins them in OPPOSITE directions in the real world. We
// encode that mirroring in ONE place below: setLeft()/setRight() take a
// WORLD-space direction (+1 = that wheel drives the car forward, -1 = back,
// 0 = coast). The right motor's pins are simply the inverse of the left's.
//
// Tuning if a direction comes out wrong:
//   * Whole car drives backwards on 'F'  -> swap BOTH lines: flip LEFT_FWD and RIGHT_FWD.
//   * Only one side runs the wrong way   -> flip just that side's constant.
//   * Left/right turns swapped           -> swap the L and R cases in handle().
const bool LEFT_FWD  = HIGH;  // left motor (IN3/IN4): forward = IN3 LEFT_FWD , IN4 !LEFT_FWD
const bool RIGHT_FWD = LOW;   // right motor (IN1/IN2): forward = IN1 RIGHT_FWD, IN2 !RIGHT_FWD (whole right channel reversed to match left)

void setLeft(int dir) {   // IN3/IN4, enabled by ENB
  if (dir == 0) { digitalWrite(IN3, LOW); digitalWrite(IN4, LOW); return; }
  bool fwd = dir > 0;
  digitalWrite(IN3, fwd ?  LEFT_FWD : !LEFT_FWD);
  digitalWrite(IN4, fwd ? !LEFT_FWD :  LEFT_FWD);
}

void setRight(int dir) {  // IN1/IN2, enabled by ENA
  if (dir == 0) { digitalWrite(IN1, LOW); digitalWrite(IN2, LOW); return; }
  bool fwd = dir > 0;
  digitalWrite(IN1, fwd ?  RIGHT_FWD : !RIGHT_FWD);
  digitalWrite(IN2, fwd ? !RIGHT_FWD :  RIGHT_FWD);
}

void drive(int leftDir, int rightDir) {
  setLeft(leftDir); 
  setRight(rightDir);
  analogWrite(ENA, speedLevel); analogWrite(ENB, speedLevel);
}

void forward()  { drive(+1, +1); }  // both wheels roll the car forward
void backward() { drive(-1, -1); }  // both wheels roll the car backward
void left()     { drive(-1, +1); }  // spin in place: left back, right forward
void right()    { drive(+1, -1); }  // spin in place: left forward, right back

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
