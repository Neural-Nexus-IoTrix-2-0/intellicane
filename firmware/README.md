# Intelligent Cane Firmware

This directory contains embedded software running across the cane's microcontrollers.

## Structure

- **`core-sensing/` (Phase 1 — Current Focus)**:
  - ESP32-based safety-critical subsystem.
  - Interfaces with forward ToF (VL53L1X), downward drop-off sensor (ultrasonic/VL53L0X), and 6-axis IMU (MPU6050).
  - Handles real-time haptic vibration motor PWM feedback, buzzer alarm, and fall detection.
  - Fully offline, deterministic, zero network dependencies.

- **`gps-tracking/` (Phase 2)**:
  - GPS NMEA parsing (NEO-6M / NEO-8M) and telemetry uplink (Wi-Fi HTTP/MQTT or LoRa backup).
  - Transmits location and health/emergency pings to family dashboard.

- **`cam-module/` (Phase 3)**:
  - Dedicated ESP32-CAM board sketch.
  - Captures snapshot images on user trigger/button press and streams them via Wi-Fi to the `ai-voice-service`.
  - Kept on an independent board to preserve GPIOs and flash memory bandwidth.
