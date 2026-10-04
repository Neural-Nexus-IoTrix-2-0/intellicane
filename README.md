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
│       Core Sensing              │      │         GPS Tracking            │      │        AI Voice Layer           │
├─────────────────────────────────┤      ├─────────────────────────────────┤      ├─────────────────────────────────┤
│ • ESP32-C3 SuperMini (RISC-V)   │      │ • u-blox NEO-6M / NEO-8M GPS    │      │ • Dedicated ESP32-CAM Board     │
│ • HC-SR04 Ultrasonic Sensor     │      │ • Wi-Fi Telemetry Uplink        │      │ • Cloud/Phone VLM (Gemini/GPT4o)│
│ • MPU6050 6-Axis Tilt & Fall    │      │ • Optional LoRa Off-Grid Uplink │      │ • Cloud Neural Text-to-Speech   │
│ • Proportional Haptic PWM       │      │ • Family Web Dashboard          │      │ • Spoken Ambient Scene Q&A      │
│ • Piezo Alarm & Fall Siren      │      │ • Emergency Geolocation Push    │      │ • Off-device heavy computation  │
│ • 100% Offline & Deterministic  │      │                                 │      │                                 │
└─────────────────────────────────┘      └─────────────────────────────────┘      └─────────────────────────────────┘
```

1. **Obstacle & Hazard Detection (Phase 1 — Active Build)**:
   - **Safety-Critical & Fully Offline**: Operates without any internet, Bluetooth, or cloud dependencies.
   - **Forward Proximity**: HC-SR04 ultrasonic sensor measures distance up to 4 meters, modulating ERM vibration motor PWM intensity (closer = stronger vibration).
   - **Audible Hazard Warning**: Piezo buzzer activates when forward obstacles are dangerously close ($< 30\text{ cm}$).
   - **Fall & Tilt Sensing**: MPU6050 IMU detects sudden drops, impact spikes, and extended immobility on the floor.
   - **Ultra-Compact Form Factor**: Driven by the stamp-sized ESP32-C3 SuperMini with native USB-C.

2. **Location Sharing & Telemetry (Phase 2)**:
   - Live GPS tracking (NEO-6M / NEO-8M) reporting coordinates over Wi-Fi/cellular to a caregiver web dashboard.
   - Fall and SOS alerts with timestamp and coordinates.
   - Optional LoRa fallback for off-grid scenarios.

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
│   ├── wiring.md                   # Pinouts, direct bench wiring, production driver circuits
│   ├── BOM.md                      # Component list and LKR budget tracking
│   └── README.md
├── docs/                            # Architectural specifications & decision logs
│   ├── architecture.md             # System architecture & block diagrams
│   ├── decisions.md                # Architecture Decision Records (ADRs)
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
| **MPU6050 (6-Axis IMU)**| I2C Data (`SDA`) | **GPIO 4** | 3.3V | Hardware I2C bus (Address: `0x68`) |
| **MPU6050 (6-Axis IMU)**| I2C Clock (`SCL`) | **GPIO 5** | 3.3V | Hardware I2C bus |
| **Haptic Vibration Motor**| `(+) / Signal` | **GPIO 6** | 3.3V PWM | Direct GPIO drive on bench build (LEDC 200 Hz PWM) |
| **Piezo Buzzer** | Audio Alarm (`+`) | **GPIO 7** | 3.3V | Universal driver for active & passive buzzers |
| **Push Button (SOS)** | Alarm Reset / Emergency | **GPIO 3** | 3.3V Input | Internal `INPUT_PULLUP` enabled (Active LOW) |
| **Status LED** | Visual Indicator | **GPIO 8** | 3.3V | Onboard SuperMini blue LED (**Active LOW**) |

*Full driver schematics, production circuits (transistor driver & voltage divider), and power distribution are documented in [`hardware/wiring.md`](./hardware/wiring.md).*

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
- [ ] **Milestone 4 (Phase 2)**: GPS tracking subsystem and caregiver web dashboard integration.
- [ ] **Milestone 5 (Phase 3)**: ESP32-CAM board firmware and off-device AI voice service pipeline.

---

## 6. Budget & BOM Summary

Tracked target prototype budget: **LKR 20,000 – 35,000**
- **Phase 1 Prototype Cost**: **~LKR 9,500** (under LKR 10,000 ceiling)
- **Total Multi-phase Estimated Cost**: **~LKR 19,350** (achieves full system under target ceiling)
- See [`hardware/BOM.md`](./hardware/BOM.md) for individual component pricing and local supplier references.
