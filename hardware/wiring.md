# Intelligent Cane Integrated Hardware Wiring Reference (1× TOF + 1× Ultrasonic)

This document details the active electrical wiring for the multi-sensor Intelligent Cane prototype featuring the **ESP32-C3 SuperMini** (Core Sensing & Haptics) powering and controlling the **ESP32-CAM** (AI Vision Subsystem), fusing **1× TOF10120 Laser Time-of-Flight sensor** and **1× HC-SR04 Ultrasonic sensor** for obstacle detection.

---

## 1. ESP32-C3 SuperMini Pin Assignment Table

| Peripheral | Signal | ESP32-C3 Pin | Electrical Configuration / Function |
|---|---|:---:|---|
| **Ultrasonic Sensor (1× HC-SR04)** | TRIG | **GPIO 0** | 10 µs trigger pulse output (Forward acoustic ranging) |
| **Ultrasonic Sensor (1× HC-SR04)** | ECHO | **GPIO 1** | Direct connection to GPIO 1 (bench prototype) |
| **Shared I2C Bus** | SDA | **GPIO 4** | Hardware I2C at 100 kHz; shared by **TOF10120** (`0x52`) and **MPU6050** (`0x68`) |
| **Shared I2C Bus** | SCL | **GPIO 5** | Hardware I2C at 100 kHz; shared by **TOF10120** (`0x52`) and **MPU6050** (`0x68`) |
| **Motor 1 (Primary Proximity)** | IN / SIG | **GPIO 6** | Forward obstacle proximity tactile feedback (0–60 cm); LEDC Ch 0, 200 Hz PWM |
| **Piezo Buzzer** | Signal | **GPIO 7** | Proximity warning beeps and high-urgency fall alarm tones |
| **Motor 2 (Critical Hazard Boost)** | IN / SIG | **GPIO 9** | Critical proximity (<25 cm) high-intensity pulse alert; LEDC Ch 1, 200 Hz PWM |
| **Camera Trigger Line** | SIG Out | **GPIO 21** | Active-LOW pulse (~80 ms) sent to ESP32-CAM GPIO 13 to command instant AI snapshot |
| **5V Power Out to CAM** | 5V | **5V Pin** | Directly supplies 5V from USB to ESP32-CAM 5V pin |
| **Common Ground to CAM** | GND | **GND Pin** | Common reference between ESP32-C3 and ESP32-CAM |
| *(Pushbutton)* | *(None)* | *GPIO 3* | Not connected (Trigger camera via Serial `c` or BLE `"describe"`; auto-reset on upright) |
| *(Status LED)* | *(None)* | *GPIO 8* | Not connected (bench build without external LED) |
| *(GPIO 2, GPIO 10)* | *(Unused)* | *2, 10* | Freed up |

---

## 2. ESP32-CAM Module Wiring (Powered by SuperMini)

| Signal | ESP32-CAM Pin | Connects to ESP32-C3 Pin | Function |
|---|:---:|:---:|---|
| **5V Power** | **5V** | **5V Pin** on ESP32-C3 | Powered directly from the C3 USB 5V rail |
| **Ground** | **GND** | **GND Pin** on ESP32-C3 | Common ground reference |
| **Trigger Input** | **GPIO 13** | **GPIO 21** on ESP32-C3 | Receives active-LOW pulse to trigger Gemini AI snapshot |

> [!IMPORTANT]
> **Power Delivery Tip**: The ESP32-CAM can draw current spikes up to 450mA during Wi-Fi transmission bursts and camera image processing. Ensure the USB-C cable plugged into the ESP32-C3 SuperMini is connected to a quality 5V USB charger (1.5A–2.0A) or high-power USB 3.0 port. If the camera reboots during capture, place a 100 µF–470 µF capacitor across the CAM's 5V and GND pins.

---

## 3. Subsystem Interconnect & Power Architecture

