# IoTrix 2.0: Semi-Final Technical Progress Sheet

**Project Title:** Intelligent Cane — Multimodal Offline-Safe Mobility Assistant  
**Track:** Track A — Embedded IoT System Development  
**Team Name:** Neural-Nexus  
**GitHub Repository:** [https://github.com/Neural-Nexus-IoTrix-2-0/intelligent-cane](https://github.com/Neural-Nexus-IoTrix-2-0/intelligent-cane)  
**Submission Date:** 5 October 2026  
**Bench Prototype Cost (Phase 1):** LKR 2,800 (Budget Ceiling: LKR 20,000–35,000)  

---

## 1. Problem
Visually impaired individuals face persistent safety hazards during unassisted navigation, including unexpected mid-body/head-level obstacles, sudden drop-offs (curbs, staircases, open drains), and accidental falls that leave users stranded without help. Existing commercial electronic travel aids (ETAs) suffer from prohibitive costs ($>\$500$ USD / LKR 150,000+), heavy reliance on uninterrupted internet/cloud connectivity for basic navigation, high power consumption, and overwhelming audio alerts that mask essential environmental auditory cues. There is an urgent need for an affordable, low-latency, 100% offline-safe intelligent mobility aid that provides intuitive tactile feedback and emergency fall detection.

---

## 2. Proposed Solution
The **Intelligent Cane** is a low-cost, lightweight, multimodal mobility system centered around an **ESP32-C3 SuperMini** microcontroller. The solution is structured across three modular phases:
1. **Phase 1 (Core Offline Reflex - Implemented & Verified):** A deterministic, sub-50 ms safety loop providing continuous, proportional haptic vibration for forward obstacle proximity (HC-SR04, 60 cm to 10 cm range) and orientation/fall detection with automatic alarm clearing (MPU6050 6-axis IMU).
2. **Phase 2 (BLE Smartphone Telemetry & Geolocation - In Progress):** Replaces expensive standalone GPS hardware and power-hungry cellular modems by streaming telemetry and fall alerts over Bluetooth Low Energy (BLE 5.0 Nordic UART Service) to a companion smartphone app, leveraging the phone's native A-GPS and 4G/5G connection to notify caregivers via a web dashboard.
3. **Phase 3 (On-Demand AI Vision - Planned):** An ESP32-CAM unit triggered on demand to capture snapshots and stream them via Wi-Fi to a Vision-Language Model (VLM) for scene narration via Bluetooth audio.

---

## 3. Architecture
The system architecture separates safety-critical real-time reflexes from high-level connectivity to guarantee 100% user safety even in complete network blackouts:

```
+-----------------------------------------------------------------------------------+
|               SUBSYSTEM 1: SAFETY-CRITICAL SENSING & FEEDBACK (OFFLINE)           |
|                                                                                   |
|  +--------------------+        +-----------------------+        +--------------+  |
|  | HC-SR04 Ultrasonic |------->|                       |------->| 3-Pin Motor  |  |
|  | (Trig: 0, Echo: 1) |        |                       | (PWM)  | (GPIO 6)     |  |
|  +--------------------+        |  ESP32-C3 SuperMini   |        +--------------+  |
|                                |   (160 MHz RISC-V)    |                          |
|  +--------------------+  I2C   |                       |------->| Piezo Buzzer |  |
|  | MPU6050 6-Axis IMU |<------>|  Non-Blocking Loop    |        | (GPIO 7)     |  |
|  | (SDA: 4, SCL: 5)   |        |  (50ms/20ms/10ms)     |        +--------------+  |
|  +--------------------+        +-----------------------+                          |
+--------------------------------------------|--------------------------------------+
                                             | BLE 5.0 (Nordic UART Service)
                                             v
+-----------------------------------------------------------------------------------+
|               SUBSYSTEM 2: SMARTPHONE TELEMETRY & CAREGIVER CLOUD                 |
|                                                                                   |
|  +-----------------------+        +--------------------+        +--------------+  |
|  | Companion Smartphone  |------->| Smartphone A-GPS   |------->| Caregiver    |  |
|  | App (BLE Central)     |        | Location & 4G Data |        | Web Dashboard|  |
|  +-----------------------+        +--------------------+        +--------------+  |
+-----------------------------------------------------------------------------------+
```

---

## 4. Current Progress
- **Physical Bench Hardware Prototype:** Fully assembled and bench-verified on ESP32-C3 SuperMini with HC-SR04, GY-521 MPU6050, 3-pin vibration motor module, active/passive buzzer, and status LED.
- **Proportional Haptic Driver:** Hardware LEDC 200 Hz PWM smoothly modulates vibration intensity inversely proportional to distance between 60 cm (start of warning) and 10 cm (maximum haptic buzz).
- **Fall Detection & Auto-Recovery:** Pitch/roll orientation computed continuously. Sustained tilt $>65^\circ$ triggers urgent audio-visual alarm; restoring cane upright ($<30^\circ$) automatically clears alarm state.
- **Resilient I2C & Multi-IC Driver:** Custom register-level MPU6050/6500 driver supporting clone ICs, dual I2C addresses (`0x68`/`0x69`), automatic bus lockup recovery, and background reconnect.
- **Wireless BLE Telemetry (NUS):** Implemented Nordic UART Service (`6E400001-...`). The cane broadcasts live telemetry and mirrors serial monitor outputs wirelessly to any BLE terminal / smartphone app while displaying the peer device's MAC address.
- **Digital Twin Simulation:** Wokwi simulation workspace fully configured and validated (`simulation/wokwi/`).

---

## 5. Technology Stack
- **Hardware:** ESP32-C3 SuperMini (RISC-V 32-bit single-core @ 160 MHz, 400 KB SRAM, 4 MB Flash, BLE 5.0), HC-SR04 Ultrasonic Transceiver, GY-521 MPU6050 6-Axis IMU, 3-Pin Vibration Motor Breakout, 3.3V/5V Piezo Buzzer, 5V Regulated Power Rail.
- **Firmware:** Embedded C++ (C++17), Arduino ESP32 Core v3.x, ESP32 LEDC PWM Hardware Timer, Wire I2C Library, ESP32 BLE Arduino Library (Nordic UART Service).
- **Toolchains & Build Systems:** Arduino CLI (`esp32:esp32:esp32c3:CDCOnBoot=cdc,FlashMode=dio`), PlatformIO Core (`esp32-c3-devkitm-1`), Clang++ (Host test suite).
- **Communication Protocols:** I2C (100 kHz standard mode), UART/USB CDC (115200 baud), BLE 5.0 GATT (Nordic UART Service: RX `6E400002`, TX `6E400003`).
- **Testing & Simulation:** C++ native unit testing framework (`tests/proximity_feedback_test.cpp`), Wokwi Web Simulator (`simulation/wokwi/diagram.json`).

---

## 6. Testing & Validation Results
- **Host Unit Testing:** 100% pass on boundary conditions, invalid distances (negative, zero, NaN, $\infty$), monotonic PWM ramp progression, motor active-high/low polarity inversion, buzzer beep envelope state transitions, and 32-bit `millis()` rollover resilience.
- **Bench Ranging Accuracy:** HC-SR04 verified across 2 cm to 250 cm against tape measure calibration with $\pm 1.5\text{ cm}$ precision. Vibration engages deterministically at $<60.0\text{ cm}$.
- **Tilt Angle & Fall Verification:** Tilt angle calibrated with digital protractor. Fall trigger activates consistently at $>65^\circ$ tilt after 1.5 s debounce; alarm automatically silences within 200 ms of restoring the cane to $<30^\circ$.
- **BLE Telemetry Latency & Range:** Telemetry stream verified at 10 Hz over BLE to smartphone terminal up to 10 meters line-of-sight with $<25\text{ ms}$ packet arrival latency.
- **Actuator Power & Thermals:** LEDC 200 Hz PWM motor driver bench tested under full 100% duty cycle for 30 minutes continuous run; motor driver transistor and MCU maintained ambient temperature ($<32^\circ\text{C}$).

---

## 7. Known Limitations & Identified Risks
1. **Ultrasonic Specular Reflection:** HC-SR04 sound waves can scatter on angled soft fabrics or sound-absorbing obstacles beyond 2 meters (Mitigation: In Phase 2/3, integrate forward ToF laser or complementary vision).
2. **Jumper Wire Interconnects:** Prototype uses solderless breadboard jumpers susceptible to mechanical vibration during vigorous tapping (Mitigation: Design custom PCB and soldered wiring harness for final competition).
3. **Smartphone BLE Dependency for Geolocation:** In Phase 2, if the user's phone battery dies or Bluetooth is disabled, remote cloud telemetry/GPS is unavailable (Mitigation: Core navigation and local fall buzzer remain 100% functional offline on the cane).
4. **Environmental Acoustic Noise:** High ambient street noise may mask low-volume buzzer frequencies (Mitigation: Haptic tactile vibration in the handle serves as the primary non-auditory channel).

---

## 8. Next Steps & Planned Improvements for Final
1. **Phase 2 Smartphone Companion App & Cloud Integration:** Build Android/iOS companion app to parse BLE telemetry, capture real-time A-GPS coordinates, and push live status and SOS SMS/alerts to the caregiver web dashboard.
2. **Custom PCB & Ergonomic 3D-Printed Enclosure:** Replace prototype jumpers with a custom two-layer PCB and fabricate a weather-resistant, ergonomically balanced 3D-printed cane handle with modular sensor mounts.
3. **Phase 3 Vision Module:** Mount ESP32-CAM on the cane shaft with an ergonomic tactile thumb button to trigger AI-driven scene descriptions and object classification.
4. **Power Optimization & Battery Management:** Integrate a 18650 Li-ion battery pack with TP4056 charge controller and implement ESP32-C3 Light Sleep during cane immobility to extend battery runtime to $>24\text{ hours}$.
