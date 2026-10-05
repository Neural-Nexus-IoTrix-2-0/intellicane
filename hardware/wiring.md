# ESP32-C3 bench wiring reference

![Circuit reference](circuit_diagram.png)

This is the required electrical reference. Confirm the physical assembly matches it before powering the demo. Earlier direct-Echo bench instructions were incorrect.

| Peripheral | Signal | ESP32-C3 GPIO | Requirement |
|---|---|---:|---|
| HC-SR04 | TRIG | 0 | Trigger output |
| HC-SR04 | ECHO | 1 | Level conversion from standard 5 V Echo |
| GY-521 / MPU6050 | SDA / SCL | 4 / 5 | I2C at 100 kHz; pull-ups to 3.3 V |
| Motor module | IN | 6 | Logic input to verified driver, 200 Hz PWM |
| Buzzer | Signal | 7 | Driver appropriate to actual voltage/current |
| Optional reset button | Switch to ground | 3 | INPUT_PULLUP |
| Status LED | Onboard | 8 | Active LOW |

## Echo conversion

For a standard HC-SR04 powered at 5 V, connect Echo through a 1 kOhm resistor to the GPIO 1 junction, then connect a 1.8 kOhm resistor from that junction to GND. Nominal input: 5 * 1.8 / (1 + 1.8) = 3.21 V. The divider is required on the bench as well as in a later enclosure. Verify resistor values and module identity. All grounds must be common.

## Sensor and actuator power

- Power the standard HC-SR04 from its rated 5 V supply.
- A GY-521 breakout may accept 5 V at VCC through its onboard regulator. Verify the actual breakout and ensure SDA/SCL pull up only to 3.3 V. Do not apply 5 V directly to a bare MPU6050 IC.
- A 3-pin motor module must contain a suitable driver and inductive protection. GPIO 6 supplies its control signal, not motor operating current. A bare motor requires a separate rated transistor/MOSFET driver and flyback protection.
- Match `MOTOR_ACTIVE_LOW` to the module; the current default is false.
- Match `BUZZER_IS_PASSIVE` to the buzzer; the default is false (active). Direct GPIO drive is suitable only for a verified low-current load within GPIO ratings. Use a driver for other loads.

## Power and mechanics

Use the verified bench power arrangement for the demo. Battery charging, boost regulation, low-battery monitoring and enclosure integration are future work. Avoid connecting competing power sources. Secure wires and isolate exposed connections; establish sensor orientation before testing tilt.

## Verification

Power off before rewiring. Check divider and common ground, then confirm near/far feedback, IMU orientation and recovery on a table. Record behavior against the actual firmware revision. No wiring photograph or physical measurements were available in this review, so the documentation does not certify the assembly.

References: [Espressif ESP32-C3 datasheet, Table 5-4](https://documentation.espressif.com/esp32-c3_datasheet_en.html), [Adafruit HC-SR04 interface guidance](https://www.adafruit.com/product/3942).
