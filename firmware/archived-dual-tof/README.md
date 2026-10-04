# Archived Dual-ToF Subsystem (Initial Concept)

This folder contains the initial dual Time-of-Flight (VL53L1X forward + VL53L0X downward) modular firmware implementation.

## Context
During early hardware procurement, VL53L1X/VL53L0X laser ToF sensors were unavailable locally. The physical build was migrated to:
- **MCU**: ESP32-C3 SuperMini
- **Distance Sensor**: HC-SR04 Ultrasonic Sensor
- **Haptics**: ERM Vibration Motor (LEDC PWM)
- **IMU**: MPU6050
- **Audio**: Piezo Buzzer

The active firmware is located in [`firmware/core-sensing/`](../core-sensing/).
This archive is preserved for reference should ToF sensors be integrated in later revisions.
