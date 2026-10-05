# IntelliCane

**Neural-Nexus · IoTrix 2.0 · Track A: Embedded IoT System Development**

IntelliCane is an affordable, multimodal smart white cane developed by undergraduate engineering students for IoTrix 2.0. It pairs an instant, 100% offline ESP32-C3 hardware reflex on the cane with a companion Android app for blind users and their caretakers. An ultrasonic sensor drives smooth, progressive handle vibrations as obstacles get closer, an onboard IMU detects sudden falls, and BLE syncs live location and emergency alerts to caregivers via Firebase.

**Team Members:** Dulnith Liyanage, Thenul Sahansa, Chamith Chethana, Suneth Vidurasa  
**Status on 5 October 2026:** Bench prototype assembled and verified, firmware compiled, unit tests passing, and Android companion app operational. Bench electronics cost: **LKR 2,800**.

## Semi-final package

The organizer announcement specifies **10 minutes total: 4 minutes demonstration, 2 minutes progress check, 4 minutes Q&A**. All four members must attend with cameras on.

- [**One-Page Technical Proposal**](ONE_PAGE_TECHNICAL_PROPOSAL.md) *(PDF: [`ONE_PAGE_TECHNICAL_PROPOSAL.pdf`](ONE_PAGE_TECHNICAL_PROPOSAL.pdf))*
- [**Project Proposal Submission PDF**](docs/Neural-Nexus.pdf) *(LaTeX source: `docs/proposals/IntelliCane_Project_Proposal.tex`)*
- [10-minute demonstration and Q&A guide](docs/IoTrix_SemiFinal_Defense_and_Demo_Guide.md)
- [System architecture](docs/architecture.md)
- [Software validation evidence](docs/test-logs/2026-10-05-software-validation.md)
- [Android Companion App](application/) (`application/` — User Dashboard, Caretaker Mode, live GPS map, automated SMS alerts, and instant emergency push notifications with sound & vibration on fall detection)

## Current behavior

| Input | Firmware behavior |
|---|---|
| Valid distance >=60 cm | Obstacle vibration and beeps off |
| Valid distance <60 cm | Increasing motor PWM and beep urgency as distance decreases |
| Distance <=10 cm, greater than zero | Full commanded motor duty |
| Missing Echo / invalid distance | Obstacle feedback off; telemetry indicates NO ECHO for nonfinite readings |
| Tilt >=65 degrees OR acceleration >=2.5 g | Silent fall state on cane (motor & buzzer silenced); dispatches `FALL_STATE:1` over BLE |
| Tilt <30 degrees with trigger absent | Fall cleared; dispatches `FALL_STATE:0` over BLE and restores normal navigation |

Local motor and buzzer are silenced during fall events to prevent user distress while high-priority BLE telemetry immediately alerts the companion mobile app and caretaker. Motor duty is smoothly modulated via 200 Hz LEDC hardware PWM.

## Hardware and interfaces

| Component | Connection |
|---|---|
| HC-SR04 Ultrasonic | TRIG GPIO 0; ECHO GPIO 1 (direct connection on bench prototype) |
| GY-521 / MPU6050 | SDA GPIO 4, SCL GPIO 5; hardware I2C at 100 kHz |
| 3-pin vibration module | IN GPIO 6; 200 Hz LEDC PWM, module powered from 5 V rail |
| Buzzer | GPIO 7; audio hazard alerts |
| Optional reset button | GPIO 3 to GND, INPUT_PULLUP |
| Onboard LED | GPIO 8, active LOW |

On the bench prototype, HC-SR04 Echo connects directly to GPIO 1. Common ground is shared across all modules. Refer to [wiring.md](hardware/wiring.md) for detailed electrical documentation.

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

**Implemented in active firmware & app:** ranging, smooth proportional haptic feedback, buzzer proximity tone, silent fall detection on the cane with instant BLE dispatch (`FALL_STATE:1`), serial diagnostics, native Android application (`application/`) with Blind User Dashboard, collapsible floating terminal, Caretaker Mode, live ESRI/OSMDroid map with user-centering FAB, real-time Firestore sync, automated emergency SMS alerts, and high-priority emergency push notifications (sound & vibration) on the caretaker's phone.

**Planned:** battery monitoring and runtime validation, final mechanical packaging with TPU shock damping, downward drop/curb sensing, and optional secondary Time-of-Flight ranging.

## Cost

Listed base electronics total **LKR 2,800** (using direct Echo connection without voltage divider), excluding battery, cane structure, enclosure and phone. All listed optional/future rows bring the component estimate to **LKR 14,400** before unpriced items and service costs. See [BOM](hardware/BOM.md).
