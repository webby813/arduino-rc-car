// ===========================================================================
//  ESP32-CAM RC Car  —  SoftAP + WebSocket control + MJPEG video
//  - Broadcasts its own Wi-Fi hotspot (no router needed), fixed IP 192.168.4.1
//  - WebSocket control endpoint on ws://192.168.4.1/ws  (Flutter app)
//  - Fallback web control page on    http://192.168.4.1/
//  - Streams MJPEG video on          http://192.168.4.1:81/stream
//  - Relays single-char drive commands over UART to the Arduino Uno (L298N)
//
//  Control protocol (WebSocket text messages):
//    F/B/L/R  drive while held        S  stop
//    T/N      turbo on / normal speed
//    C1/C0    camera stream on / off
//    H1/H0    headlight (flash LED) on / off
//    P        heartbeat ping (app sends every 250 ms)
//
//  Failsafe: if the control socket goes silent for CTRL_TIMEOUT_MS the car
//  stops. While driving, the active command is re-sent to the Uno every
//  UART_KEEPALIVE_MS so the Uno's own 1 s watchdog stays fed.
// ===========================================================================

#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"

// ----------------------- USER CONFIG --------------------------------------
// Hotspot credentials the car broadcasts. Don't edit here — put your own in
// a .env file at the repo root (copy .env.dev); load_env.py injects them at
// build time. These are only the fallback defaults.
#ifndef AP_SSID
#define AP_SSID "RC-CAR"
#endif
#ifndef AP_PASS
#define AP_PASS "rccar1234" // WPA2 requires at least 8 characters
#endif

// Video tuning (single-point tunables; see doc/notes.md)
#define VIDEO_FRAME_SIZE   FRAMESIZE_QVGA // 320x240 over the direct AP link
#define VIDEO_JPEG_QUALITY 20             // 10=best quality, 63=smallest file

// Failsafe timings
#define CTRL_TIMEOUT_MS   600  // stop if WS control goes silent this long
#define UART_KEEPALIVE_MS 300  // re-send active drive char to feed Uno watchdog

// Headlight: onboard flash LED (GPIO 4 is free — SD card is not used)
#define HEADLIGHT_PIN 4

// UART that talks to the Uno.  ESP32 TX(GPIO13) -> Uno RX(pin 2).
// We only transmit, so the RX pin (14) is just a placeholder.
#define UNO_TX_PIN 13
#define UNO_RX_PIN 14
#define UNO_BAUD   9600
HardwareSerial UnoSerial(1); // use UART1

// --------------------- AI-THINKER CAMERA PINMAP ---------------------------
#define PWDN_GPIO_NUM  32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM   0
#define SIOD_GPIO_NUM  26
#define SIOC_GPIO_NUM  27
#define Y9_GPIO_NUM    35
#define Y8_GPIO_NUM    34
#define Y7_GPIO_NUM    39
#define Y6_GPIO_NUM    36
#define Y5_GPIO_NUM    21
#define Y4_GPIO_NUM    19
#define Y3_GPIO_NUM    18
#define Y2_GPIO_NUM     5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM  23
#define PCLK_GPIO_NUM  22

httpd_handle_t controlServer = NULL;
httpd_handle_t streamServer  = NULL;

// --------------------- SHARED STATE (httpd tasks <-> loop) ----------------
volatile int      ctrlFd      = -1;  // fd of the single WS control client
volatile uint32_t lastCtrlMs  = 0;   // last time any WS message arrived
volatile char     activeDrive = 'S'; // current drive command (S = stopped)
volatile uint32_t lastUartMs  = 0;   // last time a drive char went to the Uno
volatile bool     cameraOn    = true;
volatile bool     streamBusy  = false;

#define PART_BOUNDARY "123456789000000000000987654321"
static const char *STREAM_CONTENT_TYPE =
    "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char *STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

