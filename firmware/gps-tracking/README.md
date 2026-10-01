# GPS Tracking Subsystem (Phase 2)

## Purpose

The GPS tracking module provides remote location telemetry for caregivers and family members through the cloud dashboard.

## Planned Features

- GPS NMEA parsing from a u-blox NEO-6M / NEO-8M module over hardware UART.
- Geolocation coordinate filtering (latitude, longitude, speed, HDOP/accuracy).
- Emergency SOS button triggering immediate cloud telemetry alerts.
- Primary network transport: Wi-Fi (MQTT/HTTP REST).
- Optional fallback transport: LoRa (e.g., SX1276/SX1278 transceiver) for long-range, off-grid telemetry.

## Hardware Interface (Planned)

| Pin / Function | ESP32 GPIO | Description |
|----------------|------------|-------------|
| GPS TX (Module RX) | GPIO 17 (UART2 TX) | Configuration commands to GPS |
| GPS RX (Module TX) | GPIO 16 (UART2 RX) | NMEA sentences stream from GPS |
| Emergency Button | GPIO 4 | Long-press triggers caregiver emergency alert |

*Note: Implementation will begin in Phase 2 once Phase 1 sensing is verified.*
