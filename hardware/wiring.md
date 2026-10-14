# Intelligent Cane Integrated Hardware Wiring Reference (Multi-Modal TOF + Ultrasonic)

This document details the complete active electrical wiring for the multi-sensor Intelligent Cane prototype featuring the **ESP32-C3 SuperMini** (Core Sensing & Haptics) and **ESP32-CAM** (AI Vision Subsystem), fusing both the **TOF10120 Laser Time-of-Flight sensor** and **HC-SR04 Ultrasonic sensor(s)** for comprehensive obstacle detection.

---

## 1. ESP32-C3 SuperMini Pin Assignment Table

| Peripheral | Signal | ESP32-C3 GPIO | Electrical Configuration / Function |
|---|---|:---:|---|
| **Forward Ultrasonic (US1)** | TRIG | 0 | 10 µs trigger pulse output (Forward / Chest obstacle acoustic ranging) |
| **Forward Ultrasonic (US1)** | ECHO | 1 | Direct connection to GPIO 1 (bench prototype) |
| **Lower Ultrasonic (US2)** | TRIG | 2 | 10 µs trigger pulse output (Lower / Ground obstacle acoustic ranging) |
| **Lower Ultrasonic (US2)** | ECHO | 10 | Direct connection to GPIO 10 |
| **Pushbutton** | Switch to GND | 3 | Internal `INPUT_PULLUP`. Short press: manual AI camera trigger; fall alarm manual reset |
| **I2C Bus (Shared)** | SDA | 4 | Hardware I2C at 100 kHz; 3.3V pull-ups. Shared by **TOF10120** (`0x52`) and **MPU6050** (`0x68`) |
| **I2C Bus (Shared)** | SCL | 5 | Hardware I2C at 100 kHz; 3.3V pull-ups. Shared by **TOF10120** (`0x52`) and **MPU6050** (`0x68`) |
| **Motor 1 (Primary Proximity)** | IN / SIG | 6 | Forward obstacle proximity tactile feedback (0–60 cm); LEDC Ch 0, 200 Hz PWM |
| **Piezo Buzzer** | Signal | 7 | Proximity warning beeps and high-urgency fall alarm tones |
| **Status LED** | Onboard | 8 | Active-LOW heartbeat / alert indicator |
| **Motor 2 (Critical Hazard Boost)** | IN / SIG | 9 | Critical proximity (<25 cm) high-intensity pulse alert; LEDC Ch 1, 200 Hz PWM |
| **Camera Trigger Line** | SIG Out | 21 | Active-LOW pulse (~80 ms) sent to ESP32-CAM GPIO 13 to command instant AI snapshot |

---

## 2. ESP32-CAM AI Vision Module Pin Assignment

| Peripheral | Signal | ESP32-CAM Pin | Electrical Configuration / Function |
|---|---|:---:|---|
| **Power Supply** | 5V / GND | 5V / GND | Connected to common 5V power bus and common system GND |
| **Hardware Snapshot Trigger** | TRIG In | GPIO 13 | Internal `INPUT_PULLUP`. Receives active-LOW pulse from ESP32-C3 GPIO 21 |
| **OV2640 Camera** | D0–D7, PCLK, XCLK, etc. | Standard AI-Thinker Pins | High-speed camera interface for MJPEG streaming and Gemini AI captures |
| **Serial / Programming** | U0TXD / U0RXD | GPIO 1 / 3 | Connects to USB-to-UART / ESP32-CAM-MB dev board for flashing & serial monitoring |

---

## 3. Subsystem Interconnect & Power Architecture

```
                 +-----------------------------------------+
                 |          Common 5V Power Rail           |
                 +----+-------------------------------+----+
                      |                               |
                      v 5V                            v 5V
             +-----------------+             +-----------------+
             |  ESP32-C3 MCU   |             |  ESP32-CAM MCU  |
             | (Core Reflexes) |             | (Gemini AI & UI)|
             +--------+--------+             +--------+--------+
                      |                               ^
                      | GPIO 21 (Trigger Pulse)       | GPIO 13 (Trigger In)
                      +-------------------------------+
                      |
        +-------------+-------------+
        | Common Ground (GND) Bus   | <--- ALL modules MUST share GND!
        +---------------------------+
```

