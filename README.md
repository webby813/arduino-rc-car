# ESP32-CAM RC Car

A WiFi RC car with live video, driven from a web page on your phone.

Two boards working together:

- **ESP32-CAM** (`esp32cam-car/`) — Wi-Fi video stream + web control page. Sends
  drive commands over UART to the Uno.
- **Arduino Uno** (`uno-motors/`) — drives the L298N motors, listening to the ESP32.

## Hardware

| Part | Role |
|---|---|
| ESP32-CAM (AI-Thinker, OV2640) | Video stream, WiFi, web control page |
| Arduino Uno | Motor control, listens to the ESP32 over SoftwareSerial |
| L298N dual H-bridge | Drives the left/right motor groups |
| DC gear motors + wheels | Drive train |
| 2× 14500 Li-ion in series (~7.4 V) | Motor / Uno supply — at least 7 V required |
| 5 V phone charger or buck converter (≥1 A) | Separate solid supply for the ESP32-CAM |

`motor_test/motor_test.ino` is a standalone L298N smoke test.

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

## Tools (VS Code)

1. Install [VS Code](https://code.visualstudio.com).
2. Extensions → install **PlatformIO IDE**. It downloads the ESP32 + AVR
   toolchains automatically the first time you build.
3. Open each folder (`esp32cam-car`, `uno-motors`) as its own PlatformIO project,
   or open this whole folder and use the PlatformIO sidebar to pick environments.

CLI equivalents (if you prefer the terminal):
```bash
pio run -d esp32cam-car                 # build ESP32 firmware
pio run -d esp32cam-car -t upload       # flash ESP32 (GPIO0->GND first!)
pio run -d uno-motors -t upload         # flash the Uno
pio device monitor -b 115200            # serial monitor
```

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

1. Edit `esp32cam-car/src/main.cpp` → set `WIFI_SSID` / `WIFI_PASS`.
2. `GPIO0` must be tied to `GND` (flash mode).
3. Upload. When the log shows `Connecting....____`, press **RST** on the ESP32-CAM.
4. After "Done", **remove the GPIO0–GND wire** and press **RST**.
5. Open the serial monitor at 115200 → note the printed IP address.

> **Power tip:** if upload fails with "Failed to connect" or you see brownout
> resets, power the ESP32-CAM from a separate solid 5V source (not the Uno's 5V),
> sharing GND. This is the #1 cause of grief.

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

Open `http://<esp32-ip>/` on your phone (same Wi-Fi). You'll see the video and a
button pad. Hold a button to move, release to stop.

---

## Tuning / troubleshooting

- **Laggy video** → in `initCamera()` lower `frame_size` or
  raise the `jpeg_quality` number (worse quality, less data).
- **`Camera init failed 0x...`** → almost always power or a reseated camera ribbon.
- **Motors run wrong direction** → swap that motor's two `IN` pins (or the wires).
- **Nothing moves but stream works** → check ESP32 `GPIO13`→Uno `pin 2` and the
  shared GND; watch the Uno serial monitor for `got: F` lines.
