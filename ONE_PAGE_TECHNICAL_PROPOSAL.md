# IntelliCane — One-Page Technical Proposal

**Project Title:** IntelliCane: Smart Assistive Cane with Haptic Obstacle Avoidance, Fall Detection, and a Mobile App for Blind Users and their Caretakers  
**Competition & Track:** IoTrix 2.0 — Track A: Embedded IoT System Development  
**Team Name:** Neural-Nexus | **Members:** Dulnith Liyanage, Thenul Senadheera, Chamith Ranasinghe, Suneth Pathirana  
**Repository:** [https://github.com/Neural-Nexus-IoTrix-2-0/intellicane](https://github.com/Neural-Nexus-IoTrix-2-0/intellicane)  
**Submission Date:** 5 October 2026 | **Bench Prototype Cost:** LKR 2,800  

---

## 1. Problem
Visually impaired individuals face recurring risks from obstacles in their travel path (ground clutter, knee-to-waist obstacles, and head-height hazards), as well as unassisted emergency falls. Commercial electronic travel aids (ETAs) often exceed \$500 USD (LKR 150,000+), depend heavily on continuous cloud connectivity for basic hazard detection, and emit overwhelming audio alerts that drown out crucial environmental sounds. There is an urgent need for an affordable, offline-safe smart white cane that delivers intuitive tactile feedback and emergency fall detection while seamlessly connecting to caregivers.

## 2. Proposed Solution
**IntelliCane** is an affordable, multimodal mobility system combining a deterministic offline safety reflex on the cane with an accessible Android companion application:
- **Core Embedded Reflex (ESP32-C3):** Operates 100% offline with sub-50 ms determinism. An HC-SR04 ultrasonic transceiver detects forward obstacles up to 4 meters, driving a 3-pin vibration motor module via 200 Hz LEDC PWM that scales smoothly between 60 cm and 10 cm. An MPU6050 6-axis IMU detects orientation/impact ($>65^\circ$ tilt) to trigger an emergency buzzer, automatically clearing when restored upright ($<30^\circ$).
- **Companion Android App (`application/`):** Connects via BLE 5.0 (Nordic UART Service) and provides two interfaces: **Blind User Mode** (accessible device status and assistance controls) and **Caretaker Mode** (real-time telemetry monitor, emergency push notifications via Firebase, and live OpenStreetMap tracking leveraging the smartphone's native A-GPS—avoiding the cost, power draw, and satellite blind spots of a standalone hardware GPS module).

## 3. System Architecture
```
[SENSING LAYER]              [CORE REFLEX (CANE)]              [TACTILE & AUDIO FEEDBACK]
HC-SR04 (Trig: 0, Echo: 1) -> ESP32-C3 SuperMini (160 MHz) -> 200 Hz LEDC PWM Motor (GPIO 6)
MPU6050 (I2C: SDA 4, SCL 5)-> Non-blocking Cooperative Loop -> Piezo Buzzer Alert (GPIO 7)
                                       |
                                       v  BLE 5.0 Nordic UART Service (RX: 6E400002 / TX: 6E400003)
[MOBILE APPLICATION]         [LOCATION & CLOUD]                [CARETAKER VIEW]
Android Kotlin App        -> Smartphone A-GPS Geolocation   -> Real-Time OSMDroid Map
(User / Caretaker Mode)   -> Firebase Firestore & Messaging -> Instant Remote Fall Alert Push
```

## 4. Current Progress
- **Bench Prototype Verified:** Physical hardware assembled with ESP32-C3 SuperMini, HC-SR04 (direct Echo to GPIO 1), GY-521 MPU6050, 3-pin vibration motor, and buzzer.
- **Proportional Haptic Driver:** Verified 200 Hz LEDC PWM ramping monotonically from 60 cm down to 10 cm, remaining silent $>60\text{ cm}$ to prevent sensory fatigue.
- **Fall Detection & Self-Clearing:** Pitch/roll tracking activates buzzer alarm on tilt $>65^\circ$ (1.5 s debounce) and automatically silences within 200 ms of upright recovery ($<30^\circ$).
- **Multi-IC Resilient I2C Driver:** Custom direct-register MPU6050/6500 driver supporting clone ICs, dual I2C addresses (`0x68`/`0x69`), and automatic bus lockup recovery.
- **Wireless BLE Telemetry:** Streams live sensor telemetry and connected peer MAC address at 10 Hz over Nordic UART Service (`6E400001-...`).
- **Android Application Implemented:** Complete native Kotlin project (`com.example.intellicane`) with Firebase authentication, OSMDroid live mapping, and BLE client.

## 5. Technology Stack & Component Costs
- **Microcontroller:** ESP32-C3 SuperMini (32-bit RISC-V @ 160 MHz, 400 KB SRAM, 4 MB Flash, BLE 5.0) — **LKR 1,400**
- **Sensing:** HC-SR04 Ultrasonic Transceiver (direct Echo to GPIO 1) — **LKR 450**; GY-521 MPU6050 6-Axis IMU — **LKR 600**
- **Actuation & Audio:** 3-Pin Vibration Motor Breakout (LEDC 200 Hz PWM) — **LKR 250**; 3.3V/5V Piezo Buzzer & Jumpers — **LKR 100**
- **Total Safety-Critical Bench Prototype Cost:** **LKR 2,800** (Tracked within the LKR 20,000–35,000 project budget ceiling).
- **Firmware & Mobile Software:** Embedded C++ (C++17), Arduino-ESP32 Core, Android Kotlin, Jetpack Compose, Firebase, OSMDroid.

## 6. Testing & Validation Results
- **Host Unit Testing (`tests/proximity_feedback_test.cpp`):** 100% pass across boundary values, invalid distances (negative, zero, NaN, $\infty$), monotonic PWM ramp progression, motor active-high/low polarity inversion, beep transitions, and 32-bit `millis()` rollover resilience.
- **Physical Ranging Benchmarks:** HC-SR04 calibrated from 2 cm to 250 cm with $\pm 1.5\text{ cm}$ precision; motor PWM engages deterministically at $<60.0\text{ cm}$.
- **Tilt Angle Verification:** Calibrated with digital protractor; fall alarm activates reliably at $>65^\circ$ and silences at $<30^\circ$.
- **BLE Telemetry Latency:** Verified at 10 Hz over BLE to smartphone terminal up to 10 m line-of-sight with $<25\text{ ms}$ packet arrival latency.

## 7. Limitations & Risk Management
- **Ultrasonic Beam Reflection:** High-incident sound waves can scatter on angled soft fabrics beyond 2 m (*Mitigation:* Integrate complementary Time-of-Flight laser on final build).
- **Jumper Wire Interconnects:** Prototype uses solderless jumpers (*Mitigation:* Migrating to a custom two-layer PCB and 3D-printed enclosure for the final).
- **Phone Battery Dependency for Cloud Tracking:** If phone battery depletes, remote caregiver tracking pauses (*Mitigation:* Local obstacle avoidance and fall buzzer remain 100% operational offline on the cane).

## 8. Planned Work Before the Final (17 October 2026)
1. **Custom PCB & 3D-Printed Handle:** Fabricate a soldered 2-layer PCB and ergonomically balanced handle housing with TPU vibration dampening.
2. **End-to-End Caretaker Cloud Alerts:** Finalize Firebase Cloud Function integration to dispatch automated SMS and push notifications on fall confirmation.
3. **Power Management:** Integrate an 18650 Li-ion battery with TP4056 charge protection and enable ESP32-C3 Light Sleep for $>24\text{ hours}$ runtime.
