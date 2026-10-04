# Geolocation & Telemetry Subsystem (Phase 2)

## Architectural Shift (ADR-007)

Per **ADR-007**, the system does **not** use a standalone hardware GPS module (such as the u-blox NEO-6M/8M) on the cane. Instead, it leverages the **ESP32-C3's built-in Bluetooth Low Energy (BLE 5.0)** connection to bridge directly with the user's companion smartphone.

### Why Smartphone BLE Geolocation?
1. **Zero Hardware Cost**: Saves ~Rs. 2,800 in module and patch antenna costs.
2. **Minimal Power Consumption**: BLE transmission consumes a fraction of the 50–80 mA continuous draw required by active GNSS receivers, dramatically extending battery life.
3. **Immediate & Indoor Fix**: Smartphones use Assisted GPS (A-GPS), cell tower multilateration, and Wi-Fi SSID scanning, providing immediate coordinates indoors and underground where standard GPS modules lose satellite fix.
4. **Ergonomic Weight**: Eliminates module bulk and heavy ceramic antennas from the cane handle.

---

## Planned Architecture

```
┌─────────────────────┐       BLE 5.0        ┌─────────────────────────┐
│  Intelligent Cane   │  Telemetry & Alert   │   Companion Smartphone  │
│ (ESP32-C3 SuperMini)├─────────────────────►│     (Flutter/iOS/Android)   │
└─────────────────────┘                      └────────────┬────────────┘
                                                          │ App attaches
                                                          │ phone A-GPS fix
                                                          ▼
                                             ┌─────────────────────────┐
                                             │  Caregiver Web Dashboard│
                                             │     (Cloud / REST API)  │
                                             └─────────────────────────┘
```

1. **BLE Telemetry Stream**: ESP32-C3 broadcasts real-time obstacle distance, tilt angle, and fall alarm state.
2. **Smartphone Companion App**: Listens to BLE notifications, attaches device latitude/longitude coordinates, and pushes updates over 4G/5G/Wi-Fi.
3. **Emergency Fall Dispatch**: When a fall alarm or emergency state is detected, the phone app sends an urgent push alert with live map pin coordinates to registered family caregivers.
