# ESP32-CAM RC Car

A WiFi RC car with live video, driven from an open-source Flutter app.
The car broadcasts its **own WiFi hotspot** — no router or internet needed.

## Concept

One cheap camera board does all the smart work, one Arduino does the
muscle work, and a phone is the remote:

- **ESP32-CAM** (`esp32cam-car/`) — broadcasts the `RC-CAR` hotspot, streams
  MJPEG video, accepts WebSocket control from the app (drive, turbo, camera,
  headlight), and relays drive commands over UART to the Uno. Also serves a
  minimal fallback web page.
- **Arduino Uno** (`uno-motors/`) — drives the L298N motors, with turbo/normal
  speed levels and a UART watchdog failsafe.
- **Flutter app** (`controller/`) — the controller: Game Boy-style portrait
  layout, PPSSPP-style landscape layout, live video, turbo, camera + headlight
  toggles. See `controller/README.md`.

```
 Phone (Flutter app)                     ESP32-CAM (hotspot RC-CAR)
   ws://192.168.4.1/ws  ──────────────▶  control + failsafe watchdog
   http://192.168.4.1:81/stream ◀─────  MJPEG ~20fps                │ UART
                                         Uno + L298N motors  ◀──────┘
```

## Hardware

| Part | Role / notes |
|---|---|
| ESP32-CAM (AI-Thinker, OV2640) | WiFi hotspot, video stream, control endpoint, headlight (flash LED) |
| Arduino Uno | Motor control, listens to the ESP32 over SoftwareSerial |
| L298N dual H-bridge | Drives the left/right motor groups |
| DC gear motors + wheels | Drive train |
| 2× 14500 Li-ion in series (~7.4 V) | Motor / Uno supply — **at least 7 V is required**; 2× AA (3 V) will not boot the boards off USB |
| 5 V phone charger or buck converter (≥1 A) | Separate solid supply for the ESP32-CAM (sharing the Uno's 5 V causes brownouts) |

`motor_test/motor_test.ino` is a standalone L298N smoke test: upload it with
any Arduino toolchain and both motors should spin forward for one second.

## Software

| Folder | What it is | Toolchain |
|---|---|---|
| `esp32cam-car/` | ESP32-CAM firmware (C++) | PlatformIO, `espressif32` |
| `uno-motors/` | Uno motor firmware (C++) | PlatformIO, `atmelavr` |
| `motor_test/` | Bare L298N test sketch | Arduino IDE or PlatformIO |
| `controller/` | Flutter controller app (Android/iOS) | Flutter SDK |

## Why VS Code + PlatformIO instead of the Arduino IDE

The firmware is plain C++ on the Arduino framework, but built with
[PlatformIO](https://platformio.org) from VS Code / the terminal:

- **Command-line workflow** — `pio run` to build, `pio run -t upload` to
  flash, `pio device monitor` for serial. Much faster to iterate and debug
  than clicking through an IDE, and it fits a normal daily dev setup.
- **Reproducible builds** — each board is its own project with a versioned
  `platformio.ini` (board, ports, build flags like PSRAM). Nothing hides in
  IDE menu state; clone and build.
- **Two different chips, one workspace** — ESP32 (`espressif32`) and Uno
  (`atmelavr`) live side by side; PlatformIO downloads both toolchains
  automatically on first build.
- **Real editor tooling** — IntelliSense, jump-to-definition, git
  integration, and the Flutter app in the same window.

## Getting started

1. Install [VS Code](https://code.visualstudio.com) and the **PlatformIO IDE**
   extension (or `pipx install platformio` for CLI-only).
2. For the app, install the [Flutter SDK](https://docs.flutter.dev/get-started/install).
3. Clone this repo and open it in VS Code — use the PlatformIO sidebar to pick
   the `esp32cam` / `uno` environments, or work from the terminal:

```bash
pio run -d esp32cam-car                 # build ESP32 firmware
pio run -d esp32cam-car -t upload       # flash ESP32 (GPIO0->GND first!)
pio run -d uno-motors -t upload         # flash the Uno
pio device monitor -b 115200            # serial monitor
cd controller && flutter run            # run the app on a connected phone
```

> Set `upload_port` / `monitor_port` in each `platformio.ini` to whatever
> serial device your board shows up as.

## Control protocol

Single-character WebSocket messages (UART to the Uno uses the same drive chars):

| Char | Meaning | Handled by |
|---|---|---|
| `F` `B` `L` `R` | drive while held | Uno |
| `S` | stop | Uno |
| `T` / `N` | turbo (PWM 255) / normal (PWM 200) | Uno |
| `C1` / `C0` | camera stream on / off | ESP32 |
| `H1` / `H0` | headlight (GPIO 4 flash LED) on / off | ESP32 |
| `P` | heartbeat, app sends every 250 ms | ESP32 |

**Failsafe:** the ESP32 stops the car after 600 ms of control silence; the Uno
independently stops after 1 s of UART silence (the ESP32 re-sends the active
command every 300 ms while driving). Either board failing still stops the car.

**Hotspot credentials:** SSID `RC-CAR`, password `rccar1234` by default. To use
your own, copy `.env.dev` to `.env` (gitignored) and edit it — the values are
injected into the firmware at build time. Change the password before sharing
your car.

---

## STEP 1 — Flash the ESP32-CAM (Uno used as USB bridge)

The Uno here is ONLY a USB-to-serial passthrough. Wire it like this:

| Uno              | ESP32-CAM         |
|------------------|-------------------|
| `RESET` → `GND`  | *(jumper on the Uno — disables the ATmega)* |
| `5V`             | `5V`              |
| `GND`            | `GND`             |
| `RX (0)`         | `U0R` (GPIO3)     |
| `TX (1)`         | `U0T` (GPIO1)     |
| —                | `GPIO0` → `GND`   |

1. (Optional) `cp .env.dev .env` and set your own `AP_SSID` / `AP_PASS` there.
2. `GPIO0` must be tied to `GND` (flash mode).
3. Upload. When the log shows `Connecting....____`, press **RST** on the ESP32-CAM.
4. After "Done", **remove the GPIO0–GND wire** and press **RST**.
5. The serial monitor (115200) prints: `Ready! Hotspot "RC-CAR" -> http://192.168.4.1/`

> **Power tip:** if upload fails with "Failed to connect" or you see brownout
> resets, power the ESP32-CAM from a separate solid 5V source (not the Uno's 5V),
> sharing GND. This is the #1 cause of grief. The headlight LED at full
> brightness makes a weak supply even more likely to brown out.

## STEP 2 — Flash the Uno with motor code

1. Undo the `RESET`→`GND` jumper and unplug it from the ESP32.
2. With the Uno on USB by itself: `pio run -d uno-motors -t upload`.

## STEP 3 — Wire the two together for driving

| ESP32-CAM        | Uno            |
|------------------|----------------|
| `GPIO13` (TX)    | pin `2` (RX)   |
| `GND`            | `GND`          |

Power the L298N motor supply as usual; power the ESP32-CAM from 5V.
Both boards **must share a common GND**.

## STEP 4 — Drive

**With the app (recommended):**

1. Build and install the app on your phone: `cd controller && flutter run`
   (or `flutter build apk --release` and sideload the APK).
2. Power on the car, then join the **`RC-CAR`** WiFi hotspot in your phone's
   settings (password `rccar1234`). Stay connected even if the phone warns
   there's no internet.
3. Open the app — it connects automatically. Hold the D-pad to drive, hold
   **TURBO** for full speed, toggle camera and headlight from the top bar.

**Without the app (fallback):** join the hotspot and open `http://192.168.4.1/`
in any browser. You get the basic button pad and video (no turbo/headlight).

---

## Tuning / troubleshooting

- **Laggy video** → in `esp32cam-car/src/main.cpp` lower `VIDEO_FRAME_SIZE` to
  `FRAMESIZE_QQVGA` or raise `VIDEO_JPEG_QUALITY` (worse image, less data).
- **`Camera init failed 0x...`** → almost always power or a reseated camera ribbon.
- **Motors run wrong direction** → swap that motor's two `IN` pins (or the wires).
- **App stuck on "Looking for the car"** → confirm the phone is on the `RC-CAR`
  network (phones silently fall back to other WiFi/mobile data); on iOS check
  Settings → RC Car Controller → Local Network is allowed.
- **Nothing moves but stream works** → check ESP32 `GPIO13`→Uno `pin 2` and the
  shared GND; watch the Uno serial monitor for `got: F` lines.
- **Car stops by itself while driving** → that's the failsafe noticing a dropped
  control link; check WiFi range/interference. The Uno also stops if the UART
  wire comes loose.
