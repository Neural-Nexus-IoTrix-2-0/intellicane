# IoTrix 2.0 Semi-Final: 12-Minute Presentation & Technical Defense Guide

**Target Track:** Track A — Embedded IoT System  
**Presentation Time Limit:** 12 minutes maximum  
**Document Purpose:** Complete speaking guide, live demo checklist, and model answers for the judges' evaluation rubric.

---

## 1. Semi-Final 12-Minute Presentation Schedule

| Time Window | Section | Judge SOP Step | Key Focus & Message |
|:---|:---|:---|:---|
| **0:00 – 1:30** (1.5 min) | **Problem & Target User** | Step 1: Problem Verification | Visually impaired navigation hazards (falls, drop-offs, head-height obstacles). The fatal flaws of existing solutions: $>\$500$ USD, heavy cloud dependence, rapid battery drain, audio masking. |
| **1:30 – 3:30** (2.0 min) | **System Architecture & Design** | Step 2: Architecture Review | Decoupled 3-phase architecture. Subsystem 1: 100% offline safety reflex (ESP32-C3). Subsystem 2: BLE smartphone bridge for zero-added-cost GPS and caregiver telemetry (Rs. 2,800 bench cost). Subsystem 3: On-demand AI vision. |
| **3:30 – 7:30** (4.0 min) | **Live Demonstration** | Step 3 & Step 5: PoC & Track Test | **Demo 1:** Proportional haptic ramping as hand approaches ($60 \rightarrow 10\text{ cm}$).<br>**Demo 2:** High-urgency buzzer warning at $<30\text{ cm}$.<br>**Demo 3:** Cane tilt past $65^\circ \rightarrow$ fall alarm triggers $\rightarrow$ restored upright $\rightarrow$ auto-clears.<br>**Demo 4:** Wireless BLE Serial Monitor on smartphone showing live telemetry & peer MAC address. |
| **7:30 – 8:30** (1.0 min) | **Validation, Budget & Progress** | Evaluation: Working Progress | Rs. 2,800 actual expenditure vs LKR 20,000–35,000 budget ceiling. 100% host unit test coverage on proximity feedback math, monotonic PWM, and millis rollover. |
| **8:30 – 12:00** (3.5 min) | **Technical Defense & Q&A** | Step 4: Technical Questions | Direct, authoritative engineering answers to judges' questions. |

---

## 2. Track A Scoring Rubric Strategy (10 Marks Breakdown)

| Sub-Area | Max Marks | What Judges Look For | How We Win Full Marks |
|:---|:---:|:---|:---|
| **Hardware Integration** | **4** | Clean wiring, stable sensor/actuator operation, correct voltage domains, safe electrical design. | Show [`hardware/circuit_diagram.png`](../hardware/circuit_diagram.png). Point out the $1\text{k}\Omega / 1.8\text{k}\Omega$ echo voltage divider protecting the 3.3V GPIO, 5V regulated rail powering the vibration module and HC-SR04, common ground, and LEDC hardware PWM. |
| **Firmware Operation** | **3** | Non-blocking architecture, responsive control logic, fault tolerance, robust data handling. | Explain non-blocking cooperative scheduling (`millis()` based: 50ms ultrasonic, 20ms IMU, 10ms actuator, 100ms telemetry). Point out zero `delay()` in loop, I2C clone IC register driver, and BLE Nordic UART service. |
| **Physical Testing** | **3** | Ability to demonstrate real-time behavioral change under varied physical inputs. | Execute the 3 live test conditions on the real hardware in front of the judges (distance ramp, fall tilt, auto-recovery). |

---

## 3. Live Demonstration Script & Routine

### Setup Before Entering the Room:
1. Connect ESP32-C3 SuperMini via USB-C to laptop (powers the cane and opens Serial Monitor at 115200 baud).
2. Open **nRF Connect** or **Serial Bluetooth Terminal** on your smartphone.
3. Scan for BLE device named `Intelligent-Cane` and hit **Connect**.
4. In the BLE app, open the TX Characteristic (`6E400003-B5A3-F393-E0A9-E50E24DCCA9E`) and enable **Notify**.
5. Observe the live stream: `DIST: XX.Xcm | TILT: XX.Xdeg | FALL: 0 | MOTOR: XXX | BUZZ: X | BLE: CONN [...]`.

### Execution During the Pitch (3.5 Minutes):
- **Test 1: Proportional Ranging (Distance > 60 cm):**  
  *Speaker:* *"Notice the cane at rest. At distances greater than 60 cm, the vibration motor is completely silent to prevent sensory fatigue."*
- **Test 2: Smooth Proportional Tactile Warning (60 cm to 10 cm):**  
  *Speaker:* *"As an obstacle or wall approaches, watch the tactile motor. From 60 cm down to 10 cm, the ESP32-C3's 200 Hz LEDC PWM smoothly scales up vibration intensity, providing the user with natural spatial depth perception without noisy audio."*
- **Test 3: Critical Hazard Alarm (< 30 cm):**  
  *Speaker:* *"When an obstacle enters the critical 30 cm collision zone, the buzzer pulses urgently, alerting the user to stop immediately."*
- **Test 4: Fall Detection & Automatic Recovery:**  
  *Speaker:* *"If the cane is accidentally dropped or the user suffers a fall, the MPU6050 detects tilt exceeding 65 degrees. After a 1.5-second debounce, an emergency alarm sounds to alert bystanders. When the user or a passerby picks the cane back up (<30 degrees), the alarm automatically clears without requiring any manual button press."*
