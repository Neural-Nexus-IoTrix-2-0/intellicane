# Intelligent Cane Integrated Hardware Wiring Reference

This document details the complete active electrical wiring for the multi-sensor Intelligent Cane prototype featuring the **ESP32-C3 SuperMini** (Core Sensing & Haptics) and **ESP32-CAM** (AI Vision Subsystem).

---

## 1. ESP32-C3 SuperMini Pin Assignment Table

| Peripheral | Signal | ESP32-C3 GPIO | Electrical Configuration / Function |
|---|---|:---:|---|
| **Forward Ultrasonic (US1)** | TRIG | 0 | 10 µs trigger pulse output (Forward / Chest obstacle ranging) |
| **Forward Ultrasonic (US1)** | ECHO | 1 | Direct connection to GPIO 1 (bench prototype) |
| **Lower Ultrasonic (US2)** | TRIG | 2 | 10 µs trigger pulse output (Lower / Knee-level obstacle ranging) |
| **Lower Ultrasonic (US2)** | ECHO | 10 | Direct connection to GPIO 10 |
| **Pushbutton** | Switch to GND | 3 | Internal `INPUT_PULLUP`. Short press: manual AI camera trigger; fall alarm manual reset |
| **I2C Bus (Shared)** | SDA | 4 | Hardware I2C at 100 kHz; 3.3V pull-ups. Shared by **MPU6050** (`0x68`) and **TOF10120** (`0x52`) |
| **I2C Bus (Shared)** | SCL | 5 | Hardware I2C at 100 kHz; 3.3V pull-ups. Shared by **MPU6050** (`0x68`) and **TOF10120** (`0x52`) |
| **Motor 1 (Forward Obstacle)** | IN / SIG | 6 | Forward obstacle proximity tactile feedback; LEDC Ch 0, 200 Hz PWM |
| **Piezo Buzzer** | Signal | 7 | Proximity warning beeps and high-urgency fall alarm tones |
| **Status LED** | Onboard | 8 | Active-LOW heartbeat / alert indicator |
| **Motor 2 (Ground Drop / Curb)** | IN / SIG | 9 | Ground drop-off (>150 cm / out-of-range) and curb (<80 cm) haptic feedback; LEDC Ch 1, 200 Hz PWM |
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

### Sensor & Actuator Power Details:
1. **Power Supply**:
   - Power both MCUs, HC-SR04 sensors, TOF10120, and vibration motor modules from a regulated **5V source** (e.g. 5V rail or power bank) capable of delivering at least **1.5A–2.0A peak** (ESP32-CAM Wi-Fi bursts draw up to 500mA).
2. **I2C Bus (GPIO 4 SDA, GPIO 5 SCL)**:
   - Connect **MPU6050** (`VCC` to 3.3V or 5V if GY-521 board has onboard 3.3V regulator; `GND` to GND; `SDA` to C3 GPIO 4; `SCL` to C3 GPIO 5).
   - Connect **TOF10120** (`VCC` to 3.3V or 5V; `GND` to GND; `RXD/SDA` to C3 GPIO 4; `TXD/SCL` to C3 GPIO 5).
   - Both sensors communicate over standard I2C at 100 kHz. The C3 firmware includes automatic bus-recovery and device-detect logic so neither sensor blocks the other if disconnected.
3. **Dual Ultrasonic Sensors (Acoustic Isolation)**:
   - **Forward Ultrasonic (US1)**: `VCC` to 5V, `GND` to GND, `TRIG` to GPIO 0, `ECHO` to GPIO 1.
   - **Lower Ultrasonic (US2)**: `VCC` to 5V, `GND` to GND, `TRIG` to GPIO 2, `ECHO` to GPIO 10.
   - Ping timing is **alternated interleaved** in firmware (Cycle 0 = US1, Cycle 1 = US2) to completely eliminate acoustic echo cross-talk between sensors.
4. **Dual Vibration Actuators (Spatial Dual-Zone Feedback)**:
   - **Motor 1 (Forward Obstacles)**: Signal to GPIO 6. Vibrates with progressive intensity and frequency as obstacles approach within 60 cm.
   - **Motor 2 (Ground Hazards)**: Signal to GPIO 9. Vibrates with pulsing pattern on ground drop-off / stairs down (>150 cm / out-of-range) and steady warning on curbs / steps (<80 cm).
   - *Note*: Ensure vibration modules include onboard transistor drivers. Direct MCU GPIO cannot drive motor inductive coils directly.
5. **Camera Snapshot Trigger**:
   - Run a single jumper wire from **ESP32-C3 GPIO 21** to **ESP32-CAM GPIO 13**.
   - Triggering happens automatically on:
     - User pressing cane button (GPIO 3).
     - Android app sending `"describe"` command over BLE NUS.
     - Critical proximity warning threshold.

---

## 4. Verification & Testing Checklist

1. [ ] **Common Ground**: Confirm all module GND pins connect to the single system ground.
2. [ ] **I2C Check**: Power up C3 and observe USB Serial (115200 baud). Firmware should confirm `[I2C] Scanning bus...` finding `0x52` (TOF10120) and `0x68` (MPU6050).
3. [ ] **Ultrasonic Validation**: Bring a hand towards US1 and US2 separately. Serial will output `[US1] d=XX.X cm` and `[US2] d=YY.Y cm`.
4. [ ] **Dual Haptics Test**:
   - Bring obstacle close to forward sensor: Motor 1 (GPIO 6) spins up smoothly.
   - Tilt cane upward / aim TOF10120 into open distance (>150 cm): Motor 2 (GPIO 9) pulses to warn of drop-off.
5. [ ] **AI Camera Trigger**: Press button on GPIO 3. Verify C3 logs `[CAM] Triggering camera snapshot (GPIO 21 pulse)` and ESP32-CAM logs `Hardware button pressed! Triggering Gemini Vision...`.
6. [ ] **Fall Detection & App Sync**: Tilt cane past 65°. Motors silence immediately, buzzer sounds fall tone, and BLE dispatches `FALL_STATE:1` to companion app. Returning upright clears with `FALL_STATE:0`.
