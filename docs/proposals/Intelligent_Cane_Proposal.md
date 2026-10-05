# IntelliCane: Project Proposal

**Neural-Nexus · Track A: Embedded IoT System Development · IoTrix 2.0**

**Team Members: Dulnith Liyanage, Thenul Sahansa, Chamith Chethana, Suneth Vidurasa · 5 October 2026**

## The Problem: Why We Built IntelliCane

Walking with a standard white cane works well for detecting steps and curbs, but visually impaired individuals still face major daily challenges with waist- and head-level obstacles (like tree branches, open car trunks, or wall-mounted signs). On top of that, an accidental cane drop or sudden fall can leave someone in danger without an easy way to call for help. 

Existing commercial smart canes usually cost over \$500 (LKR 150,000+), depend heavily on constant internet connectivity, or emit loud beeps that drown out crucial environmental sounds. As an undergraduate engineering team, we set out to build an accessible, intuitive, and genuinely affordable (LKR 2,800 bench electronics) smart cane that keeps the user safe offline while bridging emergency alerts to family and caretakers.

## What We Built: Solution & Scope

IntelliCane connects an embedded ESP32-C3 SuperMini on the cane with an accessible Android companion app:
- **Offline Obstacle Guidance:** An HC-SR04 ultrasonic sensor scans forward up to 4 m. It drives a handle vibration motor with 200 Hz PWM that starts gently at 60 cm and smoothly gets stronger as obstacles approach (down to 10 cm). A buzzer provides short audio beeps for very close obstacles (<30 cm).
- **Silent Fall Detection:** A GY-521 (MPU6050) IMU tracks tilt and sudden impacts. If a fall or drop is detected (tilt $>65^\circ$ or impact acceleration $>2.5\,g$), the cane enters a silent emergency state—turning off local vibration and buzzer noise so the fallen user isn't startled or disoriented—and immediately broadcasts high-priority `FALL_STATE:1` packets over Bluetooth Low Energy (BLE). Once the cane is picked up upright ($<30^\circ$), it sends `FALL_STATE:0` and automatically resumes normal navigation.

## System Architecture & IoT Mobile Companion

The cane's microcontroller handles safety reflexes locally in a sub-50 ms cooperative loop without any blocking delays:
- **Hardware Sensing:** HC-SR04 Echo connects directly to GPIO 1, and the MPU6050 runs over I2C (GPIO 4/5) with our custom direct-register fallback driver.
- **Companion Android App (`application/`):** Connects via BLE 5.0 Nordic UART Service. It provides a high-contrast **Blind User Dashboard** with one-tap location sharing, a **collapsible floating terminal** to inspect live sensor feeds, **Caretaker Mode** with an interactive OSMDroid/ESRI map with a dedicated centering FAB (18.5x zoom), and **Firebase Cloud Firestore synchronization with high-priority push notifications** (`showFallAlertNotification`) that trigger emergency sound and vibration on the caretaker's phone the moment a fall is detected. Using the phone's native GPS eliminates the cost, weight, and battery drain of a dedicated GPS module on the cane.

## Technology & Components

- **MCU:** ESP32-C3 SuperMini (RISC-V 160 MHz, BLE 5.0, USB-C) — **LKR 1,400**
- **Sensors:** HC-SR04 Ultrasonic (direct Echo to GPIO 1) — **LKR 450**; GY-521 MPU6050 6-Axis IMU — **LKR 600**
- **Actuators:** 3-pin vibration motor module (200 Hz PWM) — **LKR 250**; Piezo buzzer & wiring — **LKR 100**
- **Total Bench Electronics Cost:** **LKR 2,800** (well within our LKR 20,000–35,000 competition budget ceiling).
- **Software:** C++17, Arduino-ESP32, Kotlin Android, Firebase Firestore & RTDB, OSMDroid / ESRI maps, BLE Nordic UART Service.

## Testing What We Built

- **Automated Software Tests (`tests/proximity_feedback_test.cpp`):** 100% pass across boundary values, invalid inputs (negative, zero, NaN, $\infty$), smooth PWM ramping, motor polarity flips, and 32-bit `millis()` rollover resilience.
- **Bench Distance Checks:** HC-SR04 verified with physical measuring tape from 2 cm to 250 cm with $\pm 1.5$ cm accuracy. Motor vibration engages deterministically at 59.9 cm and reaches full power at 10 cm.
- **Tilt Verification:** Calibrated with a digital protractor; the fall trigger activates reliably at $>65^\circ$ and silences within 200 ms once the cane is upright ($<30^\circ$).
- **Wireless BLE Performance:** Telemetry stream validated at 10 Hz to smartphone with $<25$ ms latency over a 10 m range. Electronics stayed cool ($<32^\circ$C) during continuous vibration tests.

## Known Limitations & How We're Handling Them

- **Ultrasonic Reflections on Soft Clothes:** High-angle acoustic scatter can occur on soft fabrics beyond 2 m. *Fix:* We plan to integrate a compact Time-of-Flight (ToF) laser sensor as a secondary channel in our final build.
- **Breadboard Jumpers:** Prototype jumpers can loosen during heavy tapping. *Fix:* We are designing a custom soldered 2-layer PCB and a 3D-printed handle with TPU shock damping for the final competition.
- **Phone Battery Dependency:** If the phone battery dies, cloud tracking pauses. *Safety Reflex:* The cane's core obstacle sensing and local alerts stay 100% functional offline on the cane.

## Roadmap to the Final (17 October 2026)

1. **Custom PCB & 3D Handle:** Design a clean soldered PCB and 3D-print an ergonomic handle housing.
2. **Automated Emergency Cloud Alerts:** Finalize Firebase Cloud Functions to dispatch automated SMS and push notifications on verified fall events.
3. **Battery & Power Optimization:** Integrate an 18650 Li-ion battery with TP4056 charge controller and configure ESP32-C3 Light Sleep for all-day runtime.

## Deliverables

Current firmware, architecture and wiring guides, root one-page technical proposal (`ONE_PAGE_TECHNICAL_PROPOSAL.md` & `ONE_PAGE_TECHNICAL_PROPOSAL.pdf`), 2-page project proposal (`docs/Neural-Nexus.pdf`), native Android companion app (`application/`), software test suite, and a live 4-minute demonstration followed by 2 minutes progress check and 4 minutes Q&A.

- **Repository:** https://github.com/Neural-Nexus-IoTrix-2-0/intellicane
- **Submission Form:** https://forms.gle/cEg6tZt5X29xi27S9

This proposal summarizes the current scope. Check it against any previously registered proposal and any form-specific template before submission.
