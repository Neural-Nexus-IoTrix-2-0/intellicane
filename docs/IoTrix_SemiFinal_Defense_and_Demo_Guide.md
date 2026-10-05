# 10-minute evaluation: demonstration and speaking guide

**Neural-Nexus: Dulnith, Thenul, Chamith, Suneth**

The latest organizer announcement supersedes the PDF's recommended 12-minute session: **4 minutes demonstration, 2 minutes progress check, 4 minutes Q&A**. All four members must attend with cameras on. Slides are optional and receive no separate marks. Show the physical prototype prominently during the demonstration.

## Suggested roles and timing

| Time | Speaker | Action |
|---|---|---|
| 0:00-0:20 | Dulnith | Introduce the user problem and show the five connected components. Slide 1. |
| 0:20-2:30 | Thenul | Move a flat target through 80, 40, 20 and 10 cm. Show live distance and feel/hear the output. Slide 2 is a reference, not measured data. |
| 2:30-4:00 | Chamith | Keep target far away, tilt the IMU gently, restore upright. Show local operation without a phone. If already verified, briefly show BLE NUS telemetry. Slide 3. |
| 4:00-5:00 | Suneth | State connected hardware, implemented firmware, compile/test results and recorded base cost. Slide 4. |
| 5:00-6:00 | Suneth | Explain limitations and the next work before the final. Slide 5. |
| 6:00-10:00 | All | Q&A and judge-selected input changes. Slide 6; appendices only if useful. |

Roles are a proposed speaking allocation, not a claim about who authored each subsystem. Rehearse handovers. One member controls screen sharing, another maintains the hardware close-up. Keep all four cameras on and avoid hiding the hardware behind the shared slides.

## Opening: Dulnith

“We are Neural-Nexus. Our Intelligent Cane prototype explores affordable obstacle feedback for white-cane users. We have connected an ESP32-C3, ultrasonic sensor, inertial sensor, vibration motor and buzzer. We will demonstrate the local feedback loop and explain the progress still needed.”

## Distance demo: Thenul

“Here is the distance reading. Above 60 centimetres, the obstacle feedback is off. As the target approaches, the motor duty and beep urgency increase. At 10 centimetres the commanded motor duty reaches its maximum. The chart shows the programmed mapping; these live readings show what the hardware is doing.”

Start far away, then show 40, 20 and 10 cm. Keep the target flat and sensor fixed. Explain any mismatch honestly. If the IMU alarm interferes, restore its calibrated upright orientation. Do not claim calibrated motor loudness or tactile strength.

## Tilt and local operation: Chamith

“This sensor gives the controller acceleration data. Our current alarm triggers at 65 degrees of tilt or a 2.5 g acceleration magnitude. It clears below 30 degrees when the trigger is absent. This is a cane orientation or impact alarm. We still need to distinguish normal handling from a possible user fall. Local obstacle feedback does not need the phone.”

Tilt gently on a table and restore. Do not drop the hardware or simulate a human fall. If BLE was rehearsed, connect to `Intelligent-Cane`, subscribe to TX `6E400003-B5A3-F393-E0A9-E50E24DCCA9E`, then disconnect and repeat one near/far test. Skip pairing attempts during the timed demo if BLE is unreliable. USB telemetry is the fallback.

## Progress check: Suneth

“Our basic hardware is connected. The active firmware implements proximity feedback, the cane alarm and BLE telemetry. The ESP32-C3 build passes, and our existing software tests pass for boundaries, invalid distances, feedback progression and buzzer timing. We still need recorded physical accuracy, latency, radio-range and battery tests. The listed base electronics cost is 2,800 rupees, excluding power, cane structure, enclosure and unpriced divider components.”

“Before the final we will prioritize wiring and mounting, repeated sensor tests and clearer fault indication. We will then work toward one phone-to-caregiver alert path. Downward sensing and AI narration remain future work.”

## Q&A answers

- **Why this board?** It provides sensor interfaces, PWM and BLE in the chosen compact board. A distance-only build could use simpler hardware; BLE supports the extension path.
- **Why BLE?** It carries small telemetry messages to a nearby compatible phone. Actual energy and latency need measurement. It is a GATT/NUS connection, not Bluetooth Classic serial.
- **Where is the IoT integration?** BLE telemetry is in the firmware and can be demonstrated if verified. Phone location and remote caregiver delivery are planned; we do not claim a working cloud system.
- **Network loss?** The local feedback calculation runs on the cane. Future remote alerts will depend on phone/network availability and will need delivery status.
- **Does this detect a person falling?** It currently detects cane orientation/impact. Normal handling or dropping the cane can trigger it. Human-fall classification is unvalidated.
- **What happens with no Echo?** Distance is unknown. Obstacle output turns off and telemetry reports NO ECHO. A distinct persistent-fault indication is planned.
- **Why a voltage divider?** Standard HC-SR04 Echo is 5 V; the MCU GPIO is 3.3 V. The reference uses 1 kOhm and 1.8 kOhm, about 3.21 V output. Confirm the actual wiring before showing it.
- **Response time?** Sensor scheduling is nominally 40 ms, but Echo acquisition and BLE sends block. We have not measured end-to-end latency and do not claim a guaranteed sub-50 ms response.
- **How validated?** Show the software validation record and any physical measurements the team actually collected. Passing selected tests is not 100% system coverage.
- **What is the main risk?** Limited sensor coverage and missed/ambiguous readings, plus confusing cane movement with a user emergency. Improve validation before expanding features.

## Before joining

1. Verify Echo level conversion, common ground, driver modules and power with the board off before any rewiring.
2. Confirm the flashed firmware matches the slide behavior. Fix sensor orientation and secure loose wires.
3. Save real distance readings and a short demo video. Do not substitute expected outputs for observations.
4. Open the deck, USB terminal and backup clip locally. Confirm conferencing audio carries the buzzer and the camera shows the target movement.
5. All four members join with cameras on. Keep a visible timer and hand off at 4:00 and 6:00.

If hardware fails live, state the fault and show the saved evidence. A backup clip supplements the required live demonstration. Do not claim it proves current live operation.
