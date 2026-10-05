# Core Sensing Subsystem — ESP32-C3 SuperMini

This project contains the prototype firmware for the obstacle detection and haptic feedback layer of the Intelligent Cane, targeting the ultra-compact **ESP32-C3 SuperMini** microcontroller.

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
| **3-Pin Vibration Motor** | `IN / Signal` | **GPIO 6** | 3.3V PWM | 200 Hz LEDC PWM to onboard driver transistor |
| | `VCC` | **5V** | 5V Power | Full power rail for maximum vibration torque |
| | `GND` | **GND** | 0V | Common ground |
| **Piezo Buzzer** | `(+) / Signal` | **GPIO 7** | 3.3V | Supports both Active & Passive 5V/3.3V buzzers |
| | `(-) / GND` | **GND** | 0V | Common ground |
| **Push Button (Optional)** | Switch | **GPIO 3** | 3.3V Input | Optional / unpopulated on bench build (Internal pullup) |
| | Return | **GND** | 0V | Ground (when button is populated) |
| **Status Blue LED** | Indicator | **GPIO 8** | 3.3V | Built-in on ESP32-C3 SuperMini (**Active LOW**) |

---

## Operating Logic & Feedback

The motor responds only to a valid distance **strictly below 60 cm**. Tilt/fall
state never drives the motor, and boot does not run an actuator test burst.

| Distance | Motor PWM | Obstacle buzzer |
|---|---|---|
| 60 cm or farther | OFF | Silent |
| Just below 60 cm | About 39% | Short, widely spaced beeps |
| 40 cm | About 63% | 116 ms ON / 460 ms OFF |
| 20 cm | About 87% | 172 ms ON / 180 ms OFF |
| 10 cm or closer | 100% | 200 ms ON / 40 ms OFF |
| Missing echo / invalid reading | OFF | Silent; serial reports `NO ECHO` |

A missing echo means the distance is unknown, not a verified clear path.
The existing fall detector retains its separate 200 ms ON / 200 ms OFF audible
alarm, including beyond 60 cm. Its detection/reset behavior is otherwise unchanged.

### Match the actual modules before flashing

- `MOTOR_ACTIVE_LOW = false` retains the existing active-HIGH module setting.
  If serial shows `PWM: 0` while the module still vibrates, verify its polarity
  and wiring; set this to `true` for an active-LOW input. Software cannot detect
  the module polarity. Use IN on GPIO 6 with a common ground.
- `BUZZER_IS_PASSIVE = false` selects an active buzzer. Its beep rate and ON-time
  increase with proximity; its instantaneous loudness/pitch is hardware-fixed.
  Set this to `true` for a passive piezo: its tone additionally rises from
  1200 Hz toward 2800 Hz. This is an urgency ramp, not calibrated volume control.
- Motor and passive-buzzer PWM use separate timers. Arduino-ESP32 2.x and 3.x
  LEDC APIs are handled explicitly.

### Verification

Host-side logic tests (requires a C++11 compiler), from the repository root:

```sh
g++ -std=c++11 -Wall -Wextra -pedantic tests/proximity_feedback_test.cpp -o proximity_feedback_test
./proximity_feedback_test
```

After flashing, check 80, 60, 59, 40, 20 and 10 cm. Verify no motor output at
60 cm or farther even when tilted. Check motor polarity against serial PWM,
missing echoes, beep re-entry and the configured buzzer type on the real hardware.
Host tests do not verify electrical wiring, physical motor response or sound level.

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
[CANE-C3] Dist:  88.8 cm | Tilt:  0.0° | Vib:   0% (PWM:   0) | Buzzer: OFF   | Alert: CLEAR
[CANE-C3] Dist:   5.5 cm | Tilt:  0.0° | Vib: 100% (PWM: 255) | Buzzer: ON | Alert: CRITICAL HAZARD!
```