// ----------------------- CONTROL PAGE (fallback) ---------------------------
static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32-CAM RC Car</title><style>
body{font-family:sans-serif;text-align:center;background:#111;color:#eee;margin:0;padding:12px}
img{width:100%;max-width:480px;border-radius:8px;background:#000}
.pad{display:grid;grid-template-columns:repeat(3,90px);gap:10px;justify-content:center;margin-top:16px}
button{height:70px;font-size:22px;border:0;border-radius:10px;background:#2a6;color:#fff;
       -webkit-user-select:none;user-select:none;touch-action:manipulation}
button:active{background:#194}
.empty{visibility:hidden}
</style></head><body>
<h3>ESP32-CAM RC Car</h3>
<img id="stream" src="">
<div class="pad">
  <span class="empty"></span>
  <button onpointerdown="cmd('F')" onpointerup="cmd('S')">&#9650;</button>
  <span class="empty"></span>
  <button onpointerdown="cmd('L')" onpointerup="cmd('S')">&#9664;</button>
  <button onpointerdown="cmd('S')">&#9632;</button>
  <button onpointerdown="cmd('R')" onpointerup="cmd('S')">&#9654;</button>
  <span class="empty"></span>
  <button onpointerdown="cmd('B')" onpointerup="cmd('S')">&#9660;</button>
  <span class="empty"></span>
</div>
<script>
  // stream is served on port 81
  window.addEventListener('load',()=>{
    document.getElementById('stream').src =
      'http://' + location.hostname + ':81/stream';
  });
  function cmd(c){ fetch('/cmd?go=' + c); }
</script>
</body></html>
)rawliteral";

// ----------------------- CONTROL HANDLING ---------------------------------
static void sendToUno(char c) {
  UnoSerial.write(c);
  lastUartMs = millis();
}

static void handleDriveChar(char c) {
  activeDrive = c;
  sendToUno(c);
}

// Stop the car and reset speed; called when the control link is lost.
static void failsafeStop(const char *reason) {
  sendToUno('S');
  sendToUno('N');
  activeDrive = 'S';
  Serial.printf("FAILSAFE stop: %s\n", reason);
}

static void handleControlMsg(const char *msg, size_t len) {
  if (len == 0) return;
  char c = msg[0];
  switch (c) {
    case 'F': case 'B': case 'L': case 'R': case 'S':
      handleDriveChar(c);
      break;
    case 'T': case 'N':
      sendToUno(c);
      break;
    case 'C':
      if (len > 1) cameraOn = (msg[1] == '1');
      break;
    case 'H':
      if (len > 1) digitalWrite(HEADLIGHT_PIN, (msg[1] == '1') ? HIGH : LOW);
      break;
    case 'P': // heartbeat — arrival time is recorded by the caller
      break;
    default:
      Serial.printf("unknown ctrl msg: %c\n", c);
      break;
  }
}

// ----------------------- HTTP HANDLERS ------------------------------------
static esp_err_t index_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, INDEX_HTML, strlen(INDEX_HTML));
}

// Legacy endpoint used by the fallback web page. Goes through the same drive
// path so the UART keep-alive also feeds the Uno watchdog for web users.
static esp_err_t cmd_handler(httpd_req_t *req) {
  char query[32];
  char value[4] = {0};
  if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
    if (httpd_query_key_value(query, "go", value, sizeof(value)) == ESP_OK) {
      char c = value[0];
      if (c == 'F' || c == 'B' || c == 'L' || c == 'R' || c == 'S') {
        handleDriveChar(c);
        Serial.printf("cmd -> %c\n", c);
      }
    }
  }
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, "ok", 2);
}

// WebSocket control endpoint. One client at a time: newest connection wins.
static esp_err_t ws_handler(httpd_req_t *req) {
  if (req->method == HTTP_GET) { // handshake completed
    int fd = httpd_req_to_sockfd(req);
    int old = ctrlFd;
    if (old >= 0 && old != fd) {
      httpd_sess_trigger_close(controlServer, old);
    }
    ctrlFd = fd;
    lastCtrlMs = millis();
    Serial.printf("WS control client connected (fd %d)\n", fd);
    return ESP_OK;
  }

  httpd_ws_frame_t frame = {};
  frame.type = HTTPD_WS_TYPE_TEXT;
  esp_err_t ret = httpd_ws_recv_frame(req, &frame, 0); // query frame length
  if (ret != ESP_OK) return ret;

  uint8_t buf[8] = {0};
  if (frame.len > 0 && frame.len < sizeof(buf)) {
    frame.payload = buf;
    ret = httpd_ws_recv_frame(req, &frame, frame.len);
    if (ret != ESP_OK) return ret;
    handleControlMsg((const char *)buf, frame.len);
  }
  lastCtrlMs = millis();
  return ESP_OK;
}

