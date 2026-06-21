# RC Car Controller (Flutter)

Open-source controller app for the ESP32-CAM RC car in this repository.
Connects directly to the car's own WiFi hotspot — no router or internet needed.

- **Portrait**: Game Boy style — live video on the top half, D-pad + TURBO below.
- **Landscape**: PPSSPP style — fullscreen video with translucent overlay controls.
- Camera and headlight toggles, automatic failsafe stop on connection loss.

## Build & install

Requires the [Flutter SDK](https://docs.flutter.dev/get-started/install) (Android and/or Xcode toolchains).

The hotspot name/password come from the **repo-root `.env`** at build time — the
same file the ESP32 firmware reads (`load_env.py`), so one file drives both.
Pass it with `--dart-define-from-file=../.env`:

```bash
flutter pub get
flutter run   --dart-define-from-file=../.env                   # debug on a connected device
flutter build apk --release --dart-define-from-file=../.env     # Android APK -> build/app/outputs/flutter-apk/
flutter build ios --release --dart-define-from-file=../.env     # iOS (requires Xcode signing setup)
```

Omit the flag and the app falls back to the defaults `RC-CAR` / `rccar1234`.
`AP_SSID` and `AP_PASS` in `.env` feed `CarConnection.hotspotName` /
`hotspotPassword` and the join-the-hotspot instructions on the connect screen.

## Use

1. Power on the car.
2. Join the WiFi hotspot **`RC-CAR`** (default password `rccar1234`; custom
   credentials go in the repo-root `.env`, see `.env.dev`). Stay connected
   even if the phone warns there is no internet.
3. Open the app — it finds the car at `192.168.4.1` automatically.

Hold a D-pad direction to drive, release to stop. Hold **TURBO** with your
other thumb for full speed. The top bar toggles the camera and headlight.

## How it talks to the car

| Channel | Transport | Purpose |
|---|---|---|
| Control | `ws://192.168.4.1/ws` | `F/B/L/R/S` drive, `T/N` turbo, `C1/C0` camera, `H1/H0` headlight, `P` heartbeat (250 ms) |
| Video | `http://192.168.4.1:81/stream` | MJPEG ~20 fps |

If the control link goes silent for 600 ms the car stops itself (and the Uno
has its own 1 s UART watchdog as a second layer).

## Dependencies

[`flutter_mjpeg`](https://pub.dev/packages/flutter_mjpeg) ·
[`web_socket_channel`](https://pub.dev/packages/web_socket_channel) ·
[`shared_preferences`](https://pub.dev/packages/shared_preferences)

Licensed under the MIT License (see `../LICENSE`).