```
       [USB-C Power 5V / 2A]
                 │
                 ▼
       ┌────────────────────────┐
       │   ESP32-C3 SuperMini   │
       │   5V Pin ──────────────┼──────> 5V Pin  ┌──────────────────┐
       │   GND Pin ─────────────┼──────> GND Pin │  ESP32-CAM AI    │
       │   GPIO 21 (Trigger) ───┼──────> GPIO 13 │  Vision Module   │
       └─────────┬──────────────┘                └──────────────────┘
                 │
     Shared I2C  ├──────> TOF10120 (SDA: GPIO 4 [Blue], SCL: GPIO 5 [Green])
     Shared I2C  ├──────> MPU6050  (SDA: GPIO 4, SCL: GPIO 5)
     Ultrasonic  ├──────> HC-SR04  (TRIG: GPIO 0, ECHO: GPIO 1, VCC: 5V, GND: GND)
     Haptics     ├──────> Motor 1 (GPIO 6) & Motor 2 (GPIO 9)
     Audio       └──────> Piezo Buzzer (GPIO 7)
```

### Sensor & Actuator Connection Details:
1. **TOF10120 Laser Distance Sensor (I2C Addr `0x52`) Wire Harness**:
   | Wire Color | Pin Label | Function | Connect to ESP32-C3 / Power |
   |:---|:---|:---|:---|
   | **Black** | `GND` | Ground | System **GND** |
   | **Red** | `VDD / VCC` | Power (3.3V–5V) | **3.3V** (or 5V rail) |
   | **Yellow** | `RXD` | UART Receive | *Leave Disconnected* (I2C mode) |
   | **White** | `TXD` | UART Transmit | *Leave Disconnected* (I2C mode) |
   | **Blue** | `SDA` | I2C Serial Data | **GPIO 4** (Shared I2C SDA) |
   | **Green** | `SCL` | I2C Serial Clock | **GPIO 5** (Shared I2C SCL) |
2. **MPU6050 Motion IMU (I2C Addr `0x68`)**:
   - `VCC` -> 3.3V (or 5V if GY-521 board with onboard 3.3V LDO)
   - `GND` -> GND
   - `SDA` -> ESP32-C3 GPIO 4 (shared with TOF Blue wire)
   - `SCL` -> ESP32-C3 GPIO 5 (shared with TOF Green wire)
3. **Ultrasonic Sensor (1× HC-SR04)**:
   - `VCC` to 5V rail
   - `GND` to common GND
   - `TRIG` to ESP32-C3 GPIO 0
   - `ECHO` to ESP32-C3 GPIO 1
4. **Dual Vibration Actuators (Spatial & Urgency Feedback)**:
   - **Motor 1 (Primary Proximity)**: Signal to GPIO 6. Proportional vibration as obstacles enter 60 cm range down to 10 cm.
   - **Motor 2 (Critical Hazard Boost)**: Signal to GPIO 9. Delivers rapid urgent pulsing when an obstacle is within 25 cm.
   - *Note*: Ensure vibration modules include onboard transistor drivers. Direct MCU GPIO cannot drive motor inductive coils directly.
5. **On-Demand Camera Triggering (Without Physical Button)**:
   - **Via Serial Monitor**: Open USB Serial at 115200 baud and type `c` or `describe` or `capture` + Enter.
   - **Via Bluetooth**: Send `"describe"`, `"cam"`, or `"capture"` over BLE NUS characteristic.

---

## 4. Verification & Testing Checklist

1. [ ] **Power & Common Ground**: Confirm ESP32-CAM 5V and GND connect to the ESP32-C3 5V and GND pins.
2. [ ] **Trigger Wire**: Confirm single jumper wire from ESP32-C3 GPIO 21 to ESP32-CAM GPIO 13.
3. [ ] **I2C Check**: Power up C3 and observe USB Serial (115200 baud). Firmware confirms `[I2C] Scanning bus...` finding `0x52` (TOF10120) and `0x68` (MPU6050).
4. [ ] **Multi-Modal Distance Validation**:
   - Move an obstacle in front of the TOF sensor $\rightarrow$ Serial outputs `ToF: XX.Xcm`.
   - Move a hand in front of the Ultrasonic sensor $\rightarrow$ Serial outputs `US: YY.Ycm`.
   - Notice `Dist:` automatically reflects whichever sensor sees the closest hazard.
5. [ ] **AI Camera Trigger**: In the C3 Serial Monitor, type `c` and press Enter. Verify C3 logs `[Serial] AI Camera capture commanded...` and ESP32-CAM logs `Hardware button pressed! Triggering Gemini Vision...`.
6. [ ] **Fall Detection & App Sync**: Tilt cane past 65°. Motors silence immediately, buzzer sounds fall tone, and BLE dispatches `FALL_STATE:1` to companion app. Returning upright automatically clears with `FALL_STATE:0`.
