# Firmware

- `core-sensing/`: active ESP32-C3 SuperMini implementation. HC-SR04 ranging, MPU6050 acceleration-based tilt/impact alarm, motor PWM, buzzer, USB diagnostics and BLE NUS telemetry. Sensors are attempted every 40 ms and telemetry every 250 ms. Some operations block; timing remains to be measured.
- `archived-dual-tof/`: historical reference implementation, not the active hardware configuration.
- `gps-tracking/`: planned companion-phone geolocation design. No location firmware/app is implemented here.
- `cam-module/`: planned camera subsystem. No working camera firmware is supplied here.

Build and operating instructions are in `core-sensing/README.md`. Use Echo level conversion and an appropriate motor driver. Local sensing does not depend on internet access, but this is not a guarantee of safe navigation.
