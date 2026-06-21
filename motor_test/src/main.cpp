// ===========================================================================
//  LEFT MOTOR REVERSE DIAGNOSTIC
//  Isolates the left motor (IN3=10, IN4=9, enabled by ENB=5) and cycles it:
//      FORWARD 2s  ->  STOP 1s  ->  REVERSE 2s  ->  STOP 1s  ->  repeat
//
//  Forward is already known to work; reverse is the failing case. While each
//  phase runs, the Serial monitor prints which pin should be HIGH so you can
//  probe with a multimeter and find where the signal dies.
//
//  WHAT TO MEASURE during the REVERSE phase (IN4 / pin 9 should be HIGH):
//    1) Arduino pin 9   -> expect ~5V.  If 0V  => Arduino pin 9 is dead.
//    2) L298N IN4 input -> expect ~5V.  5V at pin 9 but 0V here => broken wire.
//    3) L298N OUT3/OUT4 (the two terminals going to the left motor):
//         FORWARD phase: one terminal high, other low (motor spins).
//         REVERSE phase: they should SWAP. If they DON'T swap while IN4=5V
//         => the L298N's IN4/OUT4 channel is blown (replace the L298N).
// ===========================================================================

#include <Arduino.h>

const int IN3 = 10;  // left motor input A
const int IN4 = 9;   // left motor input B  (HIGH only in reverse)
const int ENB = 5;   // left motor enable (PWM)

void leftStop() {
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  analogWrite(ENB, 0);
}

void setup() {
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(ENB, OUTPUT);
  Serial.begin(9600);
  Serial.println("LEFT MOTOR REVERSE DIAGNOSTIC");
}

void loop() {
  // FORWARD: IN3 HIGH, IN4 LOW  (known-good baseline)
  Serial.println(">>> FORWARD  : pin10(IN3)=HIGH  pin9(IN4)=LOW   (should spin)");
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  analogWrite(ENB, 200);
  delay(2000);

  Serial.println("    stop");
  leftStop();
  delay(1000);

  // REVERSE: IN3 LOW, IN4 HIGH  (the failing case -- probe pin 9 / IN4 / OUT4 now)
  Serial.println(">>> REVERSE  : pin10(IN3)=LOW   pin9(IN4)=HIGH  (probe here!)");
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
  analogWrite(ENB, 200);
  delay(2000);

  Serial.println("    stop");
  leftStop();
  delay(1000);
}
