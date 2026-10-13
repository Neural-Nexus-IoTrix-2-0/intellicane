#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <esp_http_server.h>
#include "esp_camera.h"
#include "camera_pins.h"

#if __has_include("config.h")
  #include "config.h"
#elif __has_include("config.h.example")
  #include "config.h.example"
#else
  #error "Configuration file missing! Please copy config.h.example to config.h"
#endif

// ============================================================================
// Global State & Web Server
// ============================================================================
WebServer server(WEB_SERVER_PORT);
httpd_handle_t stream_httpd = NULL;

bool cameraInitialized = false;
bool flashState = false;
unsigned long lastTriggerTime = 0;
const unsigned long DEBOUNCE_DELAY_MS = 1000;

// Last scene analysis cache
String lastSpokenSummary = "Awaiting command. Press Describe to scan surroundings.";
String lastUrgentHazards = "None";
String lastWalkablePath = "Unknown";
String lastObjectsJson = "[]";
String lastTextSigns = "None";
unsigned long lastAnalysisTime = 0;
int lastLatencyMs = 0;

// MJPEG Stream Boundary Definition
#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

// Base64 lookup table
static const char B64_TABLE[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// ============================================================================
// Forward Declarations
// ============================================================================
void setupCamera();
void setupWiFi();
void setupWebServer();
void startCameraStreamServer();
void setStatusLed(bool on);
void setFlashLed(bool on);
camera_fb_t* captureFrame();
String performSceneDescription();
String callGeminiVision(camera_fb_t *fb);
void streamBase64(WiFiClientSecure &client, const uint8_t *data, size_t length);
void handleSerialCommands();
void checkHardwareTrigger();

// Web Server Handlers (Port 80)
void handleRoot();
void handleCapture();
void handleDescribe();
void handleFlashToggle();
void handleStatus();
void handleResolution();

// ============================================================================
// Setup
// ============================================================================
void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  delay(1000);

  Serial.println("\n================================================");
  Serial.println("  IntelliCane - ESP32-CAM AI Vision System     ");
  Serial.println("================================================");

  // Initialize LEDs & Trigger Pin
  pinMode(STATUS_LED_PIN, OUTPUT);
  pinMode(FLASH_LED_PIN, OUTPUT);
  setStatusLed(true);  // Turn on status LED during boot
  setFlashLed(false);

  pinMode(TRIGGER_PIN, INPUT_PULLUP);
  Serial.printf("[INIT] Hardware trigger configured on GPIO %d (active LOW)\n", TRIGGER_PIN);

  // Initialize Camera
  setupCamera();

  // Initialize Wi-Fi
  setupWiFi();

  // Initialize Web Control Server (Port 80)
  setupWebServer();

  // Initialize High-Speed MJPEG Video Stream (Port 81)
  startCameraStreamServer();

  setStatusLed(false); // Turn off status LED (ready)
  Serial.println("\n[READY] System operational.");
  Serial.println("Stream URL:   http://<ip>:81/stream");
  Serial.println("Dashboard:    http://<ip>/ or http://intellicane.local");
  Serial.println("Commands: DESCRIBE, CAPTURE, FLASH ON, FLASH OFF, STATUS, HELP");
  Serial.println("================================================\n");
}

// ============================================================================
// Main Loop
// ============================================================================
void loop() {
  server.handleClient();
  handleSerialCommands();
  checkHardwareTrigger();
  yield();
}

// ============================================================================
// Camera Configuration
// ============================================================================
void setupCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000; // 20 MHz clean pixel clock
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode    = CAMERA_GRAB_LATEST;

  if (psramFound()) {
    Serial.println("[CAM] PSRAM detected (4MB). Configuring VGA mode.");
    config.frame_size   = FRAMESIZE_VGA; // 640x480 - smooth 15-20 FPS stream
    config.jpeg_quality = 12;
    config.fb_count     = 2;
    config.fb_location  = CAMERA_FB_IN_PSRAM;
  } else {
    Serial.println("[CAM] WARNING: No PSRAM detected. Falling back to QVGA 320x240.");
    config.frame_size   = FRAMESIZE_QVGA;
    config.jpeg_quality = 14;
    config.fb_count     = 1;
    config.fb_location  = CAMERA_FB_IN_DRAM;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("[CAM ERROR] Camera initialization failed with error 0x%x\n", err);
    Serial.println("[CAM ERROR] Please ensure the OV2640 camera ribbon is securely seated.");
    cameraInitialized = false;
    return;
  }

  sensor_t *s = esp_camera_sensor_get();
  if (s != NULL) {
    s->set_vflip(s, 1);       // Cane mount upright orientation
    s->set_hmirror(s, 0);
    s->set_brightness(s, 1);  // Indoor clarity boost
    s->set_contrast(s, 1);
  }

  cameraInitialized = true;
  Serial.println("[CAM] Camera initialized successfully.");
}