### Sensor & Actuator Connection Details:
1. **Power Supply**:
   - Power both MCUs, HC-SR04 sensors, TOF10120, and vibration motor modules from a regulated **5V source** (e.g. 5V rail or power bank) capable of delivering at least **1.5A–2.0A peak** (ESP32-CAM Wi-Fi bursts draw up to 500mA).
2. **TOF10120 Laser Distance Sensor (I2C Addr `0x52`) Wire Harness**:
   | Wire Color | Pin Label | Function | Connect to ESP32-C3 / Power |
   |:---|:---|:---|:---|
   | **Black** | `GND` | Ground | System **GND** |
   | **Red** | `VDD / VCC` | Power (3.3V–5V) | **3.3V** (or 5V rail) |
   | **Yellow** | `RXD` | UART Receive | *Leave Disconnected* (I2C mode) |
   | **White** | `TXD` | UART Transmit | *Leave Disconnected* (I2C mode) |
   | **Blue** | `SDA` | I2C Serial Data | **GPIO 4** (Shared I2C SDA) |
   | **Green** | `SCL` | I2C Serial Clock | **GPIO 5** (Shared I2C SCL) |
3. **MPU6050 Motion IMU (I2C Addr `0x68`)**:
   - `VCC` -> 3.3V (or 5V if GY-521 board with onboard 3.3V LDO)
   - `GND` -> GND
   - `SDA` -> ESP32-C3 GPIO 4 (shared with TOF Blue wire)
   - `SCL` -> ESP32-C3 GPIO 5 (shared with TOF Green wire)
4. **Ultrasonic Sensors (HC-SR04)**:
   - **Forward Ultrasonic (US1)**: `VCC` to 5V, `GND` to GND, `TRIG` to GPIO 0, `ECHO` to GPIO 1.
   - **Lower Ultrasonic (US2 - Optional)**: `VCC` to 5V, `GND` to GND, `TRIG` to GPIO 2, `ECHO` to GPIO 10.
   - Ping timing is alternated interleaved (Cycle 0 = US1, Cycle 1 = US2) to eliminate acoustic echo cross-talk.
5. **Dual Vibration Actuators (Spatial & Urgency Feedback)**:
   - **Motor 1 (Primary Proximity)**: Signal to GPIO 6. Proportional vibration as obstacles enter 60 cm range down to 10 cm.
   - **Motor 2 (Critical Hazard Boost)**: Signal to GPIO 9. Delivers rapid urgent pulsing when an obstacle is within 25 cm.
   - *Note*: Ensure vibration modules include onboard transistor drivers. Direct MCU GPIO cannot drive motor inductive coils directly.
6. **Camera Snapshot Trigger**:
   - Run a single jumper wire from **ESP32-C3 GPIO 21** to **ESP32-CAM GPIO 13**.
   - Triggering happens automatically on:
     - User pressing cane button (GPIO 3).
     - Android app sending `"describe"` command over BLE NUS.

---

## 4. Verification & Testing Checklist

1. [ ] **Common Ground**: Confirm all module GND pins connect to the single system ground.
2. [ ] **I2C Check**: Power up C3 and observe USB Serial (115200 baud). Firmware should confirm `[I2C] Scanning bus...` finding `0x52` (TOF10120) and `0x68` (MPU6050).
3. [ ] **Multi-Modal Distance Validation**:
   - Move an obstacle in front of the TOF sensor $\rightarrow$ Serial outputs `ToF: XX.Xcm`.
   - Move a hand in front of the Ultrasonic sensor $\rightarrow$ Serial outputs `US: YY.Ycm`.
   - Notice `Dist:` automatically reflects whichever sensor sees the closest hazard.
4. [ ] **Haptics Test**:
   - Bring obstacle within 60 cm: Motor 1 (GPIO 6) spins up smoothly and buzzer beeps.
   - Move obstacle within 25 cm: Motor 2 (GPIO 9) engages in rapid pulsing alert.
5. [ ] **AI Camera Trigger**: Press button on GPIO 3. Verify C3 logs `[CAM] Triggering camera snapshot (GPIO 21 pulse)` and ESP32-CAM logs `Hardware button pressed! Triggering Gemini Vision...`.
6. [ ] **Fall Detection & App Sync**: Tilt cane past 65°. Motors silence immediately, buzzer sounds fall tone, and BLE dispatches `FALL_STATE:1` to companion app. Returning upright clears with `FALL_STATE:0`.
