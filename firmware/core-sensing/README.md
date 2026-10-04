# Core Sensing Subsystem — ESP32-C3 SuperMini

This project contains the production firmware for the safety-critical obstacle detection and haptic feedback layer of the Intelligent Cane, targeting the ultra-compact **ESP32-C3 SuperMini** microcontroller.

---

## Hardware Pinout (ESP32-C3 SuperMini)

| Peripheral | Function | ESP32-C3 Pin | Logic Level | Electrical Notes |
|:---|:---|:---:|:---:|:---|
| **HC-SR04** (Ultrasonic) | `TRIG` | **GPIO 0** | 3.3V Output | 10 µs trigger pulse |
| | `ECHO` | **GPIO 1** | 3.3V / 5V | **Direct connection** (bench prototype) |
| | `VCC` | **5V** | 5V Power | Powers ultrasonic transducer |
| | `GND` | **GND** | 0V | Common ground |
| **MPU6050** (6-Axis IMU) | `SDA` | **GPIO 4** | 3.3V | Hardware I2C Data line |
| | `SCL` | **GPIO 5** | 3.3V | Hardware I2C Clock line |
| | `VCC` | **3V3** | 3.3V Power | Onboard 3.3V regulator rail |
| | `GND` | **GND** | 0V | Common ground |
| | `AD0` | **GND** | 0V | Sets I2C address to `0x68` |
| **Haptic Vibration Motor** | `(+) / Signal` | **GPIO 6** | 3.3V PWM | **Direct GPIO drive** (LEDC 200 Hz PWM, 8-bit) |
| | `(-) / Ground` | **GND** | 0V | Common ground |
| **Piezo Buzzer** | `(+) / Signal` | **GPIO 7** | 3.3V | Supports both Active & Passive 5V/3.3V buzzers |
| | `(-) / GND` | **GND** | 0V | Common ground |
| **Push Button (Optional)** | Switch | **GPIO 3** | 3.3V Input | Optional / unpopulated on bench build (Internal pullup) |
| | Return | **GND** | 0V | Ground (when button is populated) |
| **Status Blue LED** | Indicator | **GPIO 8** | 3.3V | Built-in on ESP32-C3 SuperMini (**Active LOW**) |

---

## Operating Logic & Feedback

1. **Clear Path ($> 120\text{ cm}$)**:
   - Vibration Motor: **OFF** (0% PWM)
   - Buzzer: **Silent**
   - Blue LED: **OFF** (HIGH)
2. **Caution & Warning Zone ($30\text{ cm} - 120\text{ cm}$)**:
   - Proportional vibration intensity ramps smoothly from 35% up to 100% as the obstacle nears.
   - Status LED reflects vibration activity.
3. **Critical Hazard Proximity ($< 30\text{ cm}$)**:
   - Vibration Motor: **100% Full Spin** (PWM 255)
   - Buzzer: **Rapid Urgent Beeping** (100ms ON / 100ms OFF)
   - Status LED: **Rapid Flashing**
4. **Cane Dropped / Fall Alarm ($> 65^\circ$ Tilt or Impact Spike)**:
   - High-priority alternating siren on buzzer.
   - Rhythmic tactile pulsing on vibration motor.
   - **Auto-Reset**: Restoring the cane upright ($< 30^\circ$) automatically clears and rearms the alarm (or pressing GPIO 3 button if installed).
5. **Startup Self-Test**:
   - On boot, the cane sounds **2 confirmation beeps** and runs a **1-second 100% vibration burst** to verify hardware actuators.

---

## How to Compile & Flash

### Method A: Arduino CLI (Recommended)
```bash
# Compile
arduino-cli compile --fqbn esp32:esp32:esp32c3:CDCOnBoot=cdc,FlashMode=dio firmware/core-sensing

# Flash to connected ESP32-C3
arduino-cli upload -p /dev/cu.usbmodem* --fqbn esp32:esp32:esp32c3:CDCOnBoot=cdc,FlashMode=dio firmware/core-sensing
```

### Method B: Arduino IDE
1. Open `firmware/core-sensing/core-sensing.ino`.
2. Under **Tools > Board**, select **`ESP32C3 Dev Module`**.
3. Under **Tools > USB CDC On Boot**, select **`Enabled`**.
4. Under **Tools > Flash Mode**, select **`DIO`**.
5. Select port `/dev/cu.usbmodem*` and click **Upload**.

### Method C: PlatformIO
```bash
cd firmware/core-sensing
pio run --target upload
```

---

## Serial Telemetry Output (115200 Baud)
```text
[CANE-C3] Dist:  88.8 cm | Tilt:  0.0° | Vib:  64% (PWM: 165) | Buzzer: MUTED   | Alert: CLEAR
[CANE-C3] Dist:   5.5 cm | Tilt:  0.0° | Vib: 100% (PWM: 255) | Buzzer: BEEPING | Alert: CRITICAL HAZARD!
```
