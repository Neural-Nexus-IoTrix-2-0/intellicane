# Intelligent Cane — Hardware Wiring & Interfacing Guide

This guide details the complete electrical connections, pin mapping, driver circuits, and power distribution for the Intelligent Cane prototype using the **ESP32-C3 SuperMini** microcontroller.

---

## 1. System Pinout Table (ESP32-C3 SuperMini)

| Component | Pin Function | ESP32-C3 Pin | Logic Level | Operating Voltage | Notes |
|:---|:---|:---:|:---:|:---:|:---|
| **HC-SR04 (Ultrasonic)** | `TRIG` | **GPIO 0** | 3.3V | 5V | 10 µs trigger pulse output |
| | `ECHO` | **GPIO 1** | 3.3V / 5V | 5V | **Direct connection** for bench prototype (omits divider) |
| | `VCC` | **5V Pin** | — | 5V | Power from 5V rail / USB VIN |
| | `GND` | **GND** | — | 0V | Common ground |
| **MPU6050 (6-Axis IMU)** | `SDA` | **GPIO 4** | 3.3V | 3.3V | Hardware I2C Data line |
| | `SCL` | **GPIO 5** | 3.3V | 3.3V | Hardware I2C Clock line |
| | `AD0` | **GND** | 0V | — | Sets I2C address to `0x68` |
| | `VCC` | **3V3 Pin** | — | 3.3V | Power from 3.3V rail |
| | `GND` | **GND** | — | 0V | Common ground |
| **Haptic Vibration Motor** | `(+) / Red` | **GPIO 6** | 3.3V PWM | 3.3V | **Direct GPIO drive** for bench prototype (omits transistor) |
| | `(-) / Blue` | **GND** | — | 0V | Common ground |
| **Audible Alarm (Buzzer)** | `(+) / SIG` | **GPIO 7** | 3.3V | 3.3V / 5V | Supports both Active & Passive buzzers |
| | `(-) / GND` | **GND** | — | 0V | Common ground |
| **Push Button (SOS / Reset)** | `SWITCH` | **GPIO 3** | 3.3V | — | **Omitted in bench build** (Optional / unpopulated; alarm auto-resets when upright) |
| | `GND` | **GND** | — | 0V | Common ground (when button is populated) |
| **Status LED** | Onboard Blue | **GPIO 8** | 3.3V | 3.3V | Built-in on SuperMini PCB (**Active LOW**) |

---

## 2. Driver & Interfacing Diagrams

### 2.1 Haptic Vibration Motor Circuit

#### Current Bench Prototype Setup (Direct GPIO Connection)
Due to local component availability constraints, the vibration motor is currently connected directly between **GPIO 6** and **GND**:

```
ESP32-C3 GPIO 6 (LEDC PWM) ─────[ (+) Vibration Motor (-) ]───── GND
```
- **Driver**: Modulated via ESP32-C3 LEDC PWM at **200 Hz** (optimized for DC brushed coin vibration motors).
- **Bench Note**: The motor runs directly from the GPIO pin current. For the current bench prototype, this simplifies assembly without needing external discrete components.
- **Production Recommendation**: In the final revision or custom PCB, adding an NPN transistor (2N2222) or N-channel MOSFET with a 1N4148 flyback diode allows full current delivery (60–120 mA) and isolates the microcontroller from inductive transients.

#### Recommended Production Driver Circuit (Reference)
```
              +3.3V or +5V (VCC_MOTOR)
                     │
                     ├──────────────┐
                     │              │
                   [Motor]        [D1: 1N4148 / 1N4001]
                     │            (Cathode to +V, Anode to Drain/Collector)
                     ├──────────────┘  (Protects against inductive back-EMF)
                     │
                 ┌───┴───┐
                 │ C / D │
ESP32-C3 GPIO 6 ─┤ R1    │  Q1: 2N2222 (NPN BJT) or 2N7000 (N-MOSFET)
(LEDC PWM)   ───┤ B / G │
                 │ E / S │
                 └───┬───┘
                     │
                    GND (Common)

Component Values:
- R1: 1 kΩ (for BJT base) or 220 Ω (for MOSFET gate with 100kΩ pull-down to GND)
- D1: 1N4148 or 1N5819 Schottky flyback diode
- Q1: 2N2222 NPN transistor or 2N7000 / AO3400 N-channel MOSFET
```

---

### 2.2 Ultrasonic Sensor Interfacing (HC-SR04)

#### Current Bench Prototype Setup (Direct Echo Connection)
For the current bench setup, the HC-SR04 is wired directly to the ESP32-C3 without an external resistor divider:

```
HC-SR04 TRIG Pin ────────────────────────── ESP32-C3 GPIO 0 (Output, 3.3V)
HC-SR04 ECHO Pin ────────────────────────── ESP32-C3 GPIO 1 (Input, Direct)
HC-SR04 VCC Pin  ────────────────────────── 5V Pin (USB VIN)
HC-SR04 GND Pin  ────────────────────────── GND
```
- **Bench Note**: The ESP32-C3 GPIO 1 pin receives the Echo return pulse directly.
- **Production Recommendation**: For permanent deployment, a two-resistor voltage divider (1 kΩ / 2 kΩ) or a native 3.3V sensor (e.g., RCWL-1601 / US-015) can be used to scale the 5V return signal to 3.3V.

#### Recommended Production Voltage Divider (Reference)
```
HC-SR04 ECHO Pin (5V Pulse)
          │
        [ 1 kΩ ] (R_TOP)
          │
          ├─────────────────── ESP32-C3 GPIO 1 (Input) (~3.3V)
          │
        [ 2 kΩ ] (R_BOTTOM)
          │
         GND
```
*Note: If using RCWL-1601 or US-015 powered directly at 3.3V, the voltage divider is not required.*

---

### 2.3 Piezo Buzzer Connection

- Connect the positive (+) pin of the buzzer to **GPIO 7**.
- Connect the negative (-) pin to **GND**.
- The firmware uses a universal digital driver that triggers active buzzers at full resonant volume and toggles passive buzzers with audible tones.

---

## 3. Power Architecture

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
                  │ MT3608 Boost Conv.   │
                  │ Step-up to 5.0V VCC  │
                  └──────────┬───────────┘
                             │
            ┌────────────────┴────────────────┐
            ▼                                 ▼
   ┌──────────────────────┐          ┌───────────────────┐
   │ ESP32-C3 SuperMini   │          │ 5V Rail           │
   │ 5V Pin (5V in)       │          │ - HC-SR04 VCC     │
   └──────────┬───────────┘          └───────────────────┘
              │
              ▼ 3.3V Onboard LDO
   ┌─────────────────────────────────┐
   │ 3.3V Rail (SuperMini 3V3 Pin)   │
   │ - MPU6050 IMU                   │
   │ - Vibration Motor Drive Rail    │
   └─────────────────────────────────┘
```
