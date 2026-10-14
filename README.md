# IntelliCane

**Neural-Nexus · IoTrix 2.0 · Track A: Embedded IoT System Development**

IntelliCane is an affordable, multimodal smart white cane developed by undergraduate engineering students for IoTrix 2.0. It pairs an instant, 100% offline ESP32-C3 hardware reflex on the cane with a companion Android app for blind users and their caretakers. An ultrasonic sensor drives smooth, progressive handle vibrations as obstacles get closer, an onboard IMU detects sudden falls, and BLE syncs live location and emergency alerts to caregivers via Firebase.

**Team Members:** Dulnith Liyanage, Thenul Sahansa, Chamith Chethana, Suneth Vidurasa  
**Status on 5 October 2026:** Bench prototype assembled and verified, firmware compiled, unit tests passing, and Android companion app operational. Bench electronics cost: **LKR 2,800**.

## Semi-final package

The organizer announcement specifies **10 minutes total: 4 minutes demonstration, 2 minutes progress check, 4 minutes Q&A**. All four members must attend with cameras on.

- [**One-Page Technical Proposal**](ONE_PAGE_TECHNICAL_PROPOSAL.md) *(PDF: [`ONE_PAGE_TECHNICAL_PROPOSAL.pdf`](ONE_PAGE_TECHNICAL_PROPOSAL.pdf))*
- [**Project Proposal Submission PDF**](docs/Neural-Nexus.pdf) *(LaTeX source: `docs/proposals/IntelliCane_Project_Proposal.tex`)*
- [10-minute demonstration and Q&A guide](docs/IoTrix_SemiFinal_Defense_and_Demo_Guide.md)
- [System architecture](docs/architecture.md)
- [Software validation evidence](docs/test-logs/2026-10-05-software-validation.md)
- [Android Companion App](application/) (`application/` — User Dashboard, Caretaker Mode, live GPS map, automated SMS alerts, and instant emergency push notifications with sound & vibration on fall detection)

## Current behavior

| Input | Firmware behavior |
|---|---|
| Obstacle distance >=60 cm (or >1.8m out-of-range) | Obstacle vibration (Motor 1 & 2) and proximity beeps off |
| Obstacle distance <60 cm | Proportional dual-motor (Motor 1 & 2) PWM duty and increasing beep frequency as distance decreases |
| Obstacle distance <=10 cm | Full dual-motor commanded duty (100%) and continuous warning tone |
| Pushbutton press or BLE `"describe"` | ESP32-C3 pulses GPIO 21 to trigger ESP32-CAM instant Gemini AI scene description |
| Tilt >=65 degrees OR acceleration >=2.5 g | Fall state: motors silenced; alert buzzer tone; dispatches `FALL_STATE:1` over BLE |
| Tilt <30 degrees or manual button press | Fall cleared; dispatches `FALL_STATE:0` over BLE and restores normal navigation |

Local motor feedback is silenced during fall events to prevent user distress while high-priority BLE telemetry immediately alerts the companion mobile app and caretaker. Dual-motor duty is smoothly modulated via 20 kHz ultrasonic LEDC hardware PWM (completely eliminating acoustic motor whine).

## Hardware and interfaces

| Component | Connection |
|---|---|
| **TOF10120 Laser Distance Sensor** | SDA GPIO 4 (Blue), SCL GPIO 5 (Green); hardware I2C at 100 kHz (0x52, optical ToF) |
| **Ultrasonic Sensor (1× HC-SR04)** | TRIG GPIO 0; ECHO GPIO 1 (Acoustic obstacle ranging with internal pull-down) |
| **MPU6050 6-Axis IMU** | SDA GPIO 4, SCL GPIO 5; shared hardware I2C (0x68) |
| **Motor 1 (Primary Proximity)** | IN GPIO 6; 20 kHz LEDC PWM Ch 0 (silent ultrasonic PWM) |
| **Motor 2 (Dual Proximity)** | IN GPIO 2; 20 kHz LEDC PWM Ch 1 (silent ultrasonic PWM) |
| **Piezo Buzzer** | GPIO 7; LEDC tone generation (2000–2800 Hz) for audio proximity and fall alarm |
| **Camera Trigger Line** | GPIO 21 output to ESP32-CAM GPIO 13 |
| **ESP32-CAM Module** | OV2640 2MP Camera, powered from C3 5V/GND, Gemini AI Vision (Port 80) & Video Stream (Port 81) |
| *(Pushbutton)* | Not connected (Trigger via Serial `c` or BLE `"describe"`; auto-reset on upright) |
| *(Status LED)* | Not connected (bench build without external LED) |

Common ground (GND) is shared across all modules. Refer to [wiring.md](hardware/wiring.md) for detailed electrical documentation.

## Firmware and build

### 1. Core Sensing & Reflex Firmware (`firmware/core-sensing/`)
Fuses TOF10120 laser ranging with interleaved HC-SR04 ultrasonic echo sensing and reads MPU6050 IMU every 35 ms, driving dual haptics and dispatching BLE NUS telemetry every 250 ms:

```sh
arduino-cli compile --fqbn esp32:esp32:esp32c3:CDCOnBoot=cdc,FlashMode=dio firmware/core-sensing
```

Host unit tests covering proximity math and ground hazard feedback logic:
```sh
clang++ -std=c++17 -Wall -Wextra -I./firmware/core-sensing tests/proximity_feedback_test.cpp -o test_runner && ./test_runner && rm test_runner
```

### 2. AI Vision Subsystem Firmware (`firmware/cam-module/`)
Serves a high-speed MJPEG video stream (Port 81) and captures on-demand snapshots analyzed by Google Gemini 3.1 Flash Lite API with Web Speech API audio narration (Port 80):

```sh
arduino-cli compile --fqbn esp32:esp32:esp32cam firmware/cam-module
```

## Implemented and verified

- **Multi-Modal Complementary Sensor Fusion**: Fuses optical TOF laser ranging (millimeter-precision for narrow obstacles) and acoustic ultrasonic ranging (wide-angle reflection for glass, mesh, and wide walls). The closest hazard always takes immediate priority.
- **Dual-Motor Tactile Feedback**: Motor 1 provides smooth progressive warning (0–60 cm); Motor 2 delivers rapid pulsing alert when obstacles enter the critical hazard zone (< 25 cm).
- **Gemini 3.1 Flash Lite AI Vision**: On-demand scene descriptions triggered via button, BLE command, or web dashboard. Chunked transfer HTTP decoder for sub-second cloud responses.
- **High-Speed Video Stream**: 15–25 FPS MJPEG camera server on port 81 with dynamic resolution switching.
- **Autonomous Fall Detection**: MPU6050 impact and tilt vector tracking with instant BLE dispatch (`FALL_STATE:1`/`FALL_STATE:0`).
- **Android Companion App (`application/`)**: User Dashboard, Caretaker Mode, live GPS map, automated SMS alerts, and Firebase cloud notifications.
