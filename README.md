# Intelligent Cane

**Neural-Nexus · IoTrix 2.0 · Track A: Embedded IoT System Development**

An ESP32-C3 SuperMini bench prototype for supplementary obstacle awareness. An ultrasonic sensor controls proportional vibration and audible feedback. An inertial sensor provides a cane orientation/impact alarm. Local feedback does not require a phone or internet service.

**Team:** Dulnith, Thenul, Chamith, Suneth. **Status on 5 October 2026:** basic hardware connected, active firmware compiles, existing host logic tests pass. Physical accuracy, end-to-end latency, BLE range and battery runtime still need recorded measurements. A cane alarm is not validated human-fall detection.

## Semi-final package

The latest organizer announcement specifies **10 minutes total: 4 minutes demonstration, 2 minutes progress check, 4 minutes Q&A**. All four members must attend with cameras on. Presentation slides are optional and receive no separate marks.

- [Technical progress sheet](docs/IoTrix_SemiFinal_Technical_Progress_Sheet.md)
- [10-minute demonstration and Q&A guide](docs/IoTrix_SemiFinal_Defense_and_Demo_Guide.md)
- [Current architecture](docs/architecture.md)
- [Software validation evidence](docs/test-logs/2026-10-05-software-validation.md)
- [Physical test record template](docs/test-logs/physical-test-template.md)
- [Project proposal](docs/proposals/Intelligent_Cane_Proposal.md)
- [Submission checklist](docs/SUBMISSION_CHECKLIST.md)
- Deliverable files: `output/semifinal/`

## Current behavior

| Input | Firmware behavior |
|---|---|
| Valid distance >=60 cm | Obstacle vibration and beeps off |
| Valid distance <60 cm | Increasing motor PWM and beep urgency as distance decreases |
| Distance <=10 cm, greater than zero | Full commanded motor duty |
| Missing Echo / invalid distance | Obstacle feedback off; telemetry indicates NO ECHO for nonfinite readings |
| Tilt >=65 degrees OR acceleration >=2.5 g | Cane orientation/impact alarm |
| Tilt <30 degrees with trigger absent | Alarm clears |

An IMU alarm can sound independently of obstacle distance. The current alarm has no 1.5-second debounce or immobility classifier. Motor duty is a command, not a calibrated measure of perceived vibration.

## Hardware and interfaces

| Component | Connection |
|---|---|
| HC-SR04 | TRIG GPIO 0; ECHO GPIO 1 through level conversion |
| GY-521 / MPU6050 | SDA GPIO 4, SCL GPIO 5; I2C at 100 kHz |
| 3-pin vibration module | IN GPIO 6; module power from its rated supply |
| Buzzer | GPIO 7; driver and supply appropriate to actual buzzer |
| Optional reset button | GPIO 3 to GND, INPUT_PULLUP |
| Onboard LED | GPIO 8, active LOW |

A standard 5 V HC-SR04 needs Echo level conversion for the 3.3 V GPIO, including on the bench. The schematic uses 1 kOhm / 1.8 kOhm. Verify the physical circuit matches [wiring.md](hardware/wiring.md). A bare motor must not draw its operating current from a GPIO.

## Firmware and build

`firmware/core-sensing/` is the current implementation. It attempts sensor sampling every 40 ms and telemetry every 250 ms. Echo measurement and BLE transmission contain blocking work, so there is no demonstrated hard response-time bound.

```sh
arduino-cli compile --fqbn esp32:esp32:esp32c3:CDCOnBoot=cdc,FlashMode=dio firmware/core-sensing
```

Select the board's actual serial port before uploading. Open USB serial at 115200 baud. For BLE, connect to `Intelligent-Cane` and subscribe to NUS TX UUID `6E400003-B5A3-F393-E0A9-E50E24DCCA9E`. Verify the chosen phone app supports BLE GATT notifications.

```sh
g++ -std=c++11 -Wall -Wextra -pedantic tests/proximity_feedback_test.cpp -o /tmp/proximity_feedback_test
/tmp/proximity_feedback_test
```

The host tests cover feedback logic, not physical sensor accuracy or electrical operation.

## Implemented and planned

**Implemented in active firmware:** ranging, PWM feedback, configurable buzzer drive, acceleration-based orientation/impact alarm, serial diagnostics, BLE NUS notifications and reset command handling.

**Planned:** phone location and caregiver alert delivery, battery monitoring/runtime validation, final mechanical packaging, downward sensing and optional camera/AI narration. The camera, dashboard and voice-service directories currently contain plans. `firmware/archived-dual-tof/` and `simulation/wokwi/` are separate reference implementations and do not validate the current C3 build.

## Cost

Listed base electronics total **LKR 2,800**, excluding required divider costs not yet entered, battery, cane structure, enclosure and phone. All listed optional/future rows bring the component estimate to **LKR 14,400** before unpriced items and service costs. See [BOM](hardware/BOM.md).