// ============================================================================
// Wi-Fi Configuration
// ============================================================================
void setupWiFi() {
  Serial.printf("[WIFI] Connecting to '%s'...\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 10000) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[WIFI] Connected! IP Address: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("[WIFI] RSSI: %d dBm\n", WiFi.RSSI());
    if (MDNS.begin("intellicane")) {
      Serial.println("[MDNS] Hostname registered: http://intellicane.local");
    }
  } else {
    Serial.println("[WIFI] Primary Wi-Fi connection timed out.");
    Serial.printf("[WIFI] Starting fallback SoftAP hotspot '%s'...\n", SOFTAP_SSID);
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(SOFTAP_SSID, SOFTAP_PASS);
    Serial.printf("[WIFI] SoftAP active! IP Address: %s\n", WiFi.softAPIP().toString().c_str());
    Serial.printf("[WIFI] Connect to hotspot '%s' (password: '%s')\n", SOFTAP_SSID, SOFTAP_PASS);
    if (MDNS.begin("intellicane")) {
      Serial.println("[MDNS] Hostname registered: http://intellicane.local");
    }
  }
}

// ============================================================================
// LED Controls
// ============================================================================
void setStatusLed(bool on) {
  // Onboard red LED is active LOW on AI-Thinker (GPIO 33)
  digitalWrite(STATUS_LED_PIN, on ? LOW : HIGH);
}

void setFlashLed(bool on) {
  flashState = on;
  // Flashlight is active HIGH on GPIO 4
  digitalWrite(FLASH_LED_PIN, on ? HIGH : LOW);
}

// ============================================================================
// Ultra-Fast Frame Capture Helper
// ============================================================================
camera_fb_t* captureFrame() {
  if (!cameraInitialized) {
    Serial.println("[CAM] Error: Camera is not initialized.");
    return nullptr;
  }

  if (ENABLE_FLASH_ON_CAPTURE) {
    setFlashLed(true);
    delay(30);
  }

  // With CAMERA_GRAB_LATEST and fb_count=2, this returns the freshest frame instantly (<15ms)
  camera_fb_t *fb = esp_camera_fb_get();

  if (ENABLE_FLASH_ON_CAPTURE) {
    setFlashLed(false);
  }

  if (!fb) {
    Serial.println("[CAM ERROR] Failed to capture camera frame buffer.");
    return nullptr;
  }

  return fb;
}

