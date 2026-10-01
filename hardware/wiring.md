# Intelligent Cane — Hardware Wiring & Interfacing Guide

This guide details the complete electrical connections, pin mapping, driver circuits, and power distribution for Phase 1 of the Intelligent Cane prototype.

---

## 1. System Pinout Table (ESP32 DevKit V1 — 30 Pin)

| Component | Pin Function | ESP32 Pin | Logic Level | Operating Voltage | Notes |
|:---|:---|:---|:---|:---|:---|
| **VL53L1X (Forward ToF)** | `SDA` | GPIO 21 | 3.3V | 3.3V | Shared I2C bus (Default Address `0x29`) |
| | `SCL` | GPIO 22 | 3.3V | 3.3V | Shared I2C bus |
| | `VIN` | 3V3 Pin | — | 3.3V | Power from ESP32 on-board 3.3V rail |
| | `GND` | GND | — | 0V | Common ground |
| **MPU6050 (6-Axis IMU)** | `SDA` | GPIO 21 | 3.3V | 3.3V | Shared I2C bus (Address `0x68` with AD0→GND) |
| | `SCL` | GPIO 22 | 3.3V | 3.3V | Shared I2C bus |
| | `AD0` | GND | 0V | — | Sets I2C address to 0x68 |
| | `VCC` | 3V3 Pin | — | 3.3V | Power from 3.3V rail |
| | `GND` | GND | — | 0V | Common ground |
| **Downward Sensor (Option A: Ultrasonic)** | `TRIG` | GPIO 18 | 3.3V | 5V / 3.3V | 10 µs trigger pulse |
| | `ECHO` | GPIO 5 | 3.3V | 5V / 3.3V | **Important:** If using 5V HC-SR04, use voltage divider to GPIO 5! |
| | `VCC` | 5V (VIN) | — | 5V | 5V rail (or 3.3V for US-015 / RCWL-1601) |
| | `GND` | GND | — | 0V | Common ground |
| **Downward Sensor (Option B: VL53L0X ToF)** | `SDA` | GPIO 21 | 3.3V | 3.3V | Shared I2C bus (address reassigned to `0x30` via XSHUT) |
| | `SCL` | GPIO 22 | 3.3V | 3.3V | Shared I2C bus |
| | `XSHUT` | GPIO 19 | 3.3V | 3.3V | Active-low shutdown pin for I2C readdressing |
| | `VCC` | 3V3 Pin | — | 3.3V | Power from 3.3V rail |
| | `GND` | GND | — | 0V | Common ground |
| **Haptic Vibration Motor** | `GATE/BASE` | GPIO 25 | 3.3V | — | LEDC PWM output to transistor driver circuit |
| **Audible Alarm (Piezo Buzzer)** | `SIG` | GPIO 26 | 3.3V | 3.3V / 5V | Tone/PWM or active alarm driver |
| | `GND` | GND | — | 0V | Common ground |
| **Status LED** | `ANODE` | GPIO 2 | 3.3V | 3.3V | Built-in ESP32 LED (or external via 220Ω resistor) |
| **SOS / Emergency Button** | `SWITCH` | GPIO 27 | 3.3V | — | Pushbutton to GND with internal `INPUT_PULLUP` |

---

## 2. Driver Circuit Diagrams

### 2.1 Haptic Vibration Motor Driver Circuit

Vibration disc motors (coin type, e.g. 1027 or 1034) typically draw **60 mA to 120 mA** at 3V–3.7V. An ESP32 GPIO pin can only safely supply **12 mA (max 40 mA peak)**. 
**Never connect the vibration motor directly to an ESP32 GPIO pin.**

Use an NPN BJT (2N2222 / BC547) or an N-channel logic-level MOSFET (2N7000 / AO3400 / IRLML2502):

