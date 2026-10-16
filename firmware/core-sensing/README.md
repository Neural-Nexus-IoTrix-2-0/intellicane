# Core Sensing Subsystem — ESP32-C3 SuperMini

This project contains the active obstacle detection and haptic feedback firmware for the **ESP32-C3 SuperMini**.

---

## Hardware Pinout (ESP32-C3 SuperMini)

| Peripheral | Function | ESP32-C3 Pin | Logic Level | Electrical Notes |
|:---|:---|:---:|:---:|:---|
| **HC-SR04** (Ultrasonic) | `TRIG` | **GPIO 0** | 3.3V Output | 10 µs trigger pulse |
| | `ECHO` | **GPIO 1** | 3.3V input | Required Echo divider / level conversion |
| | `VCC` | **5V** | 5V Power | Powers ultrasonic transducer |
| | `GND` | **GND** | 0V | Common ground |
| **MPU6050** (6-Axis IMU) | `SDA` | **GPIO 4** | 3.3V | Hardware I2C Data line (GY-521 pin 4) |
| | `SCL` | **GPIO 5** | 3.3V | Hardware I2C Clock line (GY-521 pin 3) |
| | `VCC` | **5V** | 5V Power | Powers GY-521 onboard 3.3V regulator |
| | `GND` | **GND** | 0V | Common ground |
| | `AD0` | **GND** | 0V | Sets I2C address to `0x68` (leave empty for default) |
| **Vibration Motor** | `IN / Signal` | **GPIO 2** | 3.3V PWM | All proximity cues, via motor driver |
| | `VCC` | **5V** | 5V Power | Full power rail for maximum vibration torque |
| | `GND` | **GND** | 0V | Common ground |
| **Buzzer** | `(+) / Signal` | **GPIO 7** | 3.3V | Active buzzer by default; passive piezo is configurable |
| | `(-) / GND` | **GND** | 0V | Common ground |
| **Push Button (Optional)** | Switch | **GPIO 3** | 3.3V Input | Optional / unpopulated on bench build (Internal pullup) |
| | Return | **GND** | 0V | Ground (when button is populated) |
| **Status Blue LED** | Indicator | **GPIO 8** | 3.3V | Built-in on ESP32-C3 SuperMini (**Active LOW**) |

---

## Operating Logic & Feedback

Proximity cues respond only to a valid distance **strictly below 60 cm**. The
fall state silences the motor and the buzzer. Boot does not run an actuator
test burst; use the Serial or BLE `motor` diagnostic command when needed.

| Distance | Motor (GPIO 2) | Buzzer cadence |
|---|---|---|
| 60 cm or farther | Off | Silent |
| Greater than 40 cm and below 60 cm | Gentle 150 ms pulses, faster as distance falls | Widely spaced chirps |
| Greater than 10 cm and up to 40 cm | Continuous graded vibration | Brief chirps with increasing urgency |
| 10 cm or closer | Full commanded duty, continuous | 120 ms on / 220 ms off |
| Neither range sensor has a valid reading | Off | Silent; range is unknown |

GPIO 6 is unused; the firmware does not configure it as an actuator output.
The Serial and BLE `motor` commands test the GPIO 2 motor for 500 ms.

A missing echo means the distance is unknown, not a verified clear path. The
fall detector's trigger and reset thresholds are unchanged.

### Match the actual modules before flashing

- `MOTOR_ACTIVE_LOW = false` selects active-HIGH motor driver inputs. Verify
  the module's polarity on the actual cane; software cannot infer it.
- `BUZZER_IS_PASSIVE = false` drives an active buzzer with DC on/off gating.
  Set it to `true` for a passive piezo; its tone is held at 800 Hz. An active
  buzzer's pitch and loudness are hardware-fixed, so software changes cadence.
- The motor uses 200 Hz PWM. A passive buzzer uses a separate LEDC timer.
  Arduino-ESP32 2.x and 3.x APIs are handled separately.

### Verification

Host-side logic tests (requires a C++11 compiler), from the repository root:

```sh
g++ -std=c++11 -Wall -Wextra -pedantic tests/proximity_feedback_test.cpp -o proximity_feedback_test
./proximity_feedback_test
```

After flashing, check 80, 60, 59, 40, 25, 20 and 10 cm. Verify caution
pulses on GPIO 2, continuous vibration at 40 cm and closer, silence at 60 cm and during a
fall, motor polarity, missing echoes and the configured buzzer type on the real
hardware. Host tests cannot verify vibration strength or sound level.

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
[CANE-C3] Dist: 88.8cm | ToF: 88.8cm | US:NO_ECHO | Tilt:  0.0° | Fall:0 | Vib:  0% (Motor:  0) | Buzz:OFF | CLEAR
[CANE-C3] Dist:  5.5cm | ToF:  5.5cm | US:12.0cm | Tilt:  0.0° | Fall:0 | Vib:100% (Motor:255) | Buzz:ON | CRITICAL HAZARD!
```

The motor field reflects instantaneous duty, including caution pulses above
40 cm. At close range the motor stays on while the buzzer continues to chirp.