// ============================================================================
// Scene Description Execution (Core Logic)
// ============================================================================
String performSceneDescription() {
  unsigned long startTime = millis();
  setStatusLed(true);

  Serial.println("\n--------------------------------------------------");
  Serial.println("[AI VISION] Capture & Analysis Triggered");

  camera_fb_t *fb = captureFrame();
  if (!fb) {
    setStatusLed(false);
    return "{\"status\":\"error\",\"message\":\"Camera capture failed\"}";
  }

  Serial.printf("[CAM] Frame captured: %u bytes (%dx%d)\n", fb->len, fb->width, fb->height);

  String resultJson = "";
  String apiKey = String(GEMINI_API_KEY);

  // Check if live Gemini API is configured
  if (apiKey.length() > 5 && apiKey != "YOUR_GEMINI_API_KEY") {
    Serial.println("[AI VISION] Querying Google Gemini Multimodal Vision API...");
    resultJson = callGeminiVision(fb);
  } else {
    // Bench test / fallback mode
    Serial.println("[AI VISION] Notice: GEMINI_API_KEY not configured. Running bench assistance model.");

    JsonDocument doc;
    doc["status"] = "success";
    doc["mode"] = "bench_simulation";
    doc["spoken_summary"] = "Path straight ahead at 12 o'clock is clear for 2 meters. A low obstacle is detected at 2 o'clock on your right. Step forward with caution.";

    JsonArray hazards = doc["urgent_hazards"].to<JsonArray>();
    hazards.add("Low stool/obstacle at 2 o'clock (1.2m away)");

    doc["walkable_path"] = "Clear corridor straight ahead at 12 o'clock.";

    JsonArray objects = doc["detected_objects"].to<JsonArray>();
    JsonObject obj1 = objects.add<JsonObject>();
    obj1["name"] = "doorway";
    obj1["position"] = "12 o'clock";
    obj1["distance"] = "2.5m";
    obj1["hazard_level"] = "none";

    JsonObject obj2 = objects.add<JsonObject>();
    obj2["name"] = "chair/stool";
    obj2["position"] = "2 o'clock";
    obj2["distance"] = "1.2m";
    obj2["hazard_level"] = "medium";

    doc["text_and_signs"] = "Room doorway visible straight ahead";
    doc["frame_bytes"] = fb->len;
    doc["latency_ms"] = millis() - startTime;

    serializeJson(doc, resultJson);
  }

  esp_camera_fb_return(fb);

  // Parse result to update cache
  JsonDocument parsed;
  DeserializationError err = deserializeJson(parsed, resultJson);
  if (!err) {
    if (parsed["spoken_summary"].is<const char*>()) {
      lastSpokenSummary = parsed["spoken_summary"].as<String>();
    }
    if (parsed["walkable_path"].is<const char*>()) {
      lastWalkablePath = parsed["walkable_path"].as<String>();
    }
    if (parsed["text_and_signs"].is<const char*>()) {
      lastTextSigns = parsed["text_and_signs"].as<String>();
    }
    if (parsed["urgent_hazards"].is<JsonArray>()) {
      String hList = "";
      for (JsonVariant v : parsed["urgent_hazards"].as<JsonArray>()) {
        if (hList.length() > 0) hList += ", ";
        hList += v.as<String>();
      }
      lastUrgentHazards = (hList.length() > 0) ? hList : "None";
    }
    if (parsed["detected_objects"].is<JsonArray>()) {
      String objStr;
      serializeJson(parsed["detected_objects"], objStr);
      lastObjectsJson = objStr;
    }
  }

  lastLatencyMs = millis() - startTime;
  lastAnalysisTime = millis();
  setStatusLed(false);

  // Print formatted report to Serial
  Serial.println("[AI REPORT]");
  Serial.printf("  Spoken Summary: %s\n", lastSpokenSummary.c_str());
  Serial.printf("  Urgent Hazards: %s\n", lastUrgentHazards.c_str());
  Serial.printf("  Walkable Path:  %s\n", lastWalkablePath.c_str());
  Serial.printf("  Signs / Text:   %s\n", lastTextSigns.c_str());
  Serial.printf("  Latency:        %d ms\n", lastLatencyMs);
  Serial.println("--------------------------------------------------\n");

  return resultJson;
}

// ============================================================================
// Chunked Base64 Streaming to WiFiClientSecure
// ============================================================================
void streamBase64(WiFiClientSecure &client, const uint8_t *data, size_t length) {
  char out[516];
  size_t outIdx = 0;
  size_t i = 0;

  while (i < length) {
    size_t rem = length - i;
    uint32_t a = data[i++];
    uint32_t b = (i < length) ? data[i++] : 0;
    uint32_t c = (i < length) ? data[i++] : 0;
    uint32_t triple = (a << 16) | (b << 8) | c;

    out[outIdx++] = B64_TABLE[(triple >> 18) & 0x3F];
    out[outIdx++] = B64_TABLE[(triple >> 12) & 0x3F];
    out[outIdx++] = (rem > 1) ? B64_TABLE[(triple >> 6) & 0x3F] : '=';
    out[outIdx++] = (rem > 2) ? B64_TABLE[triple & 0x3F] : '=';

    if (outIdx >= 512) {
      client.write((const uint8_t*)out, outIdx);
      outIdx = 0;
    }
  }

  if (outIdx > 0) {
    client.write((const uint8_t*)out, outIdx);
  }
}

