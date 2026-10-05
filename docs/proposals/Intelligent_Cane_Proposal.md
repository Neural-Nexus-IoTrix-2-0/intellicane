# Intelligent Cane: project proposal

**Neural-Nexus · Track A: Embedded IoT System Development**

**Dulnith, Thenul, Chamith, Suneth · 5 October 2026**

## Problem and intended use

The project explores supplementary obstacle awareness for visually impaired white-cane users. A sensor mounted on a cane could provide advance tactile and audible feedback about obstacles within its field of view. The design aims to keep this local response independent of a phone or internet service. The prototype does not establish complete navigation coverage or replace mobility training.

## Proposed solution and current scope

An ESP32-C3 SuperMini reads one HC-SR04 ultrasonic sensor and a GY-521/MPU6050 inertial module. A motor module provides proportional vibration and a buzzer provides audible feedback. The team has connected these components for a bench demonstration. The firmware activates obstacle feedback below 60 cm, reaching full commanded motor duty at 10 cm. It separately triggers a cane orientation/impact alarm at tilt >=65 degrees or acceleration magnitude >=2.5 g. It clears below 30 degrees when the trigger is absent.

This is a simple cane-state alarm. A cane drop or ordinary handling may trigger it, and a person's fall may not. Human-fall classification requires further development and validation.

## Architecture and IoT extension

Sensor signals enter the C3 through Echo timing and I2C. The controller calculates motor duty and beep timing locally, then emits USB serial and BLE Nordic UART Service telemetry. BLE connects to a phone terminal when a compatible client subscribes. A future companion app would attach phone location, accuracy and timestamp to an event, then use the phone's network to notify a caregiver and record delivery acknowledgment. That app and remote service are not implemented.

Downward sensing and on-demand camera/AI narration are possible later extensions. They are outside the active bench prototype. The core feedback remains independent of those services, although BLE and local sensing currently share MCU execution time.

## Technology and component choices

The stack uses embedded C++, Arduino-ESP32 3.3.2, Adafruit MPU6050, Wire I2C at 100 kHz, 200 Hz LEDC motor PWM and BLE GATT/NUS. The chosen board provides the required interfaces and a BLE extension path. The ultrasonic sensor gives a simple ranging interface, but coverage and surface-dependent returns require testing.

Standard HC-SR04 Echo requires conversion from 5 V to the C3's 3.3 V logic. The reference uses a 1 kOhm / 1.8 kOhm divider. I2C pull-ups must be to 3.3 V, and motor/buzzer current must use a suitable driver. The team must verify the actual assembly against the schematic.

## Progress and validation evidence

The active firmware compiled for ESP32-C3 on 5 October 2026. Existing host tests passed for boundary/invalid inputs, monotonic feedback, polarity, beep transitions and timer rollover. The team confirms basic hardware connections. Physical accuracy, response latency, BLE range, runtime power and usability measurements have not been supplied in the evidence reviewed. The project makes no numerical claims for those outcomes.

The next bench test will record repeated measurements at 80, 60, 59, 40, 20 and 10 cm, compare expected and actual actuator behavior, and test tilt/recovery and local operation with the phone disconnected. Record no-Echo events and false alarms. Controlled tests should precede supervised evaluation with intended users and mobility practitioners.

## Limitations and risk management

One sensor covers only its mounted field of view. No Echo is ambiguous and currently disables obstacle feedback; a separate persistent-fault indication is planned. The warning range and haptic/audio pattern need usability evaluation. The current alarm does not classify human falls. Some sensor and BLE operations block, so hard response-time bounds are unproven. Breadboard wiring, unverified power/driver arrangements and unfinished mounting can impair reliability. Phone-based remote delivery will depend on connectivity, permissions and location quality, with opt-in sharing.

## Budget and feasibility

Listed base electronics total LKR 2,800: MCU 1,400, ultrasonic 450, IMU 600, motor module 250, buzzer/wiring 100. Required divider pricing has not been entered. Including all listed optional/future component rows totals LKR 14,400 before unpriced items, an existing phone and ongoing service costs. These are repository estimates, not independently checked supplier quotations. Battery runtime and product weight will be measured rather than assumed.

## Planned work before the final

The event website lists the final on 17 October 2026. The proposed priorities are: verify electrical interfaces and secure mounting; collect repeated physical and failure-case tests; improve fault indication and cane-motion classification; then attempt one phone-to-caregiver alert flow. The team will scope that last integration against the time remaining and report incomplete work honestly. Camera/AI and a custom PCB remain later options if they address demonstrated needs.

## Deliverables

Current firmware, architecture and wiring reference, one-page technical progress sheet, software validation record, physical evidence as collected, and a live 4-minute demonstration followed by a 2-minute progress check and 4-minute Q&A.

Repository: https://github.com/Neural-Nexus-IoTrix-2-0/intelligent-cane

Submission: https://forms.gle/cEg6tZt5X29xi27S9

This proposal summarizes the current scope. Check it against any previously registered proposal and any form-specific template before submission.
