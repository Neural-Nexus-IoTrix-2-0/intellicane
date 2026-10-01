# Bill of Materials (BOM) — Intelligent Cane Prototype

Track A Entry: Embedded IoT System Development  
Target Prototype Budget: **LKR 20,000 – 35,000**

---

## Phase 1: Core Obstacle & Drop-off Detection (Current Phase)

| Item | Component Description | Model / Spec | Qty | Unit Price (LKR) | Subtotal (LKR) | Sourcing / Notes |
|:---|:---|:---|:---:|:---:|:---:|:---|
| 1 | Microcontroller Board | ESP32 DevKit V1 (30-pin, CP2102/CH340) | 1 | 2,200 | 2,200 | Local electronics / Daraz / LankaTronix |
| 2 | Forward Distance Sensor | VL53L1X Time-of-Flight (4m range) | 1 | 2,800 | 2,800 | I2C Laser ranging sensor |
| 3 | Downward Sensor (Option A) | HC-SR04 or US-015 Ultrasonic Sensor | 1 | 650 | 650 | Wide angle ground detection |
| 4 | 6-Axis Motion / Fall Sensor | MPU6050 Accelerometer + Gyroscope | 1 | 950 | 950 | I2C orientation & fall tracking |
| 5 | Haptic Actuator | Coin / Cylinder Vibration Motor (3V) | 2 | 250 | 500 | 1 in handle grip, 1 spare |
| 6 | Audible Alarm | Passive / Active 5V Piezo Buzzer | 1 | 150 | 150 | Urgent alarm |
| 7 | Motor Driver & Protection | 2N2222 NPN BJT + 1N4148 Diode + Resistors | 1 kit | 250 | 250 | Driver circuit for vibration motor |
| 8 | Battery System | 18650 Li-ion Cell (2500mAh) + Holder | 1 | 1,400 | 1,400 | Rechargeable power source |
| 9 | Power Management | TP4056 USB-C Charger + MT3608 Boost module | 1 set | 650 | 650 | 3.7V to 5V step-up & charge control |
| 10 | Cane Hardware & Mounting | Lightweight white cane / PVC shaft & clamp | 1 | 2,500 | 2,500 | Structural chassis |
| 11 | Passive Components & Wire | Jumper wires, toggle switch, perfboard | 1 lot | 1,200 | 1,200 | Miscellaneous wiring |
| **Phase 1 Total** | | | | | **~LKR 13,250** | *Well within budget ceiling* |

---

## Phase 2 & 3 Additions (Estimated Future Costs)

| Item | Component Description | Model / Spec | Qty | Unit Price (LKR) | Subtotal (LKR) | Notes |
|:---|:---|:---|:---:|:---:|:---:|:---|
| 12 | GPS Telemetry Module | u-blox NEO-6M / NEO-8M with patch antenna | 1 | 2,800 | 2,800 | Phase 2 GPS tracking |
| 13 | Secondary Vision MCU | ESP32-CAM (with OV2640 2MP Camera) | 1 | 2,600 | 2,600 | Phase 3 Camera module |
| 14 | FTDI Programmer | FT232RL USB-to-UART (for ESP32-CAM) | 1 | 950 | 950 | Flashing tool for CAM module |
| 15 | 3D Printed Enclosure | PETG / PLA custom ergonomic handle | 1 | 3,500 | 3,500 | Enclosure & sensor hood |
| **Total Cumulative System** | | | | | **~LKR 23,100** | **Target range: 20,000–35,000 LKR (Achieved)** |