- **Test 5: Wireless BLE Telemetry Mirror:**  
  *Speaker:* *"Notice my smartphone screen. Over the Nordic UART Service on BLE 5.0, the cane streams real-time sensor metrics and the connected phone's MAC address with zero wires. This same link bridges into the phone's A-GPS for caregiver tracking in Phase 2."*

---

## 4. Model Answers to the 8 Recommended Technical Defense Questions

### Q1: Why did you select this architecture instead of a simpler alternative?
> **Answer:**  
> *"A simpler alternative would be a monolithic microcontroller trying to do obstacle sensing, GPS reading, cellular LTE communication, and AI processing all on one board. That approach has two fatal flaws: first, network latency or GPS acquisition delays block safety-critical reflexes; second, cellular modems and GPS modules inflate cost beyond LKR 15,000 and drain the battery in under 3 hours.  
> Our architecture decouples safety-critical reflex from network services. The ESP32-C3 runs a dedicated, deterministic offline loop guaranteeing <50 ms tactile response. High-level connectivity (GPS, cellular, cloud dashboard) is offloaded via BLE to the user's smartphone, cutting hardware cost to just Rs. 2,800 while ensuring the cane remains 100% safe even with zero phone battery or internet."*

### Q2: Why did you select this communication protocol (BLE Nordic UART Service)?
> **Answer:**  
> *"We evaluated Classic Bluetooth (SPP), Wi-Fi, and BLE. Wi-Fi draws over 120 mA continuously, which is prohibitive for a wearable battery. Classic Bluetooth SPP is unsupported natively on iOS without MFi licensing.  
> BLE 5.0 gives us an ultra-low power profile (<15 mA active radio), sub-25 ms latency, native cross-platform support across both Android and iOS, and standard Nordic UART Service (NUS) GATT characteristics. This allows effortless wireless telemetry streaming and seamless bidirectional command transfer."*

### Q3: What happens if the network connection is lost?
> **Answer:**  
> *"Nothing changes in terms of user safety. The cane's core functionality—ultrasonic obstacle detection, proportional haptic feedback, MPU6050 fall detection, and emergency local audio-visual alarms—is 100% self-contained on the ESP32-C3.  
> The network connection (via the smartphone) is only used for remote caregiver notifications and GPS tracking. If cell reception or Bluetooth drops, local safety reflexes continue uninterrupted."*

### Q4: What is the most important limitation of your current prototype/model?
> **Answer:**  
> *"The primary physical limitation is the ultrasonic beam reflection characteristic: sound waves can experience specular reflection or absorption against angled soft fabrics beyond 2 meters.  
> The primary mechanical limitation of the Phase 1 bench build is the use of breadboard jumpers, which are susceptible to vibration. In the final build, we are migrating to a custom two-layer PCB and adding a complementary forward Time-of-Flight (ToF) laser sensor for reflective redundancy."*

### Q5: How did you validate that your system behaves as expected?
> **Answer:**  
> *"We implemented a three-tier validation strategy:  
> 1. **Automated Host Unit Testing:** In `tests/proximity_feedback_test.cpp`, we tested edge cases including NaN, infinity, negative distances, monotonic PWM duty ramp, motor polarity inversion, and 32-bit `millis()` rollover resilience.  
> 2. **Physical Sensor Calibration:** Bench-tested the HC-SR04 against physical distance marks from 2 cm to 250 cm with $\pm 1.5\text{ cm}$ accuracy.  
> 3. **Digital Protractor Tilt Verification:** Calibrated MPU6050 pitch/roll thresholds under varying drop and pickup angles to eliminate false fall alarms."*

### Q6: Which component or subsystem is currently the highest technical risk?
> **Answer:**  
> *"In Phase 1, the highest risk was I2C bus lockup and clone MPU6050 IC compatibility, which we solved by writing a custom direct-register fallback driver with automatic bus recovery.  
> Looking forward to Phase 2 and the final competition, the highest technical risk is BLE connection stability in high-interference 2.4 GHz public environments and background app execution on iOS/Android. We mitigate this through automatic BLE advertising restarts, local circular buffer caching, and persistent background service foreground notifications."*

### Q7: If you had one additional month, what would you improve first?
> **Answer:**  
> *"We would prioritize three engineering upgrades:  
> 1. Fabricate a custom surface-mount PCB to eliminate all jumper wires.  
> 2. Complete the 3D-printed enclosure featuring an ergonomic handle that channels motor vibrations directly into the user's palm while isolating the IMU from hand tremors.  
> 3. Implement the native companion smartphone app that captures A-GPS coordinates upon receiving a BLE fall packet and automatically dispatches an SMS and dashboard alert to caregivers."*

### Q8: Which part of the system could fail in a real deployment, and how would you handle it?
> **Answer:**  
> *"In a real deployment, the three most likely failure modes are:  
> 1. **Sensor Occlusion or Mud/Water Splash on Transducers:** Handled by a firmware health watchdog that flags out-of-range acoustic echoes and alerts the user with a distinct diagnostic vibration pulse.  
> 2. **Low Battery Voltage:** Handled by ADC voltage divider monitoring on the 18650 cell, sounding a distinctive low-battery chirp when voltage drops below 3.3V.  
> 3. **Mechanical Impact on Drop:** Handled by mounting the electronics in an internal shock-damped TPU sleeve inside the rigid 3D-printed cane enclosure."*
