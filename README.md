# ESP32-CAM RC Car

Building a WiFi RC car from scratch: an ESP32-CAM for camera + WiFi, an
Arduino Uno for motor control, and an L298N driving the wheels. End goal:
drive it from a phone with live video.

## Hardware

| Part | Role |
|---|---|
| Arduino Uno | Motor control |
| ESP32-CAM (AI-Thinker, OV2640) | WiFi + camera (not wired in yet) |
| L298N dual H-bridge | Drives the left/right motor groups |
| DC gear motors + wheels | Drive train |
| 2× 14500 Li-ion in series (~7.4 V) | Motor / board supply |

## Power notes

- 2× 1.5 V AA cells are not enough: everything boots on USB, nothing boots
  on battery. The L298N + Uno need at least ~7 V on the motor supply.
- Switching to a pair of 14500 Li-ion cells in series.

## Motor test

`motor_test/motor_test.ino` — upload with the Arduino IDE; both motors
should spin forward for one second, then stop.
