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
│ • VL53L1X Forward Laser ToF     │      │ • u-blox NEO-6M / NEO-8M GPS    │      │ • Dedicated ESP32-CAM Board     │
│ • Downward Ultrasonic / ToF     │      │ • Wi-Fi Telemetry Uplink        │      │ • Cloud/Phone VLM (Gemini/GPT4o)│
│ • MPU6050 6-Axis Tilt & Fall    │      │ • Optional LoRa Off-Grid Uplink │      │ • Cloud Neural Text-to-Speech   │
│ • Proportional Haptic PWM       │      │ • Family Web Dashboard          │      │ • Spoken Ambient Scene Q&A      │
│ • Double-Burst Drop-off Pattern │      │ • Emergency Geolocation Push    │      │ • Off-device heavy computation  │
│ • 100% Offline & Deterministic  │      │                                 │      │                                 │
└─────────────────────────────────┘      └─────────────────────────────────┘      └─────────────────────────────────┘
```

1. **Obstacle & Drop-Off Detection (Phase 1 — Current Focus)**:
   - **Safety-Critical & Fully Offline**: Operates without any internet, Bluetooth, or cloud dependencies.
   - **Forward Proximity**: VL53L1X Time-of-Flight sensor measures distance up to 4 meters, modulating ERM vibration motor PWM intensity (closer = stronger vibration).
   - **Ground Drop-offs**: Downward-angled ultrasonic sensor or second ToF detects descending stairs, curbs, holes, or open drains, triggering an unmistakable **double-burst tactile vibration signature**.
   - **Audible Hazard Warning**: Piezo buzzer activates when forward obstacles are dangerously close ($< 30\text{ cm}$).
   - **Fall & Tilt Sensing**: MPU6050 IMU detects sudden drops, impact spikes, and extended immobility on the floor.

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
│   ├── core-sensing/               # Phase 1: Obstacle + drop detection + vibration PWM
│   │   ├── include/                # Sensor drivers, state machine, and config headers
│   │   ├── src/                    # Driver and controller implementations
│   │   ├── platformio.ini          # PlatformIO configuration (board: esp32dev)
│   │   └── README.md
│   ├── gps-tracking/               # Phase 2: GPS NMEA parsing & telemetry reporting
│   └── cam-module/                 # Phase 3: Dedicated ESP32-CAM snapshot streamer
├── ai-voice-service/                # Phase 3: Cloud/companion VLM + TTS pipeline
├── dashboard/                       # Family-facing web dashboard (HTML/JS)
├── hardware/                        # Hardware schematics, wiring, and BOM
│   ├── wiring.md                   # Pinouts, MOSFET driver schematic, voltage dividers
│   ├── BOM.md                      # Component list and LKR budget tracking
│   └── README.md
├── docs/                            # Architectural specifications & decision logs
│   ├── architecture.md             # System architecture & timing diagrams
│   ├── decisions.md                # Architecture Decision Records (ADRs)
│   └── README.md
├── .gitignore
└── README.md
```

---

## 3. Hardware Pin Mapping (Core Sensing — ESP32)

| Component | Function | ESP32 GPIO | Logic Level | Notes |
|:---|:---|:---:|:---:|:---|
| **VL53L1X** | I2C Data (`SDA`) | **GPIO 21** | 3.3V | Shared 400kHz I2C bus (Addr: `0x29`) |
| **VL53L1X** | I2C Clock (`SCL`) | **GPIO 22** | 3.3V | Shared 400kHz I2C bus |
| **MPU6050** | I2C Data / Clock | **GPIO 21 / 22** | 3.3V | Shared I2C bus (Addr: `0x68`, AD0 to GND) |
| **Downward Sensor** | Ultrasonic Trigger | **GPIO 18** | 3.3V | 10 µs trigger pulse |
| **Downward Sensor** | Ultrasonic Echo | **GPIO 5** | 3.3V | 5V→3.3V divider (1kΩ/2kΩ) if using HC-SR04 |
| **Downward Sensor (Alt)**| VL53L0X XSHUT | **GPIO 19** | 3.3V | Readdresses VL53L0X to `0x30` on boot |
| **Haptic Motor** | Transistor Gate/Base | **GPIO 25** | 3.3V PWM | LEDC 5kHz PWM (Duty 0–255) |
| **Piezo Buzzer** | Alarm Audio | **GPIO 26** | 3.3V | Urgent proximity alarm (<30cm) & fall alert |
| **Status LED** | Visual Indicator | **GPIO 2** | 3.3V | Slow pulse = OK; Fast blink = Hazard |
| **SOS Button** | Emergency / Reset | **GPIO 27** | 3.3V | Input pull-up (Active LOW) |

