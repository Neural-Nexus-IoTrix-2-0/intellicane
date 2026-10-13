#pragma once

// ============================================================================
// IntelliCane ESP32-CAM Configuration
// ============================================================================

// 1. Wi-Fi Settings
// Set your Wi-Fi SSID and Password here.
// If the ESP32-CAM cannot connect to your Wi-Fi within 10 seconds,
// it automatically creates its own SoftAP hotspot named "IntelliCane-Cam".
#define WIFI_SSID           "YOUR_WIFI_SSID"
#define WIFI_PASS           "YOUR_WIFI_PASSWORD"

// Fallback SoftAP configuration (when primary Wi-Fi is unavailable)
#define SOFTAP_SSID         "IntelliCane-Cam"
#define SOFTAP_PASS         "intellicane123"

// 2. Google Gemini Vision API Settings (Direct On-Device Cloud Inference)
// Obtain a free Gemini API key from https://aistudio.google.com/
// If left empty or as placeholder, the system can route through the local
// AI Voice Service or operate in benchmark test mode.
#define GEMINI_API_KEY      "YOUR_GEMINI_API_KEY"
#define GEMINI_MODEL        "gemini-1.5-flash"

// 3. Optional Local / Cloud AI Voice Service Backend URL
// If you are running the companion FastAPI service (ai-voice-service/):
// Example: "http://192.168.1.100:8000/analyze/image"
#define AI_SERVICE_URL      ""

// 4. Hardware Trigger & GPIO Controls
// GPIO 13 is configured as an active-LOW input with internal pull-up.
// Connect a momentary push button between GPIO 13 and GND,
// or connect a pulse output from the ESP32-C3 main cane microcontroller.
#define TRIGGER_PIN         13

// Flash LED pin (GPIO 4 on AI-Thinker). Used for night/indoor obstacle assist.
#define ENABLE_FLASH_ON_CAPTURE false

// Web Server Port
#define WEB_SERVER_PORT     80

// Serial Communication Baud Rate
#define SERIAL_BAUD_RATE    115200