static esp_err_t stream_handler(httpd_req_t *req) {
  if (!cameraOn || streamBusy) { // camera off, or one viewer already active
    httpd_resp_set_status(req, "503 Service Unavailable");
    return httpd_resp_send(req, NULL, 0);
  }
  streamBusy = true;

  camera_fb_t *fb = NULL;
  esp_err_t res = httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
  if (res == ESP_OK) {
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    char part_buf[64];
    while (cameraOn) {
      int64_t fr_start = esp_timer_get_time();

      fb = esp_camera_fb_get();
      if (!fb) { res = ESP_FAIL; break; }

      size_t hlen = snprintf(part_buf, sizeof(part_buf), STREAM_PART, fb->len);
      if (httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY)) != ESP_OK ||
          httpd_resp_send_chunk(req, part_buf, hlen) != ESP_OK ||
          httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len) != ESP_OK) {
        esp_camera_fb_return(fb);
        break;
      }
      esp_camera_fb_return(fb);

      // Cap at ~20fps — prevents flooding the connection
      int64_t elapsed = (esp_timer_get_time() - fr_start) / 1000;
      if (elapsed < 50) delay(50 - elapsed);
    }
  }

  streamBusy = false;
  return res;
}

static void startServers() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

  // Control server on port 80
  config.server_port = 80;
  config.ctrl_port   = 32768;
  if (httpd_start(&controlServer, &config) == ESP_OK) {
    httpd_uri_t index_uri = {};
    index_uri.uri     = "/";
    index_uri.method  = HTTP_GET;
    index_uri.handler = index_handler;

    httpd_uri_t cmd_uri = {};
    cmd_uri.uri     = "/cmd";
    cmd_uri.method  = HTTP_GET;
    cmd_uri.handler = cmd_handler;

    httpd_uri_t ws_uri = {};
    ws_uri.uri          = "/ws";
    ws_uri.method       = HTTP_GET;
    ws_uri.handler      = ws_handler;
    ws_uri.is_websocket = true;

    httpd_register_uri_handler(controlServer, &index_uri);
    httpd_register_uri_handler(controlServer, &cmd_uri);
    httpd_register_uri_handler(controlServer, &ws_uri);
  }

  // Stream server on port 81
  config.server_port = 81;
  config.ctrl_port   = 32769;
  if (httpd_start(&streamServer, &config) == ESP_OK) {
    httpd_uri_t stream_uri = {};
    stream_uri.uri     = "/stream";
    stream_uri.method  = HTTP_GET;
    stream_uri.handler = stream_handler;
    httpd_register_uri_handler(streamServer, &stream_uri);
  }
}

// ----------------------- CAMERA INIT --------------------------------------
static bool initCamera() {
  camera_config_t config = {};
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM; config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM; config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM; config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // With PSRAM we can afford a bigger frame + double buffering.
  if (psramFound()) {
    config.frame_size   = VIDEO_FRAME_SIZE;
    config.jpeg_quality = VIDEO_JPEG_QUALITY;
    config.fb_count     = 2;
    config.grab_mode    = CAMERA_GRAB_LATEST;
    config.fb_location  = CAMERA_FB_IN_PSRAM;
  } else {
    config.frame_size   = FRAMESIZE_QQVGA; // no PSRAM: stay small
    config.jpeg_quality = 35;
    config.fb_count     = 1;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed 0x%x\n", err);
    return false;
  }
  return true;
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(false);

  pinMode(HEADLIGHT_PIN, OUTPUT);
  digitalWrite(HEADLIGHT_PIN, LOW); // headlight off at boot

  UnoSerial.begin(UNO_BAUD, SERIAL_8N1, UNO_RX_PIN, UNO_TX_PIN);

  if (!initCamera()) {
    Serial.println("Halting: fix camera wiring/power and reboot.");
    return;
  }

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  WiFi.setSleep(false);

  startServers();
  Serial.printf("Ready! Hotspot \"%s\"  ->  http://%s/\n",
                AP_SSID, WiFi.softAPIP().toString().c_str());
}

void loop() {
  uint32_t now = millis();

  // Failsafe: WS control client went silent (covers app kill, out of range,
  // and socket close — all of them stop the message flow).
  if (ctrlFd >= 0 && (now - lastCtrlMs) > CTRL_TIMEOUT_MS) {
    int fd = ctrlFd;
    ctrlFd = -1;
    httpd_sess_trigger_close(controlServer, fd);
    failsafeStop("control link silent");
  }

  // Keep-alive: while driving, refresh the Uno's UART watchdog.
  if (activeDrive != 'S' && (now - lastUartMs) > UART_KEEPALIVE_MS) {
    sendToUno(activeDrive);
  }

  delay(20);
}
