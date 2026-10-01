# Phase 1: Core Sensing Subsystem (`core-sensing`)

This PlatformIO project contains the safety-critical firmware for the Intelligent Cane prototype. It runs on an ESP32 microcontroller with **zero cloud or network dependency**, guaranteeing low-latency hazard detection, ground drop-off protection, and intuitive tactile and audible user warnings.

---

## Hardware Interfacing

| Subsystem | Sensor / Actuator | ESP32 GPIO | Bus / Protocol |
|---|---|---|---|
| Forward Ranging | Pololu VL53L1X Time-of-Flight (up to 4m) | SDA=21, SCL=22 | I2C (0x29 @ 400kHz) |
| Motion / Orientation | Adafruit MPU6050 6-Axis IMU | SDA=21, SCL=22 | I2C (0x68 @ 400kHz) |
| Downward Ground | Ultrasonic (HC-SR04/US-015) | TRIG=18, ECHO=5 | Digital Pulse (5V->3.3V divider) |
| Downward Ground (Alt) | VL53L0X Time-of-Flight | SDA=21, SCL=22, XSHUT=19 | I2C (0x30 @ 400kHz) |
| Tactile Haptic | ERM Vibration Motor | GPIO 25 | LEDC PWM (5kHz, 8-bit) via Transistor |
| Audio Alert | Piezo Buzzer | GPIO 26 | Digital / Tone Alarm |
| Diagnostics LED | Onboard Blue LED | GPIO 2 | Digital Output |
| SOS / Reset Button | Tactile Pushbutton | GPIO 27 | INPUT_PULLUP (Active LOW) |

---

## Safety Logic & Feedback Signatures

1. **Forward Obstacle Distance Proximity**:
   - `> 150 cm`: Vibration motor **OFF** (Clear path).
   - `80 cm – 150 cm`: Gentle proportional haptic vibration (PWM 30%–60%).
   - `30 cm – 80 cm`: Strong proportional haptic vibration (PWM 60%–95%).
   - `< 30 cm`: **100% Full Vibration + Urgent Buzzer Beeps** (Critical hazard).

2. **Ground Drop-off Detection (Descending stairs, curbs, open drains)**:
   - Downward sensor continuously checks ground distance against calibrated baseline.
   - When distance spikes by $+20\text{ cm}$ above baseline (or beam reflection is lost over a step down):
     - Overrides forward vibration with a distinctive **double-burst tactile signature**:
       `150ms ON` $\rightarrow$ `80ms OFF` $\rightarrow$ `150ms ON` $\rightarrow$ `250ms OFF`.
     - The user immediately feels the difference between an obstacle ahead and a step down below.

3. **Fall Detection (MPU6050 Multi-Stage)**:
   - Detects freefall ($<0.4\text{ g}$) followed by impact ($>2.5\text{ g}$) and subsequent immobility with cane horizontal ($>70^\circ$ tilt) for $>3\text{ seconds}$.
   - Triggers continuous emergency alarm and alert pulsing until user recovers cane or presses SOS button.

---

## Build & Flash Instructions

### Prerequisites
- [PlatformIO Core](https://platformio.org/install/cli) installed (`pio`).

### Compile Firmware
```bash
cd firmware/core-sensing
pio run
```

### Upload to ESP32
Connect your ESP32 board via USB, then:
```bash
pio run --target upload
```

### Monitor Serial Output
```bash
pio device monitor -b 115200
```

### Telemetry Stream Format
```
[TELEM] Fwd:  85.2 cm | Down:  39.5 cm (Base: 40.0) | Tilt: 12.3 deg | G: 0.99 | Haptic: 142 PWM (PROP) | Buzz: 0
```