// ============================================================================
// Direct Google Gemini Multimodal Vision API Client
// ============================================================================
String callGeminiVision(camera_fb_t *fb) {
  WiFiClientSecure client;
  client.setInsecure(); // Skip certificate validation on embedded MCU
  client.setTimeout(15000);

  const char* host = "generativelanguage.googleapis.com";
  const int port = 443;

  if (!client.connect(host, port)) {
    Serial.println("[GEMINI ERROR] Connection to Google Gemini API failed.");
    return "{\"status\":\"error\",\"message\":\"Failed to connect to Gemini API\"}";
  }

  // System instruction for blind assistance
  const char* promptText =
    "You are the AI vision assistant for IntelliCane, an intelligent white cane assisting a blind person. "
    "Analyze this camera frame captured from the cane. "
    "Identify urgent hazards (drop-offs, steps, curbs, low branches, approaching objects), "
    "the clear walkable path, and surrounding objects with clock-face orientation (e.g. 12 o'clock, 2 o'clock) "
    "and distance (close <1m, medium 1-3m, far >3m). "
    "Format strictly as JSON with keys: "
    "spoken_summary (1-3 clear sentences prioritizing hazards, walkable direction, and context), "
    "urgent_hazards (array of strings), "
    "walkable_path (string), "
    "detected_objects (array of {name, position, distance, hazard_level}), "
    "text_and_signs (string).";

  // Build JSON prefixes and calculate lengths
  String jsonPrefix = "{\"contents\":[{\"parts\":[{\"text\":\"";
  jsonPrefix += promptText;
  jsonPrefix += "\"},{\"inlineData\":{\"mimeType\":\"image/jpeg\",\"data\":\"";

  String jsonSuffix = "\"}}]}]}";

  size_t b64Length = 4 * ((fb->len + 2) / 3);
  size_t totalContentLength = jsonPrefix.length() + b64Length + jsonSuffix.length();

  String path = "/v1beta/models/";
  path += GEMINI_MODEL;
  path += ":generateContent?key=";
  path += GEMINI_API_KEY;

  // Send HTTP POST headers
  client.printf("POST %s HTTP/1.1\r\n", path.c_str());
  client.printf("Host: %s\r\n", host);
  client.println("Content-Type: application/json");
  client.printf("Content-Length: %u\r\n", totalContentLength);
  client.println("Connection: close");
  client.println();

  // Send JSON body with chunked Base64 image
  client.print(jsonPrefix);
  streamBase64(client, fb->buf, fb->len);
  client.print(jsonSuffix);

  // Read Response
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      break; // End of HTTP headers
    }
  }

  String responseBody = "";
  while (client.available()) {
    responseBody += client.readString();
  }
  client.stop();

  // Parse Gemini response
  JsonDocument geminiDoc;
  DeserializationError err = deserializeJson(geminiDoc, responseBody);
  if (err) {
    Serial.printf("[GEMINI PARSE ERROR] %s\n", err.c_str());
    return "{\"status\":\"error\",\"message\":\"Failed to parse Gemini response\"}";
  }

  // Extract generated text
  const char* rawText = geminiDoc["candidates"][0]["content"]["parts"][0]["text"];
  if (!rawText) {
    Serial.println("[GEMINI ERROR] No content parts returned in response.");
    return responseBody;
  }

  // Clean markdown json fences if present: ```json ... ```
  String cleaned = String(rawText);
  cleaned.trim();
  if (cleaned.startsWith("```json")) {
    cleaned = cleaned.substring(7);
  } else if (cleaned.startsWith("```")) {
    cleaned = cleaned.substring(3);
  }
  if (cleaned.endsWith("```")) {
    cleaned = cleaned.substring(0, cleaned.length() - 3);
  }
  cleaned.trim();

  return cleaned;
}

// ============================================================================
// High-Speed Real-Time MJPEG Stream Server (Port 81)
// ============================================================================
esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;
  char part_buf[64];

  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if (res != ESP_OK) return res;

  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "X-Framerate", "25");

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      res = ESP_FAIL;
      break;
    }

    res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    if (res == ESP_OK) {
      size_t hlen = snprintf(part_buf, 64, _STREAM_PART, fb->len);
      res = httpd_resp_send_chunk(req, part_buf, hlen);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
    }
    esp_camera_fb_return(fb);
    fb = NULL;

    if (res != ESP_OK) {
      break;
    }
  }
  return res;
}

void startCameraStreamServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 81;
  config.ctrl_port = 32769;

  httpd_uri_t stream_uri = {
    .uri       = "/stream",
    .method    = HTTP_GET,
    .handler   = stream_handler,
    .user_ctx  = NULL
  };

  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &stream_uri);
    Serial.println("[STREAM] Real-time 20+ FPS MJPEG stream active at port 81 (/stream)");
  } else {
    Serial.println("[STREAM ERROR] Failed to start stream server on port 81.");
  }
}

