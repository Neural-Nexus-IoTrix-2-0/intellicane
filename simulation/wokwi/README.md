# Intelligent Cane — Wokwi Simulation Setup Guide

This folder contains a complete, self-contained Wokwi simulation for the Intelligent Cane prototype matching the exact components on your Wokwi canvas.

---

## 1. Pin Connections Overview

| Component | Component Pin | ESP32 Board Pin | Wire Color in Diagram | Description |
|:---|:---|:---|:---:|:---|
| **HC-SR04** (Ultrasonic) | `VCC` | `5V` | Red | 5V Power rail |
| | `GND` | `GND.1` | Black | Ground |
| | `TRIG` | `GPIO 18` | Blue | Ultrasonic trigger pulse |
| | `ECHO` | `GPIO 5` | Cyan | Ultrasonic echo timing pulse |
| **MPU6050** (6-Axis IMU) | `VCC` | `3V3` | Red | 3.3V Power rail |
| | `GND` | `GND.1` | Black | Ground |
| | `SCL` | `GPIO 22` | Gold | I2C Clock line |
| | `SDA` | `GPIO 21` | Orange | I2C Data line |
| **Piezo Buzzer** | `1` (-) | `GND.2` | Black | Ground |
| | `2` (+) | `GPIO 26` | Purple | Audio alarm / tone generator |
| **Pushbutton** (Green) | `1.l` | `GND.1` | Black | Ground |
| | `2.r` | `GPIO 27` | Green | SOS / Alert reset (`INPUT_PULLUP`) |
| **IR Receiver** (3-Pin) | `VCC` | `3V3` | Red | 3.3V Power rail |
| | `GND` | `GND.2` | Black | Ground |
| | `DAT` | `GPIO 13` | Magenta | Digital demodulated IR signal |

---

## 2. Quick Setup in Wokwi (3 Easy Steps)

### Step 1: Add Libraries in Wokwi
1. Click the **"Library Manager"** tab in Wokwi (next to `diagram.json`).
2. Add the following 4 libraries:
   - `Adafruit MPU6050`
   - `Adafruit Unified Sensor`
   - `Adafruit BusIO`
   - `IRremote`

### Step 2: Paste the Circuit Diagram
1. Click the **`diagram.json`** tab in Wokwi.
2. Select all and replace with the contents of [`diagram.json`](./diagram.json).
3. All wires will connect automatically to the exact pins!

### Step 3: Paste the Code
1. Click the **`sketch.ino`** tab in Wokwi.
2. Select all and replace with the contents of [`sketch.ino`](./sketch.ino).
3. Click the green **Play** (Simulation) button!

---

## 3. How to Test Each Feature in Wokwi

1. **Obstacle Proximity (HC-SR04)**:
   - Click on the **HC-SR04** during simulation.
   - Drag the distance slider:
     - `> 150 cm`: Safe path (clear).
     - `25 cm – 60 cm`: Warning zone (buzzer emits warning chirps).
     - `< 15 cm`: **CRITICAL HAZARD** (continuous urgent buzzer siren + rapid onboard LED flashing).

2. **Fall Detection (MPU6050)**:
   - Click on the blue **MPU6050** board during simulation.
   - Change the tilt/acceleration sliders (e.g., tilt angle $> 65^\circ$).
   - A **FALL ALARM!** alternating emergency siren triggers on the buzzer!

3. **Remote Control & "Find My Cane" (IR Receiver)**:
   - Click the **IR Receiver** module during simulation.
   - A popup dialog will appear. Click **"Send"** to transmit an NEC IR packet.
   - You can also add an `IR Remote` to the canvas to press buttons.
   - What happens:
     - If an alarm is sounding: The IR signal immediately silences it.
     - Otherwise: Triggers the **"Find My Cane"** locator melody and toggles between **OUTDOOR** (150cm range) and **INDOOR** (80cm range) modes.

4. **SOS / Alarm Mute Button**:
   - Click the green **Pushbutton** to clear any active fall alarm or send an emergency acknowledgement.
