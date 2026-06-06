// ===========================================================================
//  ESP32-CAM RC Car  —  video stream + Wi-Fi control (Option B)
//  - Streams MJPEG video on  http://<ip>:81/stream
//  - Serves a control page on http://<ip>/
//  - Sends single-char drive commands (F/B/L/R/S) over UART to the Arduino Uno
//    which actually drives the L298N motors.
// ===========================================================================

#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"

// ----------------------- USER CONFIG --------------------------------------
const char *WIFI_SSID = "YourWiFiName";
const char *WIFI_PASS = "YourWiFiPassword";

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

#define PART_BOUNDARY "123456789000000000000987654321"
static const char *STREAM_CONTENT_TYPE =
    "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char *STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

// ----------------------- CONTROL PAGE -------------------------------------
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

// ----------------------- HTTP HANDLERS ------------------------------------
static esp_err_t index_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, INDEX_HTML, strlen(INDEX_HTML));
}

static esp_err_t cmd_handler(httpd_req_t *req) {
  char query[32];
  char value[4] = {0};
  if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
    if (httpd_query_key_value(query, "go", value, sizeof(value)) == ESP_OK) {
      char c = value[0];
      if (c == 'F' || c == 'B' || c == 'L' || c == 'R' || c == 'S') {
        UnoSerial.write(c);            // forward command to the Uno
        Serial.printf("cmd -> %c\n", c);
      }
    }
  }
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, "ok", 2);
}

static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t *fb = NULL;
  esp_err_t res = httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
  if (res != ESP_OK) return res;
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

  char part_buf[64];
  while (true) {

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

  }
  return res;
}

static void startServers() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

  // Control server on port 80
  config.server_port = 80;
  config.ctrl_port   = 32768;
  if (httpd_start(&controlServer, &config) == ESP_OK) {
    httpd_uri_t index_uri = {"/",    HTTP_GET, index_handler, NULL};
    httpd_uri_t cmd_uri   = {"/cmd", HTTP_GET, cmd_handler,   NULL};
    httpd_register_uri_handler(controlServer, &index_uri);
    httpd_register_uri_handler(controlServer, &cmd_uri);
  }

  // Stream server on port 81
  config.server_port = 81;
  config.ctrl_port   = 32769;
  if (httpd_start(&streamServer, &config) == ESP_OK) {
    httpd_uri_t stream_uri = {"/stream", HTTP_GET, stream_handler, NULL};
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
    config.frame_size   = FRAMESIZE_QVGA;  // 320x240
    config.jpeg_quality = 12;              // 10=best quality, 63=smallest file
    config.fb_count     = 2;
    config.grab_mode    = CAMERA_GRAB_LATEST;
    config.fb_location  = CAMERA_FB_IN_PSRAM;
  } else {
    config.frame_size   = FRAMESIZE_QVGA;  // 320x240
    config.jpeg_quality = 12;
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

  UnoSerial.begin(UNO_BAUD, SERIAL_8N1, UNO_RX_PIN, UNO_TX_PIN);

  if (!initCamera()) {
    Serial.println("Halting: fix camera wiring/power and reboot.");
    return;
  }

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  WiFi.setSleep(false);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) { delay(400); Serial.print("."); }
  Serial.println();

  startServers();
  Serial.print("Ready! Open  http://");
  Serial.println(WiFi.localIP());
}

void loop() {
  delay(1000); // everything runs in the HTTP server tasks
}