// ============================================================================
// Web Server Endpoints (Port 80)
// ============================================================================
void setupWebServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/capture", HTTP_GET, handleCapture);
  server.on("/capture.jpg", HTTP_GET, handleCapture);
  server.on("/describe", HTTP_GET, handleDescribe);
  server.on("/describe", HTTP_POST, handleDescribe);
  server.on("/flash", HTTP_GET, handleFlashToggle);
  server.on("/resolution", HTTP_GET, handleResolution);
  server.on("/status", HTTP_GET, handleStatus);

  server.begin();
  Serial.println("[HTTP] Web control server running on port 80.");
}

void handleRoot() {
  String html = R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>IntelliCane AI Vision</title>
  <style>
    :root {
      --bg: #0b1120;
      --card: #151f32;
      --border: #22324e;
      --text: #f8fafc;
      --text-muted: #94a3b8;
      --primary: #38bdf8;
      --primary-hover: #0284c7;
      --danger: #ef4444;
      --warning: #f59e0b;
      --success: #10b981;
    }
    body {
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
      background: var(--bg);
      color: var(--text);
      margin: 0;
      padding: 16px;
      display: flex;
      flex-direction: column;
      align-items: center;
    }
    .container {
      max-width: 680px;
      width: 100%;
    }
    header {
      text-align: center;
      margin-bottom: 18px;
    }
    h1 {
      margin: 0 0 6px 0;
      font-size: 1.65rem;
      color: var(--primary);
    }
    .badge {
      display: inline-block;
      padding: 4px 12px;
      font-size: 0.75rem;
      font-weight: 700;
      border-radius: 999px;
      background: #0369a1;
      color: white;
      letter-spacing: 0.05em;
    }
    .card {
      background: var(--card);
      border: 1px solid var(--border);
      border-radius: 14px;
      padding: 16px;
      margin-bottom: 16px;
      box-shadow: 0 4px 12px rgba(0,0,0,0.3);
    }
    .viewfinder-header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 10px;
      font-size: 0.85rem;
    }
    .live-indicator {
      display: inline-flex;
      align-items: center;
      gap: 6px;
      padding: 3px 10px;
      background: rgba(16, 185, 129, 0.2);
      border: 1px solid var(--success);
      color: #6ee7b7;
      border-radius: 999px;
      font-weight: 600;
      font-size: 0.75rem;
    }
    .live-dot {
      width: 8px;
      height: 8px;
      background: #10b981;
      border-radius: 50%;
      animation: pulse 1.5s infinite;
    }
    @keyframes pulse {
      0% { opacity: 1; transform: scale(1); }
      50% { opacity: 0.4; transform: scale(1.2); }
      100% { opacity: 1; transform: scale(1); }
    }
    .res-select {
      background: #0b1120;
      color: var(--text);
      border: 1px solid var(--border);
      border-radius: 6px;
      padding: 4px 8px;
      font-size: 0.8rem;
      cursor: pointer;
    }
    .camera-frame {
      width: 100%;
      height: 320px;
      object-fit: contain;
      background: #000;
      border-radius: 10px;
      border: 1px solid var(--border);
      display: block;
    }
    .btn-group {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 12px;
      margin-bottom: 16px;
    }
    button {
      padding: 14px 16px;
      font-size: 1rem;
      font-weight: 600;
      border: none;
      border-radius: 10px;
      cursor: pointer;
      display: flex;
      align-items: center;
      justify-content: center;
      gap: 8px;
      transition: all 0.2s;
    }
    .btn-primary {
      background: var(--primary);
      color: #0b1120;
      grid-column: span 2;
      font-size: 1.05rem;
    }
    .btn-primary:hover { background: var(--primary-hover); }
    .btn-secondary {
      background: #1e293b;
      color: var(--text);
      border: 1px solid var(--border);
    }
    .btn-secondary:hover { background: #334155; }
    .section-title {
      font-size: 0.8rem;
      text-transform: uppercase;
      letter-spacing: 0.06em;
      color: var(--text-muted);
      margin-bottom: 10px;
      display: flex;
      align-items: center;
      gap: 6px;
    }
    .speech-box {
      font-size: 1.15rem;
      line-height: 1.5;
      font-weight: 500;
      color: #f1f5f9;
      background: rgba(56, 189, 248, 0.1);
      border-left: 4px solid var(--primary);
      padding: 14px;
      border-radius: 0 10px 10px 0;
      margin-bottom: 12px;
    }
    .hazard-box {
      padding: 12px 14px;
      border-radius: 8px;
      font-weight: 600;
      margin-bottom: 12px;
    }
    .hazard-alert {
      background: rgba(239, 68, 68, 0.2);
      border: 1px solid var(--danger);
      color: #fca5a5;
    }
    .hazard-clear {
      background: rgba(16, 185, 129, 0.2);
      border: 1px solid var(--success);
      color: #6ee7b7;
    }
    .info-row {
      display: flex;
      justify-content: space-between;
      padding: 10px 0;
      border-bottom: 1px solid var(--border);
      font-size: 0.95rem;
    }
    .info-row:last-child { border-bottom: none; }
    .status-text {
      text-align: center;
      font-size: 0.82rem;
      color: var(--text-muted);
      margin-top: 10px;
    }
  </style>
</head>
<body>
  <div class="container">
    <header>
      <h1>IntelliCane AI Vision</h1>
      <span class="badge">ESP32-CAM Assistive System</span>
    </header>

    <div class="card">
      <div class="viewfinder-header">
        <div id="live-badge" class="live-indicator">
          <span class="live-dot"></span>
          <span id="fps-counter">REAL-TIME STREAM</span>
        </div>
        <div>
          <select id="resSelector" class="res-select" onchange="changeResolution(this.value)">
            <option value="qvga">QVGA (Fastest ~25 FPS)</option>
            <option value="vga" selected>VGA (Balanced ~15 FPS)</option>
            <option value="svga">SVGA (High Detail)</option>
          </select>
        </div>
      </div>

      <img id="cam-preview" class="camera-frame" alt="Camera Feed">
      <div id="status-line" class="status-text">Continuous real-time viewfinder active.</div>
    </div>

    <div class="btn-group">
      <button class="btn-primary" onclick="triggerDescribe()">
        <span>🎙️</span> Describe Surroundings
      </button>
      <button id="streamToggleBtn" class="btn-secondary" onclick="toggleStreamMode()">
        <span>⏸️</span> Pause Stream
      </button>
      <button class="btn-secondary" onclick="toggleFlash()">
        <span>💡</span> Toggle Flash
      </button>
    </div>

    <div class="card">
      <div class="section-title">🔊 Spoken Guidance</div>
      <div id="speech-text" class="speech-box">Awaiting command. Press Describe to scan surroundings.</div>
      <button class="btn-secondary" style="width:100%;" onclick="speakCurrent()">
        <span>🔈</span> Replay Audio Speech
      </button>
    </div>

    <div class="card">
      <div class="section-title">⚠️ Hazards & Path</div>
      <div id="hazard-banner" class="hazard-box hazard-clear">No immediate hazards detected</div>
      <div class="info-row">
        <span>Walkable Path</span>
        <strong id="walkable-path" style="color:var(--primary);">Unknown</strong>
      </div>
      <div class="info-row">
        <span>Text / Signs</span>
        <strong id="signs-text">None</strong>
      </div>
      <div class="info-row">
        <span>Inference Latency</span>
        <span id="latency-text">0 ms</span>
      </div>
    </div>
  </div>

  <script>
    let isStreaming = true;
    let streamFailed = false;
    let currentSpeech = "";
    let pollTimer = null;

    const imgElement = document.getElementById('cam-preview');
    const streamUrl = `http://${window.location.hostname}:81/stream`;

    function startStream() {
      isStreaming = true;
      document.getElementById('streamToggleBtn').innerHTML = '<span>⏸️</span> Pause Stream';
      document.getElementById('live-badge').style.display = 'inline-flex';

      // Load port 81 MJPEG video stream
      imgElement.src = streamUrl;

      // Fallback if port 81 is blocked on user network
      imgElement.onerror = function() {
        if (!streamFailed && isStreaming) {
          streamFailed = true;
          console.warn("Port 81 stream blocked or failed. Switching to fast frame polling.");
          startFastPolling();
        }
      };
    }

    function stopStream() {
      isStreaming = false;
      if (pollTimer) clearTimeout(pollTimer);
      imgElement.src = '/capture?t=' + Date.now();
      document.getElementById('streamToggleBtn').innerHTML = '<span>▶️</span> Resume Stream';
      document.getElementById('live-badge').style.display = 'none';
      document.getElementById('status-line').innerText = 'Stream paused. Showing static snapshot.';
    }

    function toggleStreamMode() {
      if (isStreaming) {
        stopStream();
      } else {
        startStream();
      }
    }

    // High-speed snapshot polling fallback
    function startFastPolling() {
      if (!isStreaming) return;
      const nextImg = new Image();
      const startT = performance.now();
      nextImg.onload = function() {
        imgElement.src = nextImg.src;
        const fps = Math.round(1000 / Math.max(1, performance.now() - startT));
        document.getElementById('fps-counter').innerText = fps + ' FPS';
        if (isStreaming) {
          pollTimer = setTimeout(startFastPolling, 40);
        }
      };
      nextImg.onerror = function() {
        if (isStreaming) pollTimer = setTimeout(startFastPolling, 200);
      };
      nextImg.src = '/capture?t=' + Date.now();
    }

    async function changeResolution(res) {
      document.getElementById('status-line').innerText = 'Switching resolution to ' + res + '...';
      try {
        await fetch('/resolution?size=' + res);
        document.getElementById('status-line').innerText = 'Resolution set to ' + res + '.';
        if (isStreaming && !streamFailed) {
          imgElement.src = streamUrl + '?t=' + Date.now();
        }
      } catch (err) {
        document.getElementById('status-line').innerText = 'Error changing resolution: ' + err.message;
      }
    }

    function speakText(text) {
      if ('speechSynthesis' in window && text) {
        window.speechSynthesis.cancel();
        const utterance = new SpeechSynthesisUtterance(text);
        utterance.rate = 1.05;
        window.speechSynthesis.speak(utterance);
      }
    }

    function speakCurrent() {
      speakText(currentSpeech || document.getElementById('speech-text').innerText);
    }

    async function triggerDescribe() {
      const statusLine = document.getElementById('status-line');
      const speechBox = document.getElementById('speech-text');
      statusLine.innerText = 'Analyzing surroundings... Please hold steady.';
      speechBox.innerText = 'Scanning scene for obstacles and walkable path...';

      try {
        const response = await fetch('/describe', { method: 'POST' });
        const data = await response.json();

        currentSpeech = data.spoken_summary || 'Surroundings analyzed.';
        speechBox.innerText = currentSpeech;
        speakText(currentSpeech);

        const hazardBanner = document.getElementById('hazard-banner');
        if (data.urgent_hazards && data.urgent_hazards.length > 0 && data.urgent_hazards[0] !== 'None') {
          hazardBanner.className = 'hazard-box hazard-alert';
          hazardBanner.innerText = '⚠️ Hazard: ' + data.urgent_hazards.join(', ');
        } else {
          hazardBanner.className = 'hazard-box hazard-clear';
          hazardBanner.innerText = '✅ No urgent hazards detected';
        }

        document.getElementById('walkable-path').innerText = data.walkable_path || 'Clear';
        document.getElementById('signs-text').innerText = data.text_and_signs || 'None';
        document.getElementById('latency-text').innerText = (data.latency_ms || 0) + ' ms';
        statusLine.innerText = 'Analysis complete (' + (data.latency_ms || 0) + ' ms)';
      } catch (err) {
        statusLine.innerText = 'Error: ' + err.message;
        speechBox.innerText = 'Failed to analyze scene. Please check connection.';
      }
    }

    async function toggleFlash() {
      const res = await fetch('/flash');
      const data = await res.json();
      document.getElementById('status-line').innerText = 'Flashlight ' + (data.flash ? 'ON' : 'OFF');
    }

    // Start live stream immediately on page load
    window.addEventListener('load', startStream);
  </script>
</body>
</html>
)rawliteral";

  server.send(200, "text/html", html);
}