*Full driver schematics and flyback diode wiring are documented in [`hardware/wiring.md`](./hardware/wiring.md).*

---

## 4. Getting Started for Contributors

### Prerequisites
1. Install [PlatformIO Core](https://platformio.org/install/cli) or the [PlatformIO IDE extension](https://platformio.org/install/ide?install=vscode) in VS Code.
2. Clone this repository:
   ```bash
   git clone https://github.com/Neural-Nexus-IoTrix-2-0/intelligent-cane.git
   cd intelligent-cane
   ```

### Building Phase 1 Firmware
```bash
cd firmware/core-sensing
pio run
```

### Flashing to ESP32
Connect your ESP32 board via micro-USB or USB-C:
```bash
pio run --target upload
```

### Live Telemetry Monitoring
Open the serial console at 115200 baud:
```bash
pio device monitor -b 115200
```

Sample output:
```text
=======================================================
  INTELLIGENT CANE — PHASE 1 CORE SENSING BOOTING
  Neural-Nexus IoTrix 2.0 (Track A Embedded IoT)
=======================================================
[System] Initializing I2C bus (SDA=GPIO21, SCL=GPIO22 @ 400kHz)...
[ForwardSensor] VL53L1X initialized successfully (Continuous Medium Mode @ 30Hz)
[DownwardSensor] Baseline calibrated: 40.2 cm (from 5 samples)
[MotionSensor] MPU6050 initialized successfully (Range: ±8G, Filter: 21Hz)
[HapticFeedback] LEDC PWM initialized on GPIO 25 (5kHz, 8-bit)
[BuzzerAlert] Buzzer initialized on GPIO 26
-------------------------------------------------------
  Forward ToF:    [OK (VL53L1X)]
  Downward:       [OK]
  MPU6050 IMU:    [OK]
  Haptic Motor:   [OK (GPIO25 LEDC CH0)]
  Piezo Buzzer:   [OK (GPIO26)]
=======================================================

[TELEM] Fwd: 112.4 cm | Down:  40.1 cm (Base: 40.2) | Tilt: 14.2 deg | G: 1.00 | Haptic: 115 PWM (PROP) | Buzz: 0
[TELEM] Fwd:  24.5 cm | Down:  40.0 cm (Base: 40.2) | Tilt: 13.8 deg | G: 0.99 | Haptic: 255 PWM (PROP) | Buzz: 1
[TELEM] Fwd: 180.0 cm | Down:  68.4 cm (Base: 40.2) | Tilt: 15.0 deg | G: 1.01 | Haptic: 240 PWM (DROP_OFF!) | Buzz: 0
```

---

## 5. Development Roadmap

- [x] **Milestone 1**: Scaffolding, architecture design, and ADR documentation.
- [x] **Milestone 2**: Phase 1 core sensing firmware (VL53L1X driver, Downward ground sensor, MPU6050 IMU, proportional haptic PWM, drop-off double-burst pattern, buzzer alarm).
- [ ] **Milestone 3**: Physical bench testing & sensor calibration on cane prototype hardware.
- [ ] **Milestone 4 (Phase 2)**: GPS tracking subsystem and caregiver web dashboard integration.
- [ ] **Milestone 5 (Phase 3)**: ESP32-CAM board firmware and off-device AI voice service pipeline.

---

## 6. Budget & BOM Summary

Tracked target prototype budget: **LKR 20,000 – 35,000**
- **Phase 1 Estimated Cost**: ~LKR 13,250
- **Total Multi-phase Estimated Cost**: ~LKR 23,100
- See [`hardware/BOM.md`](./hardware/BOM.md) for individual component pricing and local supplier references.
