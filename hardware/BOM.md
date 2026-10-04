# Bill of Materials (BOM) — Intelligent Cane Prototype

Track A Entry: Embedded IoT System Development  
Target Prototype Budget: **LKR 20,000 – 35,000**

---

## Phase 1: Core Sensing & Haptics Subsystem (Active Build)

| Item | Component Description | Model / Spec | Qty | Unit Price (LKR) | Subtotal (LKR) | Sourcing / Notes |
|:---|:---|:---|:---:|:---:|:---:|:---|
| 1 | Microcontroller Board | **ESP32-C3 SuperMini** (RISC-V 160MHz, native USB-C) | 1 | 1,800 | 1,800 | Ultra-compact cane handle MCU |
| 2 | Distance Sensor | **HC-SR04** Ultrasonic Sensor (2cm–400cm) | 1 | 650 | 650 | Obstacle ranging |
| 3 | 6-Axis Motion / Fall Sensor | **MPU6050** Accelerometer + Gyroscope (GY-521) | 1 | 950 | 950 | I2C orientation & fall tracking |
| 4 | Haptic Actuator | Coin Vibration Motor (3V 1027/1034) | 1 | 250 | 250 | Proportional tactile feedback |
| 5 | Audible Alarm | Active / Passive 5V Piezo Buzzer | 1 | 150 | 150 | Critical close proximity & fall siren |
| 6 | Motor Driver & Diode (Production Rev) | 2N2222 NPN BJT + 1N4148 Diode + 1kΩ | 0 (1 opt) | 150 | 0 | *Omitted in bench build (direct GPIO 6 drive used)* |
| 7 | Voltage Divider Resistors (Production Rev)| 1kΩ and 2kΩ 1/4W Resistors | 0 (1 opt) | 50 | 0 | *Omitted in bench build (direct GPIO 1 Echo used)* |
| 8 | Pushbutton | 6x6mm tactile momentary pushbutton | 1 | 50 | 50 | SOS / Alarm reset |
| 9 | Battery System | 18650 Li-ion Cell (2500mAh) + Holder | 1 | 1,400 | 1,400 | Rechargeable power source |
| 10 | Power Management | TP4056 USB-C Charger + MT3608 Boost module | 1 set | 650 | 650 | 3.7V to 5V step-up & charge control |
| 11 | Cane Hardware & Mounting | Lightweight white cane / PVC shaft & clamp | 1 | 2,500 | 2,500 | Structural chassis |
| 12 | Passive Components & Wire | Jumper wires, perfboard | 1 lot | 1,000 | 1,000 | Miscellaneous wiring |
| **Phase 1 Total** | | | | | **~LKR 9,500** | *Well within budget ceiling (< LKR 10,000)* |

---

## Phase 2 & 3 Additions (Estimated Future Costs)

| Item | Component Description | Model / Spec | Qty | Unit Price (LKR) | Subtotal (LKR) | Notes |
|:---|:---|:---|:---:|:---:|:---:|:---|
| 13 | GPS Telemetry Module | u-blox NEO-6M / NEO-8M with patch antenna | 1 | 2,800 | 2,800 | Phase 2 GPS tracking |
| 14 | Secondary Vision MCU | ESP32-CAM (with OV2640 2MP Camera) | 1 | 2,600 | 2,600 | Phase 3 Camera module |
| 15 | FTDI Programmer | FT232RL USB-to-UART (for ESP32-CAM) | 1 | 950 | 950 | Flashing tool for CAM module |
| 16 | 3D Printed Enclosure | PETG / PLA custom ergonomic handle | 1 | 3,500 | 3,500 | Enclosure & sensor hood |
| **Total Cumulative System** | | | | | **~LKR 19,550** | **Target range: 20,000–35,000 LKR (Achieved)** |