void handleCapture() {
  camera_fb_t *fb = captureFrame();
  if (!fb) {
    server.send(500, "text/plain", "Camera capture failed");
    return;
  }

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Content-Disposition", "inline; filename=capture.jpg");
  server.setContentLength(fb->len);
  server.send(200, "image/jpeg", "");

  WiFiClient client = server.client();
  const size_t chunkSize = 2048;
  size_t remaining = fb->len;
  uint8_t *ptr = fb->buf;
  while (remaining > 0 && client.connected()) {
    size_t toWrite = (remaining < chunkSize) ? remaining : chunkSize;
    client.write(ptr, toWrite);
    ptr += toWrite;
    remaining -= toWrite;
  }
  esp_camera_fb_return(fb);
}

void handleResolution() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  if (!server.hasArg("size")) {
    server.send(400, "text/plain", "Missing size parameter: qvga, vga, or svga");
    return;
  }

  String sizeStr = server.arg("size");
  sizeStr.toLowerCase();

  sensor_t *s = esp_camera_sensor_get();
  if (!s) {
    server.send(500, "text/plain", "Sensor not ready");
    return;
  }

  if (sizeStr == "qvga") {
    s->set_framesize(s, FRAMESIZE_QVGA);   // 320x240 (Fastest 25+ FPS)
  } else if (sizeStr == "vga") {
    s->set_framesize(s, FRAMESIZE_VGA);    // 640x480 (Balanced ~15 FPS)
  } else if (sizeStr == "svga") {
    s->set_framesize(s, FRAMESIZE_SVGA);   // 800x600 (High detail)
  } else {
    server.send(400, "text/plain", "Invalid resolution: use qvga, vga, or svga");
    return;
  }

  Serial.printf("[CAM] Resolution switched to: %s\n", sizeStr.c_str());
  server.send(200, "application/json", "{\"status\":\"ok\",\"resolution\":\"" + sizeStr + "\"}");
}

