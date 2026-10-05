# Bill of Materials (BOM) — Intelligent Cane Prototype

Track A Entry: Embedded IoT System Development  
Target Prototype Budget: **LKR 20,000 – 35,000**

---

## Phase 1: Core Sensing & Haptics Subsystem (Active Bench Build)

| Item | Component Description | Model / Spec | Qty | Unit Price (LKR) | Subtotal (LKR) | Sourcing / Notes |
|:---|:---|:---|:---:|:---:|:---:|:---|
| 1 | Microcontroller Board | **ESP32-C3 SuperMini** (RISC-V 160MHz, native USB-C, BLE 5.0) | 1 | 1,400 | 1,400 | Ultra-compact cane handle MCU with onboard BLE |
| 2 | Distance Sensor | **HC-SR04** Ultrasonic Sensor (2cm–400cm) | 1 | 450 | 450 | Forward obstacle ranging |
| 3 | 6-Axis Motion / Fall Sensor | **MPU6050** Accelerometer + Gyroscope (GY-521) | 1 | 600 | 600 | Hardware I2C orientation & fall tracking |
| 4 | Haptic Actuator Module | **3-Pin Vibration Motor Module** (Integrated Driver) | 1 | 250 | 250 | Tactile feedback with onboard driver |
| 5 | Audible Alarm & Wiring | Active / Passive 5V Buzzer + breadboard jumpers | 1 lot | 100 | 100 | Audio hazard alerts & breadboard hookup |
| 6 | Discrete Driver & Diode | 2N2222 / 1N4148 (Not needed with 3-pin module) | 0 | 0 | 0 | *Included directly on 3-pin module PCB* |
| 7 | Required Echo Divider | 1kΩ and 1.8kΩ resistors | 1 pair | TBD | TBD | Required for standard 5V HC-SR04; verify fitted before powering |
| 8 | Pushbutton | 6x6mm tactile momentary pushbutton | 0 | 0 | 0 | *Omitted in bench build (auto-reset on upright used)* |
| **Recorded Base Electronics Subtotal** | | | | | **Rs. 2,800** | Excludes unpriced divider; physical validation pending records |

### Optional Phase 1 Commercial Packaging & Power (Estimated)
| Item | Component Description | Model / Spec | Qty | Unit Price (LKR) | Subtotal (LKR) | Notes |
|:---|:---|:---|:---:|:---:|:---:|:---|
| 9 | Battery System | 18650 Li-ion Cell + Holder | 1 | 1,400 | 1,400 | Optional standalone portable power |
| 10 | Power Management | TP4056 USB-C Charger + Boost module | 1 set | 650 | 650 | 3.7V to 5V step-up & charge control |
| 11 | Cane Hardware & Mounting | Lightweight white cane / PVC shaft & clamp | 1 | 2,500 | 2,500 | Structural chassis |

---

## Phase 2 & 3 Additions (Estimated Future Costs)

| Item | Component Description | Model / Spec | Qty | Unit Price (LKR) | Subtotal (LKR) | Notes |
|:---|:---|:---|:---:|:---:|:---:|:---|
| 12 | Geolocation & Tracking | **Smartphone BLE Location Bridge** (Companion App) | 1 | **0** | **0** | Planned software; assumes an existing compatible phone. Power/location performance unmeasured. |
| 13 | Secondary Vision MCU | ESP32-CAM (with OV2640 2MP Camera) | 1 | 2,600 | 2,600 | Phase 3 Camera module |
| 14 | FTDI Programmer | FT232RL USB-to-UART (for ESP32-CAM) | 1 | 950 | 950 | Flashing tool for CAM module |
| 15 | 3D Printed Enclosure | PETG / PLA custom ergonomic handle | 1 | 3,500 | 3,500 | Enclosure & sensor hood |
| **Total Cumulative System (Full 3-Phase)** | | | | | **LKR 14,400 + unpriced items** | Sum of all listed priced rows; excludes phone and service costs |
