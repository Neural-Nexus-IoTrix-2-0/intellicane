# IntelliCane — One-Page Technical Proposal

**Project Title:** IntelliCane: Smart Assistive White Cane with Obstacle Avoidance, Fall Detection, and Companion Mobile App for Caretakers  
**Competition & Track:** IoTrix 2.0 — Track A: Embedded IoT System Development  
**Team Name:** Neural-Nexus | **Members:** Dulnith Liyanage, Thenul Sahansa, Chamith Chethana, Suneth Vidurasa  
**Repository:** [https://github.com/Neural-Nexus-IoTrix-2-0/intellicane](https://github.com/Neural-Nexus-IoTrix-2-0/intellicane)  
**Submission Date:** 5 October 2026 | **Bench Prototype Cost:** LKR 2,800  

---

## 1. The Problem: Why We Built IntelliCane
A traditional white cane is great for finding steps and curbs on the ground, but it cannot warn visually impaired people about waist- or head-level hazards—like open car doors, low tree branches, or protruding table corners. In addition, if a user trips and falls, they can easily get hurt and isolated without any way to alert family. Existing smart canes cost upwards of \$500 (LKR 150,000+), depend on a reliable internet connection, or blast annoying beeps that make it hard to hear surrounding traffic. As undergraduate students, we wanted to build something genuinely affordable, practical, and dependable that works offline while keeping caretakers in the loop.

## 2. How IntelliCane Works
IntelliCane is built in two simple, connected parts:
- **Smart Cane Hardware (ESP32-C3):** The cane works completely on its own without needing a phone or internet. An ultrasonic sensor scans ahead up to 4 m. A small motor in the handle vibrates gently when an obstacle is 60 cm away, and vibrates progressively stronger as you get closer (down to 10 cm). A motion sensor detects sudden falls or drops. During a fall, the cane stays quiet locally (no loud buzzing or alarm to panic the user) and instantly transmits an emergency alert over Bluetooth.
- **Companion Mobile App (`application/`):** Connects to the cane over Bluetooth Low Energy with two modes:
  - **Blind User Mode:** Clean high-contrast screen showing connection status, a 1-tap location sharing button, and a collapsible diagnostic terminal for testing.
  - **Caretaker Mode:** Shows the user's live status, displays their real-time location on an interactive map with a quick re-center button, and **sends an instant emergency push notification (with sound and vibration)** to the caretaker's phone the moment a fall occurs!

## 3. System Overview
```
[ON THE CANE]                [MICROCONTROLLER (ESP32-C3)]      [ALERTS FOR THE USER]
Ultrasonic Sensor (Obstacles)-> Fast offline loop (under 50 ms) -> Handle Vibration (Stronger closer)
Motion Sensor (Tilt & Falls) -> Fall detection logic           -> Buzzer (Warning tone; silent on fall)
                                         |
                                         v  Wireless Bluetooth Low Energy (BLE) Link
[COMPANION MOBILE APP]       [CLOUD SERVICES (FIREBASE)]       [CARETAKER PHONE]
Blind User Dashboard        -> Live phone GPS location        -> Real-time tracking map (1-tap centering)
Diagnostic Terminal         -> Automatic cloud sync           -> Instant emergency notification (Sound + Vibrate)
```

## 4. What We Have Built & Verified
- **Working Hardware Prototype:** Assembled and tested with the ESP32-C3 controller, ultrasonic sensor (Echo wired directly to GPIO 1), motion sensor, handle vibration motor, and buzzer for just LKR 2,800.
- **Intuitive Handle Vibration:** The handle vibrates smoothly between 60 cm and 10 cm. It stays off past 60 cm so the user's hand does not get tired.
- **Silent Fall Protection:** If the cane tilts past $65^\circ$ or takes a hard drop, it keeps quiet on the cane and immediately alerts the phone over Bluetooth. Standing the cane back up ($<30^\circ$) automatically clears the emergency and resumes normal guidance.
- **Caretaker App with Push Notifications:** Built natively in Kotlin with a live tracking map and automated emergency notifications. The instant a fall packet arrives, the caretaker's phone rings and vibrates with a high-priority alert.
- **Bench Testing:** Verified ultrasonic distance accuracy with a physical tape measure from 2 cm to 2.5 m ($\pm 1.5\text{ cm}$ accuracy). Tested fall detection with a digital protractor and ran software unit tests to ensure calculations never crash or freeze.

## 5. Components & Cost (Total: LKR 2,800)
- **Cane Electronics:** ESP32-C3 SuperMini (LKR 1,400), Ultrasonic Sensor (LKR 450), Motion Sensor (LKR 600), Vibration Motor (LKR 250), Buzzer & Jumpers (LKR 100). **Total: LKR 2,800** (comfortably within our competition budget ceiling of LKR 35,000).
- **Software Tools:** C++ firmware (Arduino-ESP32), Android app in Kotlin, Firebase cloud database, OpenStreetMap / ESRI live maps.

## 6. Known Limitations & Roadmap to the Final (17 October 2026)
- **Current Limitations & Solutions:** Soft angled clothing can sometimes scatter ultrasound beyond 2 m (we will add a compact laser sensor for the final). Breadboard jumper wires can loosen during heavy movement (we are making a custom soldered PCB and 3D-printed handle).
- **Next Steps for the Final:** (1) Build the custom soldered PCB and ergonomic handle, (2) Add automated SMS emergency alerts via cloud functions, and (3) Add a rechargeable 18650 battery with charge protection for all-day battery life.
