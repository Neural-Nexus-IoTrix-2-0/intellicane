# Intelligent Cane (`intelligent-cane`)

> **Neural-Nexus | IoTrix 2.0 (Track A: Embedded IoT System Development)**  
> Low-cost, offline-safe, multimodal smart mobility cane for visually impaired individuals.

---

## 1. Project Overview

The Intelligent Cane is an assistive IoT device designed to provide proactive navigation assistance, ground drop-off protection, emergency fall alerts, and ambient visual context for visually impaired users.

### Three Integrated Subsystems

```
                               ┌────────────────────────────────────────────────────────┐
                               │                    INTELLIGENT CANE                    │
                               └──────────────────────────┬─────────────────────────────┘
                                                          │
         ┌────────────────────────────────────────────────┼────────────────────────────────────────┐
         │                                                │                                        │
         ▼                                                ▼                                        ▼
┌─────────────────────────────────┐      ┌─────────────────────────────────┐      ┌─────────────────────────────────┐
│           Phase 1               │      │            Phase 2              │      │            Phase 3              │
│       Core Sensing              │      │    BLE Geolocation & Telemetry  │      │        AI Voice Layer           │
├─────────────────────────────────┤      ├─────────────────────────────────┤      ├─────────────────────────────────┤
│ • ESP32-C3 SuperMini (RISC-V)   │      │ • Smartphone BLE A-GPS Bridge   │      │ • Dedicated ESP32-CAM Board     │
│ • HC-SR04 Ultrasonic Sensor     │      │ • Low-power BLE 5.0 Broadcast   │      │ • Cloud/Phone VLM (Gemini/GPT4o)│
│ • MPU6050 6-Axis Tilt & Fall    │      │ • Zero Extra Hardware Cost      │      │ • Cloud Neural Text-to-Speech   │
│ • Proportional Haptic PWM       │      │ • Family Web Dashboard          │      │ • Spoken Ambient Scene Q&A      │
│ • Piezo Alarm & Fall Siren      │      │ • Emergency Geolocation Push    │      │ • Off-device heavy computation  │
│ • 100% Offline & Deterministic  │      │ • Works indoors & outdoors      │      │                                 │
└─────────────────────────────────┘      └─────────────────────────────────┘      └─────────────────────────────────┘
```

1. **Obstacle & Hazard Detection (Phase 1 — Active Build)**:
   - **Safety-Critical & Fully Offline**: Operates without any internet, Bluetooth, or cloud dependencies.
   - **Forward Proximity**: HC-SR04 ultrasonic sensor measures distance up to 4 meters, modulating ERM vibration motor PWM intensity (closer = stronger vibration).
   - **Audible Hazard Warning**: Piezo buzzer activates when forward obstacles are dangerously close ($< 30\text{ cm}$).
   - **Fall & Tilt Sensing**: MPU6050 IMU detects sudden drops, impact spikes, and extended immobility on the floor.
   - **Ultra-Compact Form Factor**: Driven by the stamp-sized ESP32-C3 SuperMini with native USB-C.

2. **Smartphone BLE Geolocation & Telemetry (Phase 2)**:
   - **No Standalone GPS Hardware Needed**: Leverages the user's companion smartphone via BLE 5.0 to fetch precise Assisted GPS (A-GPS), cell tower, and Wi-Fi geolocation.
   - Drastically cuts power consumption, avoids indoor satellite blind spots, and saves weight and budget.
   - Emergency fall and SOS alerts automatically trigger the smartphone app to upload coordinates and status to the family caregiver dashboard.

3. **AI Voice Vision Layer (Phase 3)**:
   - Dedicated ESP32-CAM module captures environment snapshots on demand.
   - Companion cloud/smartphone service runs multimodal Vision-Language Model (VLM) + Neural TTS to describe immediate surroundings, obstacles, signs, or currency into the user's earpiece.

---

## 2. Repository Structure

```
intelligent-cane/
├── firmware/                       # Embedded C++/Arduino code
│   ├── core-sensing/               # Phase 1 Active Build: ESP32-C3 SuperMini + HC-SR04 + MPU6050
│   │   ├── core-sensing.ino        # Production sketch with 200Hz PWM, buzzer, and self-test
│   │   ├── platformio.ini          # PlatformIO configuration (board: esp32-c3-devkitm-1)
│   │   └── README.md               # Pinout and flashing instructions
│   ├── archived-dual-tof/          # Archived reference build (dual VL53L1X/VL53L0X ToF)
│   ├── gps-tracking/               # Phase 2: GPS NMEA parsing & telemetry reporting
│   └── cam-module/                 # Phase 3: Dedicated ESP32-CAM snapshot streamer
├── ai-voice-service/                # Phase 3: Cloud/companion VLM + TTS pipeline
├── dashboard/                       # Family-facing web dashboard (HTML/JS)
├── hardware/                        # Hardware schematics, wiring, and BOM
│   ├── circuit_diagram.png         # Official ESP32-C3 SuperMini schematic diagram
│   ├── wiring.md                   # Pinouts, direct bench wiring, production driver circuits
│   ├── BOM.md                      # Component list and LKR budget tracking
│   └── README.md
├── docs/                            # Architectural specifications & decision logs
│   ├── architecture.md             # System architecture & block diagrams
│   ├── decisions.md                # Architecture Decision Records (ADRs)
│   ├── PROGRESS_REPORT.md          # 1-page executive technical progress report
│   └── README.md
├── simulation/                      # Wokwi simulation workspace
├── .gitignore
└── README.md
```

---

## 3. Hardware Pin Mapping (Core Sensing — ESP32-C3 SuperMini)

