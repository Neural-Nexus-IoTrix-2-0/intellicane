# ESP32-CAM AI Vision Module

This module implements real-time visual assistance for the **IntelliCane** smart white cane. When commanded (via Serial, Web Dashboard, REST API, or a hardware trigger button), the module captures an image from the OV2640 camera, runs object and hazard analysis tailored for visually impaired navigation, and generates a structured description with actionable orientation and audio readout.

---

## Features

- **Hardware Platform**: AI-Thinker ESP32-CAM with OV2640 2MP camera and ESP32-CAM-MB micro-USB programmer base.
- **Multimodal AI Scene Understanding**:
  - **Urgent Hazards First**: Detects stairs/steps down, drop-offs, curbs, low-hanging obstacles at head height, moving vehicles/pedestrians, and tripping hazards.
  - **Walkable Path Guidance**: Calculates safe walking trajectories using clock-face directions (e.g., *"Clear path forward at 12 o'clock"*).
  - **Spatial Object Detection**: Identifies objects with clock-face positions (12 o'clock, 2 o'clock, 10 o'clock), estimated distances, and hazard levels.
  - **Signage & Text Recognition**: Reads room numbers, exit signs, and street indicators.
  - **Spoken Guidance**: Generates 1–3 concise sentences optimized for text-to-speech audio feedback.
- **Multiple Command Triggers**:
  1. **USB Serial**: Type `DESCRIBE` or `CAPTURE` at 115200 baud.
  2. **Web Dashboard**: Access `http://<device-ip>/` in any browser for an interactive touch interface with auto-speech.
  3. **Hardware Trigger**: Push button on **GPIO 13** (active LOW with internal pull-up) or UART/trigger pulse from the primary ESP32-C3 cane controller.
  4. **REST API**: Call `GET /describe` or `POST /describe` for JSON, or `GET /capture` for JPEG snapshots.
- **Dual Execution Modes**:
  - **Direct On-Device Cloud Inference**: Direct HTTPS POST to Google Gemini Vision API (`gemini-1.5-flash` or `gemini-2.0-flash`) via `WiFiClientSecure` using zero-memory-overhead chunked Base64 streaming.
  - **Companion AI Voice Service**: Local or cloud backend integration (`ai-voice-service/`) for neural speech synthesis.
  - **Out-of-the-Box Bench Simulation**: Works immediately for offline testing even before configuring an API key.

---

## Hardware & Pinout (AI-Thinker ESP32-CAM)

| Function | Pin / GPIO | Notes |
|---|---|---|
| Camera Interface | D0–D7, XCLK, PCLK, VSYNC, HREF, SIOD, SIOC | Pre-configured in `camera_pins.h` |
| Flashlight LED | GPIO 4 | High-brightness LED (active HIGH) |
| Onboard Status LED | GPIO 33 | Red indicator LED (active LOW) |
| Hardware Trigger Pin | GPIO 13 | Active LOW, configured with internal pull-up |
| USB Programming | TX0 (GPIO 1), RX0 (GPIO 3) | Handled automatically by the ESP32-CAM-MB dev board |
| Power | 5V / GND | Powered via micro-USB on dev board (draws ~180–310 mA peak) |

---

## Quick Start Guide

### 1. Configuration
Open [`config.h`](config.h) and set:
```cpp
#define WIFI_SSID       "YourWiFiName"
#define WIFI_PASS       "YourWiFiPassword"
#define GEMINI_API_KEY  "YourGoogleGeminiApiKey" // Get free key at https://aistudio.google.com/
```
*Note: If Wi-Fi fails to connect within 10 seconds, the ESP32-CAM automatically creates a fallback hotspot named **`IntelliCane-Cam`** (password: `intellicane123`).*

### 2. Compile and Upload

#### Using `arduino-cli`:
```sh
# Compile
arduino-cli compile --fqbn esp32:esp32:esp32cam firmware/cam-module

# Upload (replace /dev/cu.usbserial-xxx with your port)
arduino-cli upload -p /dev/cu.usbserial-110 --fqbn esp32:esp32:esp32cam firmware/cam-module
```

#### Using PlatformIO:
```sh
cd firmware/cam-module
pio run -t upload
```

### 3. Verification & Testing

#### A. Over Serial Monitor (115200 Baud)
Open your serial monitor:
```sh
arduino-cli monitor -p /dev/cu.usbserial-110 -c baudrate=115200
```
Type `DESCRIBE` and press Enter. The camera takes a snapshot and prints:
```
--------------------------------------------------
[AI REPORT]
  Spoken Summary: Path straight ahead at 12 o'clock is clear for 2 meters. A low obstacle is at 2 o'clock on your right. Step forward with caution.
  Urgent Hazards: Low obstacle at 2 o'clock (1.2m away)
  Walkable Path:  Clear corridor directly ahead at 12 o'clock.
  Signs / Text:   Office doorway visible straight ahead
  Latency:        820 ms
--------------------------------------------------
```

#### B. Via Web Browser Dashboard
1. Connect your phone or laptop to the same Wi-Fi network (or connect directly to the `IntelliCane-Cam` hotspot).
2. Open `http://<esp32-ip>/` (or `http://192.168.4.1/` if connected to hotspot).
3. Tap **"Describe Surroundings"** to capture, analyze, and hear the audio announcement spoken aloud through your device!

#### C. Via Hardware Button
Connect a momentary push button between **GPIO 13** and **GND**. Pressing the button immediately triggers scene analysis.
