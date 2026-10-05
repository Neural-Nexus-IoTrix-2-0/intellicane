# IntelliCane — One-Page Technical Proposal

**Project Title:** IntelliCane: Smart Assistive Cane with Haptic Obstacle Avoidance, Fall Detection, and a Companion App for Blind Users and Caretakers  
**Competition & Track:** IoTrix 2.0 — Track A: Embedded IoT System Development  
**Team Name:** Neural-Nexus | **Members:** Dulnith Liyanage, Thenul Sahansa, Chamith Chethana, Suneth Vidurasa  
**Repository:** [https://github.com/Neural-Nexus-IoTrix-2-0/intellicane](https://github.com/Neural-Nexus-IoTrix-2-0/intellicane)  
**Submission Date:** 5 October 2026 | **Bench Prototype Cost:** LKR 2,800  

---

## 1. The Problem: Why We Built IntelliCane
Navigating everyday environments with a standard white cane is tough. Visually impaired individuals frequently bump into knee-to-head level hazards (like open windows, low tree branches, or protruding table edges) and risk serious injury if they drop their cane or take a fall. Existing commercial smart canes attempt to fix this, but they often cost over \$500 (LKR 150,000+), depend entirely on continuous cloud connections, or blast loud beeps that drown out traffic and ambient sound. As undergraduate students, our goal was to build an accessible, highly practical, and genuinely affordable (LKR 2,800) smart cane that keeps users safe offline while connecting them with family and caretakers.

## 2. Our Proposed Solution
**IntelliCane** pairs an instant offline hardware reflex on the cane with an accessible Android companion app (`com.example.intellicane`):
- **Offline Safety Reflex (ESP32-C3):** Runs completely on the cane with a sub-50 ms loop. An HC-SR04 ultrasonic sensor scans ahead up to 4 m, driving a handle vibration motor via 200 Hz PWM that starts gently at 60 cm and smoothly intensifies as obstacles get closer (down to 10 cm). An MPU6050 IMU detects sudden drops and falls ($>65^\circ$ tilt or $>2.5\,g$ impact). When a fall happens, the cane stays quiet locally (no loud buzzing or alarm to panic the fallen user) and immediately sends emergency alert packets over BLE.
- **Companion Android App (`application/`):** Connects over BLE 5.0 (Nordic UART Service) and provides two dedicated modes:
  - **Blind User Mode:** Clean, accessible dashboard showing connection status, one-tap location sharing, and a collapsible floating terminal to view live sensor telemetry during debugging.
  - **Caretaker Mode:** Real-time presence tracking, automatic cloud fall alerts synced through Firebase Firestore, and an interactive OSMDroid/ESRI map with an instant FAB that centers on the user with 18.5x zoom using the phone's native GPS.

## 3. System Architecture
```
[SENSING LAYER]              [CORE REFLEX (CANE)]              [TACTILE & AUDIO FEEDBACK]
HC-SR04 (Trig: 0, Echo: 1) -> ESP32-C3 SuperMini (160 MHz) -> 200 Hz LEDC PWM Motor (GPIO 6)
MPU6050 (I2C: SDA 4, SCL 5)-> Non-blocking Cooperative Loop -> Proximity Audio Warning (GPIO 7)
                                       |                       (Silent on Cane during Fall)
                                       v  BLE 5.0 Nordic UART Service (RX: 6E400002 / TX: 6E400003)
[MOBILE APPLICATION]         [LOCATION & CLOUD]                [CARETAKER VIEW]
Android Kotlin App        -> Smartphone A-GPS Geolocation   -> Real-Time OSMDroid/ESRI Map
(User / Caretaker Mode)   -> Firebase Firestore & Presence  -> Emergency Fall State Sync & Push
```

## 4. What We've Built & Verified So Far
- **Working Hardware Bench Prototype:** Built on an ESP32-C3 SuperMini with HC-SR04 Echo wired directly to GPIO 1, GY-521 MPU6050, 3-pin vibration motor module, and buzzer.
- **Smooth Proportional Haptics:** 200 Hz PWM vibration scales smoothly from 60 cm down to 10 cm, staying completely off past 60 cm so the user's hand doesn't tire out.
- **Silent Fall Detection over BLE:** Tilting past $65^\circ$ or impact $>2.5\,g$ silences the local motor and buzzer, immediately transmitting `FALL_STATE:1` and `[ALERT: FALL DETECTED!]` over BLE; standing the cane back up ($<30^\circ$) sends `FALL_STATE:0` and restores normal navigation.
- **Resilient I2C Sensor Driver:** Custom direct-register fallback driver handles clone MPU6050/6500 chips, multiple I2C addresses (`0x68`/`0x69`), and recovers automatically from bus lockups.
- **Live Android Mobile App:** Native Kotlin app (`com.example.intellicane`) with User Dashboard, collapsible floating terminal, Firestore cloud fall sync (`updateFallStateInFirestore`), and live map with user-centering FAB (`fabUserLocation`).

## 5. Components & Budget Breakdown (Phase 1 Bench: LKR 2,800)
- **Hardware Parts:** ESP32-C3 SuperMini (LKR 1,400), HC-SR04 Ultrasonic (LKR 450), GY-521 MPU6050 IMU (LKR 600), 3-Pin Vibration Motor (LKR 250), Piezo Buzzer & Jumpers (LKR 100). **Total: LKR 2,800** (well within our LKR 20,000–35,000 project budget ceiling).
- **Software Stack:** C++17, Arduino-ESP32, Kotlin Android, Firebase Firestore & RTDB, OSMDroid / ESRI, BLE 5.0 Nordic UART Service.

## 6. Testing What We Built
- **Host Unit Tests (`tests/proximity_feedback_test.cpp`):** 100% pass across boundary inputs (negative, NaN, $\infty$), smooth PWM ramping, motor polarity flips, beep transitions, and 32-bit `millis()` rollover safety.
- **Bench Calibration:** Ultrasonic sensor verified with physical tape measurements from 2 cm to 250 cm with $\pm 1.5\text{ cm}$ accuracy. Fall trigger verified with a digital protractor ($>65^\circ$ trigger, $<30^\circ$ clear).
- **Wireless BLE Performance:** Stable 10 Hz telemetry stream to smartphone with $<25\text{ ms}$ latency over a 10 m range. Electronics stayed cool ($<32^\circ\text{C}$) during continuous vibration testing.

## 7. Known Limitations & How We're Handling Them
- **Ultrasonic Reflections on Soft Clothes:** Sound waves can scatter on angled soft fabrics beyond 2 m (*Fix:* Adding a compact Time-of-Flight laser sensor for the final build).
- **Breadboard Jumpers:** Prototype uses jumpers that can loosen with heavy motion (*Fix:* Designing a custom 2-layer PCB and 3D-printed handle before the final).
- **Phone Battery Dependency for Cloud:** If the phone dies, cloud syncing pauses (*Safety Reflex:* The cane's core obstacle sensing remains 100% functional offline).

## 8. Roadmap to the Final (17 October 2026)
1. **Custom PCB & 3D Handle:** Design a soldered PCB and 3D-print an ergonomic handle housing with TPU shock damping.
2. **Automated Caretaker Alerts:** Wire up Firebase Cloud Functions to dispatch automated SMS and push notifications on confirmed fall events.
3. **Battery Optimization:** Integrate an 18650 Li-ion battery with TP4056 charge controller and implement ESP32-C3 Light Sleep for all-day runtime.
