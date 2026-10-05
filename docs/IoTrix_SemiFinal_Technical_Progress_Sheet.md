# Intelligent Cane: technical progress sheet

**Neural-Nexus · Track A · 5 October 2026**

**Team:** Dulnith, Thenul, Chamith, Suneth

**Repository:** https://github.com/Neural-Nexus-IoTrix-2-0/intelligent-cane

## Problem
White-cane users may benefit from supplementary feedback about obstacles in a sensor's field of view. The prototype explores affordable tactile and audible proximity feedback without requiring an internet service for the local response.

## Proposed solution
An ESP32-C3 SuperMini reads an ultrasonic sensor and increases motor duty and beep urgency below 60 cm. An inertial sensor provides a cane orientation/impact alarm. This is a bench proof of concept, not validated human-fall detection or complete navigation assistance.

## Architecture
```text
HC-SR04 ---- Echo timing ----> ESP32-C3 ---- PWM ----> Motor module
MPU6050 -------- I2C -------->    |    ------ alarm --> Buzzer
                                +---- USB / BLE ---> Terminal
Planned: companion phone location and network delivery to caregiver service
```

## Current progress
The team has connected the board, ultrasonic sensor, IMU, motor and buzzer. Active firmware implements sensing, feedback and BLE NUS telemetry. The firmware compiles and existing host logic tests pass. Physical performance and BLE operation need recorded bench evidence.

## Technology stack
ESP32-C3 SuperMini, HC-SR04, GY-521/MPU6050, motor module, buzzer; embedded C++ using Arduino-ESP32 3.3.2, Adafruit MPU6050, Wire I2C, LEDC PWM and BLE GATT/NUS. Listed base electronics cost: LKR 2,800, excluding unpriced divider, power, cane and enclosure.

## Testing
On 5 October, ESP32-C3 compilation passed and the existing host tests passed for distance boundaries, invalid values, monotonic feedback, polarity, beep transitions and timer rollover. Physical accuracy, latency, range and battery runtime remain unmeasured in the submitted evidence. Test log: `docs/test-logs/2026-10-05-software-validation.md`.

## Limitations
One ultrasonic sensing direction; no Echo is ambiguous. Tilt/impact may indicate cane handling rather than a person falling. No downward sensor, caregiver delivery or AI implementation. Echo level conversion and actual motor/buzzer drivers require physical verification. No guaranteed response-time or battery-runtime claim.

## Next step
Record repeated distance and tilt tests, secure wiring and mounting, improve fault indication and motion classification, then complete one phone-to-caregiver alert path. Camera/AI remains an optional later extension.
