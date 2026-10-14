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
| Forward obstacle >=60 cm | Obstacle vibration (Motor 1) and beeps off |
| Forward obstacle <60 cm | Increasing Motor 1 PWM and beep urgency as distance decreases |
| Forward obstacle <=10 cm | Full commanded Motor 1 duty (100% vibration) |
| Ground TOF baseline (80–150 cm) | Ground hazard feedback off (Motor 2 idle) |
| Ground TOF drop-off (>150 cm / out-of-range) | Motor 2 pulsing warning pattern (detects stairs down, holes, curbs down) |
| Ground TOF curb step-up (<80 cm) | Motor 2 steady warning pattern (detects curbs up, steps, obstacles on ground) |
| Pushbutton press or BLE `"describe"` | ESP32-C3 pulses GPIO 21 to trigger ESP32-CAM instant Gemini AI scene description |
| Tilt >=65 degrees OR acceleration >=2.5 g | Fall state: motors silenced; alert buzzer tone; dispatches `FALL_STATE:1` over BLE |
| Tilt <30 degrees or manual button press | Fall cleared; dispatches `FALL_STATE:0` over BLE and restores normal navigation |

Local motor feedback is silenced during fall events to prevent user distress while high-priority BLE telemetry immediately alerts the companion mobile app and caretaker. Dual-zone motor duty is smoothly modulated via 200 Hz LEDC hardware PWM.

## Hardware and interfaces

| Component | Connection |
|---|---|
| **Forward HC-SR04 Ultrasonic (US1)** | TRIG GPIO 0; ECHO GPIO 1 (Chest/Forward obstacle ranging) |
| **Lower HC-SR04 Ultrasonic (US2)** | TRIG GPIO 2; ECHO GPIO 10 (Knee/Lower obstacle ranging) |
| **MPU6050 6-Axis IMU** | SDA GPIO 4, SCL GPIO 5; hardware I2C at 100 kHz (0x68) |
| **TOF10120 Laser Sensor** | SDA GPIO 4, SCL GPIO 5; shared hardware I2C (0x52, downward angled) |
| **Motor 1 (Forward Obstacle)** | IN GPIO 6; 200 Hz LEDC PWM Ch 0 |
| **Motor 2 (Ground Drop / Curb)** | IN GPIO 9; 200 Hz LEDC PWM Ch 1 |
| **Piezo Buzzer** | GPIO 7; audio proximity beeps and fall alarm |
| **Tactile Pushbutton** | GPIO 3 to GND, INPUT_PULLUP (Camera trigger / manual fall reset) |
| **Camera Trigger Line** | GPIO 21 output to ESP32-CAM GPIO 13 |
| **ESP32-CAM Module** | OV2640 2MP Camera, Wi-Fi streaming (Port 81), Gemini AI Vision (Port 80) |
| **Onboard Status LED** | GPIO 8, active LOW |

Common ground (GND) is shared across all modules. Refer to [wiring.md](hardware/wiring.md) for detailed electrical documentation.

## Firmware and build

### 1. Core Sensing & Reflex Firmware (`firmware/core-sensing/`)
Interleaves dual ultrasonic pings every 40 ms, reads TOF10120 laser ranging and MPU6050 IMU, manages dual-zone haptics, and dispatches BLE NUS telemetry every 250 ms:

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

- **Dual-Range Ultrasonic Sensing**: Interleaved ping scheduling on US1 (Forward) and US2 (Lower) with 0% acoustic cross-talk.
- **Time-of-Flight Ground Sensing**: TOF10120 laser ranging angled downward to instantly catch drop-offs, open pits, curbs, and stairs.
- **Dual-Zone Spatial Haptics**: Motor 1 for forward obstacles; Motor 2 for ground hazards (curbs and drop-offs).
- **Gemini 3.1 Flash Lite AI Vision**: On-demand scene descriptions triggered via button, BLE command, or web dashboard. Chunked transfer HTTP decoder for sub-second cloud responses.
- **High-Speed Video Stream**: 15–25 FPS MJPEG camera server on port 81 with dynamic resolution switching.
- **Autonomous Fall Detection**: MPU6050 impact and tilt vector tracking with instant BLE dispatch (`FALL_STATE:1`/`FALL_STATE:0`).
- **Android Companion App (`application/`)**: User Dashboard, Caretaker Mode, live GPS map, automated SMS alerts, and Firebase cloud notifications.