void handleDescribe() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");

  String jsonResponse = performSceneDescription();
  server.send(200, "application/json", jsonResponse);
}

void handleFlashToggle() {
  setFlashLed(!flashState);
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", String("{\"flash\":") + (flashState ? "true" : "false") + "}");
}

void handleStatus() {
  JsonDocument doc;
  doc["status"] = "ok";
  doc["camera_initialized"] = cameraInitialized;
  doc["flash_led"] = flashState;
  doc["wifi_ssid"] = WiFi.SSID();
  doc["wifi_rssi"] = WiFi.RSSI();
  doc["ip_address"] = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
  doc["free_heap"] = ESP.getFreeHeap();
  doc["free_psram"] = psramFound() ? ESP.getFreePsram() : 0;
  doc["uptime_sec"] = millis() / 1000;

  String res;
  serializeJson(doc, res);
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", res);
}

// ============================================================================
// Serial Command Processor
// ============================================================================
void handleSerialCommands() {
  if (!Serial.available()) return;

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  cmd.toUpperCase();

  if (cmd == "DESCRIBE") {
    performSceneDescription();
  } else if (cmd == "CAPTURE") {
    camera_fb_t *fb = captureFrame();
    if (fb) {
      Serial.printf("[CAPTURE] Success: %u bytes (%dx%d)\n", fb->len, fb->width, fb->height);
      esp_camera_fb_return(fb);
    } else {
      Serial.println("[CAPTURE] Failed to capture photo.");
    }
  } else if (cmd == "FLASH ON") {
    setFlashLed(true);
    Serial.println("[FLASH] Flashlight turned ON.");
  } else if (cmd == "FLASH OFF") {
    setFlashLed(false);
    Serial.println("[FLASH] Flashlight turned OFF.");
  } else if (cmd == "STATUS") {
    Serial.println("\n--- IntelliCane Diagnostics ---");
    Serial.printf("Camera Ready:   %s\n", cameraInitialized ? "YES" : "NO");
    Serial.printf("Wi-Fi Status:   %s\n", (WiFi.status() == WL_CONNECTED) ? "CONNECTED" : "SOFTAP");
    Serial.printf("IP Address:     %s\n", (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString().c_str() : WiFi.softAPIP().toString().c_str());
    Serial.printf("Free Heap:      %u bytes\n", ESP.getFreeHeap());
    Serial.printf("Free PSRAM:     %u bytes\n", psramFound() ? ESP.getFreePsram() : 0);
    Serial.printf("Flashlight:     %s\n", flashState ? "ON" : "OFF");
    Serial.println("--------------------------------\n");
  } else if (cmd == "HELP") {
    Serial.println("\nCommands: DESCRIBE, CAPTURE, FLASH ON, FLASH OFF, STATUS, HELP\n");
  }
}

// ============================================================================
// Hardware Trigger Pin Monitor
// ============================================================================
void checkHardwareTrigger() {
  if (digitalRead(TRIGGER_PIN) == LOW) {
    if (millis() - lastTriggerTime > DEBOUNCE_DELAY_MS) {
      lastTriggerTime = millis();
      Serial.println("[HARDWARE TRIGGER] Button pressed / signal detected on GPIO 13!");
      performSceneDescription();
    }
  }
}
