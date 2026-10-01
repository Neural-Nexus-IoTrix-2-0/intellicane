# ESP32-CAM Vision Module (Phase 3)

## Purpose

The camera module captures high-resolution snapshots of the user's surrounding environment when requested and forwards them to the AI voice service for scene interpretation, obstacle identification, and text reading.

## Architectural Separation

The ESP32-CAM runs on a dedicated secondary microcontroller board:
- **Pin Availability**: An ESP32-CAM board consumes nearly all exposed pins for the OV2640 camera interface, leaving almost no free GPIOs for I2C, PWM, or ultrasonic sensors.
- **Reliability Isolation**: Real-time haptic sensing (Phase 1) is life-critical and must run 100% offline without CPU interruption from camera image capture, Wi-Fi reconnection, or TLS handshakes.
- **Power Efficiency**: The camera module can enter deep sleep when not in active use and wake on demand.

## Planned Interfaces

- **Camera**: OV2640 2MP CMOS sensor.
- **Capture Trigger**: Hardware interrupt line or UART signal from the primary sensing board, or physical button on cane handle.
- **Uplink**: Wi-Fi station mode pushing JPEG multipart POST or WebSockets to the `ai-voice-service`.
