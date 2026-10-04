# Intelligent Cane — Technical Progress Report (1-Page Summary)

**Project:** Intelligent Cane (`intelligent-cane`)  
**Competition / Track:** IoTrix 2.0 — Track A: Embedded IoT System Development  
**Team:** Neural-Nexus | **Reporting Date:** October 2026 | **Current Status:** Phase 1 Bench Verified

---

## 1. Executive Summary & Objective

The **Intelligent Cane** is an offline-safe, multimodal mobility assistant engineered for visually impaired individuals. It integrates deterministic physical obstacle detection, ground drop-off protection, emergency fall detection, and an expansion path for smartphone-bridged BLE location tracking and AI scene narration. The design prioritizes **100% offline safety autonomy**, sub-50 ms safety-critical reflex time, low power consumption, and affordability.

---

## 2. Technical Progress & Completed Milestones (Phase 1)

All core sensing, actuation, and safety-critical embedded features for Phase 1 are fully designed, coded, physically bench-tested, and verified on real hardware:

* **Microcontroller Platform (ESP32-C3 SuperMini):**
  * Migrated to the stamp-sized ESP32-C3 (RISC-V single-core @ 160 MHz) with native USB-CDC and ultra-low quiescent current.
  * Configured dual-address I2C recovery (`0x68` / `0x69`) to guarantee plug-and-play compatibility with varying MPU6050 breakout boards.

* **Obstacle Proximity & Continuous Haptic Guidance:**
  * **HC-SR04 Ultrasonic Transceiver:** Calibrated for precise obstacle distance measurement from 2 cm to 400 cm.
  * **Hardware PWM Vibration:** 3-pin vibration motor module driven via 200 Hz LEDC timer (`GPIO 6`).
  * **Proximity Gating:** Continuous proportional vibration ramps smoothly from 60 cm down to 10 cm, avoiding false buzzing for distant objects ($> 60\text{ cm}$) while giving tactile urgency as obstacles approach.

* **Fall & Orientation Detection:**
  * **MPU6050 6-Axis IMU:** Continuously tracks pitch and roll angles (`SDA=GPIO 4, SCL=GPIO 5`).
  * **Automatic Recovery:** Triggers emergency audio/visual alert on sustained horizontal orientation ($> 65^\circ$), and automatically clears when the cane is restored upright ($< 30^\circ$).

* **Emergency Audio & Diagnostics:**
  * High-frequency Piezo buzzer warnings for critical hazard ($< 30\text{ cm}$) and fallen cane states.
  * Active-low LED status monitoring and startup actuator self-test on boot.

---

## 3. Hardware & Pinout Verification Matrix

| Subsystem | Component | MCU Pin | Electrical Configuration | Test Status |
|:---|:---|:---:|:---|:---:|
| **Ranging** | HC-SR04 Ultrasonic | `GPIO 0` (Trig), `GPIO 1` (Echo) | 5V VCC, 3.3V Trig / Direct Echo | **PASSED** (2–400 cm) |
| **Motion** | MPU6050 6-Axis IMU | `GPIO 4` (SDA), `GPIO 5` (SCL) | 5V VCC, I2C @ 100 kHz (`0x68`/`0x69`) | **PASSED** (Tilt/Fall) |
| **Haptics** | 3-Pin Vibration Motor | `GPIO 6` (Signal) | 5V VCC, 200 Hz LEDC PWM | **PASSED** (0–100% duty) |
| **Audio** | Piezo Buzzer | `GPIO 7` | Direct GPIO drive | **PASSED** (Pulsed alert) |
| **System** | Status Indicator | `GPIO 8` | Onboard blue LED (Active-LOW) | **PASSED** (Heartbeat) |

---

## 4. Software Quality & Architecture

* **Modular Abstraction:** Decoupled proximity feedback calculation into [`firmware/core-sensing/ProximityFeedback.h`](file:///Users/dulnithliyanage/Academics/IoTrix-2.0/intelligent-cane/firmware/core-sensing/ProximityFeedback.h) with dedicated host-runnable unit tests ([`tests/proximity_feedback_test.cpp`](file:///Users/dulnithliyanage/Academics/IoTrix-2.0/intelligent-cane/tests/proximity_feedback_test.cpp)).
* **Resilient I2C Bus Management:** Implemented automatic bus recovery and timeout safeguards to eliminate bus hang-ups.
* **Firmware Deployment:** Verified via Arduino CLI and PlatformIO toolchains.

---

## 5. Budget Status & Next Steps

* **Budget Tracking:** Actual Phase 1 bench prototype expenditure was only **Rs. 2,800 (LKR)** — achieving a remarkable cost reduction compared to typical commercial canes, well beneath the LKR 10,000 Phase 1 cap and the overall project budget ceiling of LKR 20,000–35,000.
* **Next Steps (Phase 2 & Phase 3 Roadmap):**
  1. **Phase 2 (Milestone 4 — Geolocation & Telemetry):** Utilize the active BLE link to bridge with the user's companion smartphone, acquiring real-time A-GPS coordinates and publishing telemetry/fall alerts to the caregiver web dashboard (eliminating the need, power, and cost of a standalone GPS hardware module).
  2. **Phase 3 (Milestone 5 — Visual AI):** Interface ESP32-CAM module with cloud/companion VLM service for on-demand scene description and text reading.
  3. **Mechanical Assembly:** 3D-print ergonomic cane handle and modular sensor enclosure.