| Component | Function | ESP32-C3 Pin | Logic Level | Notes |
|:---|:---|:---:|:---:|:---|
| **HC-SR04 Ultrasonic** | Trigger Pulse (`TRIG`) | **GPIO 0** | 3.3V Output | 10 µs trigger pulse |
| **HC-SR04 Ultrasonic** | Echo Pulse (`ECHO`) | **GPIO 1** | 3.3V / 5V | Direct connection on bench build (divider optional for prod) |
| **MPU6050 (6-Axis IMU)**| I2C Data (`SDA`) | **GPIO 4** | 3.3V | Hardware I2C bus (GY-521 pin 4; VCC to 5V) |
| **MPU6050 (6-Axis IMU)**| I2C Clock (`SCL`) | **GPIO 5** | 3.3V | Hardware I2C bus (GY-521 pin 3) |
| **3-Pin Vibration Motor**| `IN / Signal` | **GPIO 6** | 3.3V PWM | Integrated driver module (VCC to 5V, LEDC 200 Hz) |
| **Piezo Buzzer** | Audio Alarm (`+`) | **GPIO 7** | 3.3V | Universal driver for active & passive buzzers |
| **Push Button (Optional)** | Alarm Reset / Emergency | **GPIO 3** | 3.3V Input | Optional / unpopulated on bench build (alarm auto-clears when upright) |
| **Status LED** | Visual Indicator | **GPIO 8** | 3.3V | Onboard SuperMini blue LED (**Active LOW**) |

*Full schematic, driver circuits, voltage divider calculations, and power distribution are documented in [`hardware/circuit_diagram.png`](./hardware/circuit_diagram.png) and [`hardware/wiring.md`](./hardware/wiring.md).*

---

## 4. Getting Started for Contributors

### Prerequisites
- [Arduino IDE](https://www.arduino.cc/en/software) with the ESP32 board package installed, OR
- [Arduino CLI](https://arduino.github.io/arduino-cli/), OR
- [PlatformIO Core](https://platformio.org/install/cli) / PlatformIO VS Code extension.

### Building & Flashing Phase 1 Firmware

#### Method 1: Arduino CLI (Fastest & Verified)
```bash
# Compile with USB CDC On Boot enabled for ESP32-C3
arduino-cli compile --fqbn esp32:esp32:esp32c3:CDCOnBoot=cdc,FlashMode=dio firmware/core-sensing

# Flash directly to connected board
arduino-cli upload -p /dev/cu.usbmodem* --fqbn esp32:esp32:esp32c3:CDCOnBoot=cdc,FlashMode=dio firmware/core-sensing
```

#### Method 2: PlatformIO
```bash
cd firmware/core-sensing
pio run --target upload
```

#### Method 3: Arduino IDE
1. Open `firmware/core-sensing/core-sensing.ino`.
2. Select Board: **ESP32C3 Dev Module**.
3. Select **Tools > USB CDC On Boot > Enabled**.
4. Select **Tools > Flash Mode > DIO**.
5. Select the USB modem serial port and click **Upload**.

### Live Telemetry Monitoring
Open the serial console at 115200 baud:
```bash
pio device monitor -b 115200
# or
arduino-cli monitor -p /dev/cu.usbmodem* -c baudrate=115200
```

Sample telemetry stream:
```text
=======================================================
  INTELLIGENT CANE — PHASE 1 CORE SENSING BOOTING
  Neural-Nexus IoTrix 2.0 (Track A Embedded IoT)
=======================================================
[System] Initializing I2C bus (SDA=GPIO 4, SCL=GPIO 5)...
[Motion] MPU6050 initialized successfully!
[HC-SR04] Ultrasonic sensor ready (Trig: GPIO 0, Echo: GPIO 1)
[Haptic] LEDC PWM initialized on GPIO 6 (200 Hz, 8-bit)
[Buzzer] Configured on GPIO 7
[SelfTest] Running motor & buzzer self-test...
[SelfTest] Self-test complete! System ARMED.
=======================================================

[CANE-C3] Dist: 142.3 cm | Tilt:  2.1° | Vib:   0% (PWM:   0) | Buzzer: MUTED   | Alert: CLEAR
[CANE-C3] Dist:  68.5 cm | Tilt:  1.8° | Vib:  57% (PWM: 146) | Buzzer: MUTED   | Alert: CLEAR
[CANE-C3] Dist:  18.2 cm | Tilt:  2.4° | Vib: 100% (PWM: 255) | Buzzer: BEEPING | Alert: CRITICAL HAZARD!
```

---

## 5. Development Roadmap

- [x] **Milestone 1**: Scaffolding, architecture design, and ADR documentation.
- [x] **Milestone 2**: Phase 1 core sensing firmware (HC-SR04 ultrasonic ranging, MPU6050 IMU, 200 Hz LEDC haptic PWM, universal buzzer driver, startup self-test).
- [x] **Milestone 3**: Physical bench testing & verification on live ESP32-C3 SuperMini hardware.
- [ ] **Milestone 4 (Phase 2)**: Smartphone BLE geolocation bridging and caregiver web dashboard integration.
- [ ] **Milestone 5 (Phase 3)**: ESP32-CAM board firmware and off-device AI voice service pipeline.

---

## 6. Budget & BOM Summary

Tracked target prototype budget: **LKR 20,000 – 35,000**
- **Actual Phase 1 Bench Prototype Cost**: **Rs. 2,800 (LKR)** (achieved dramatic cost efficiency, far below the LKR 10,000 Phase 1 cap)
- **Total Multi-phase Estimated Cost**: **~LKR 11,500 – 14,000** (achieves full system well below target ceiling)
- See [`hardware/BOM.md`](./hardware/BOM.md) for individual component pricing and local supplier references.