```
              +3.3V or +5V (VCC_MOTOR)
                     │
                     ├──────────────┐
                     │              │
                   [Motor]        [D1: 1N4148 / 1N4001]
                     │            (Cathode to +V, Anode to Drain/Collector)
                     ├──────────────┘  (Flyback diode protects against inductive spikes)
                     │
                 ┌───┴───┐
                 │ C / D │
ESP32 GPIO 25 ───┤ R1    │  Q1: 2N2222 (BJT) or 2N7000 (N-MOSFET)
(LEDC PWM)   ───┤ B / G │
                 │ E / S │
                 └───┬───┘
                     │
                    GND (Common)

Component Values:
- R1: 1 kΩ (for BJT base) or 220 Ω (for MOSFET gate with 100kΩ pull-down to GND)
- D1: 1N4148 or 1N5819 Schottky diode
- Q1: 2N2222 NPN transistor or AO3400 / 2N7000 N-channel MOSFET
```

---

### 2.2 Ultrasonic 5V to 3.3V Voltage Divider (HC-SR04)

If using a classic 5V HC-SR04, the `ECHO` pin outputs 5V logic pulses. Connecting 5V directly to ESP32 GPIO 5 can damage the ESP32 pin.
Use a simple two-resistor voltage divider:

```
HC-SR04 ECHO Pin (5V Pulse)
          │
        [ 1 kΩ ] (R_TOP)
          │
          ├─────────────────── ESP32 GPIO 5 (Input) (~3.3V)
          │
        [ 2 kΩ ] (R_BOTTOM)
          │
         GND
```
*Note: If using RCWL-1601 or US-015 powered at 3.3V, the voltage divider is not required.*

---

### 2.3 Piezo Buzzer Connection

- **Passive Buzzer**: Connect (+) to GPIO 26 through a 100 Ω current-limiting resistor, and (-) to GND. The ESP32 produces variable frequencies via `ledcWriteTone` or `tone()`.
- **Active Buzzer (Loud)**: Can be switched using a small 2N2222 transistor or connected via GPIO 26 with a 220 Ω resistor.

---

## 3. Power Distribution Architecture

```
                  ┌──────────────────────┐
                  │ 3.7V Li-Ion / 18650  │
                  │ Battery (2200-3000mA)│
                  └──────────┬───────────┘
                             │
                             ▼
                  ┌──────────────────────┐
                  │ TP4056 USB-C Charger │
                  │ + Protection Circuit │
                  └──────────┬───────────┘
                             │
                             ▼
                  ┌──────────────────────┐
                  │ MT3608 / Boost Conv. │
                  │ Step-up to 5.0V VCC  │
                  └──────────┬───────────┘
                             │
            ┌────────────────┴────────────────┐
            ▼                                 ▼
   ┌─────────────────┐               ┌─────────────────┐
   │ ESP32 DevKit    │               │ 5V Rail         │
   │ VIN Pin (5V in) │               │ - Ultrasonic VCC│
   └────────┬────────┘               │ - Buzzer        │
            │                        └─────────────────┘
            ▼ 3.3V Internal LDO
   ┌─────────────────────────────────┐
   │ 3.3V Rail (ESP32 3V3 Pin)       │
   │ - VL53L1X ToF (3.3V)            │
   │ - MPU6050 IMU (3.3V)            │
   │ - Vibration Motor Drive Rail    │
   └─────────────────────────────────┘
```

---

## 4. Mechanical Mounting & Sensor Orientation

1. **Forward ToF (VL53L1X)**:
   - Mounted facing directly forward ($0^\circ$ parallel to cane shaft or tilted slightly up $+5^\circ$ to cover chest/head clearance).
   - Conical Field of View: $27^\circ$. Ranging distance: up to 4 meters.
2. **Downward Sensor (Ultrasonic or VL53L0X)**:
   - Mounted 30–50 cm up the shaft, angled **$45^\circ$ to $55^\circ$ downward**.
   - Under normal level walking, ground distance reading is stable (e.g. 35–45 cm).
   - When reaching a descending staircase, open manhole, or curb, distance abruptly spikes ($> 60\text{ cm}$) or registers out-of-range, triggering the drop-off tactile signature.
3. **MPU6050 (Orientation & Fall Detection)**:
   - Mounted rigidly along the cane shaft inside the handle enclosure.
   - Detects cane angle: if cane falls flat on the floor ($>70^\circ$ tilt) or suffers high impact followed by immobility, a fall condition is flagged.
