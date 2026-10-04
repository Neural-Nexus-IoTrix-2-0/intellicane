# Architectural Decision Records (ADR) & Design Log

This document records architectural, hardware, and firmware design decisions, trade-offs, pin selections, and component ambiguities for the Intelligent Cane project (`Neural-Nexus-IoTrix-2.0`).

---

## ADR-001: Separation of Sensor ESP32 from ESP32-CAM (Phase 1 vs. Phase 3)

### Status
**Accepted**

### Context
The cane requires real-time obstacle and drop-off detection (safety-critical) as well as an AI camera subsystem for scene narration. The team considered whether a single ESP32-CAM module could handle both sensor reading / motor PWM and camera streaming, or if two microcontrollers were necessary.

### Decision
Use two physically separate boards:
1. **Primary MCU (ESP32 DevKit V1)**: Dedicated exclusively to real-time safety functions (VL53L1X, downward sensor, MPU6050, haptic motor PWM, buzzer, offline fail-safe logic).
2. **Secondary MCU (ESP32-CAM)**: Dedicated to snapshot capture, JPEG compression, and Wi-Fi streaming to the AI cloud/companion service.

### Rationale
- **GPIO Starvation**: The OV2640 camera interface on ESP32-CAM uses almost all exposed GPIOs. Only GPIO 1, 3 (Serial), 0, 16, and sometimes 12–15 are accessible, with many sharing pull-up/boot functions.
- **Safety Criticality / Real-time Determinism**: Image capture and Wi-Fi TLS handshakes can block or jitter CPU execution for 300–1200 ms. A visually impaired user walking at 1 m/s would travel over a meter without obstacle/drop-off detection if the MCU hangs on camera I/O.
- **Power Decoupling**: The camera board can be powered down or placed into deep sleep when the user is not actively asking for scene description, conserving battery.

---

## ADR-002: Downward Drop-Off Sensor Choice (Ultrasonic vs. VL53L0X ToF)

### Status
**Configurable Abstraction / Proposed Default: HC-SR04 / US-015 Ultrasonic**

### Context
Detecting stairs descending, curbs, open drains, and drop-offs requires sensing ground distance at an angled orientation (~45° to 60° forward-downward).
Two sensor options were specified in the project brief:
1. Ultrasonic (e.g., HC-SR04, RCWL-1601, US-015, or waterproof JSN-SR04T).
2. Second ToF laser sensor (VL53L0X).

### Trade-offs & Ambiguities
- **Two ToFs on One I2C Bus**: Both VL53L1X and VL53L0X boot with the identical default I2C address (`0x29`). To run both on the same I2C bus, one sensor's `XSHUT` pin must be connected to an ESP32 GPIO to hold it in shutdown while the first sensor's address is reprogrammed via software on every boot. This requires extra wiring and boot coordination.
- **Surface & Ambient Light Reflection**: Downward ToF laser beams can absorb or scatter unpredictably on wet asphalt, black tile, puddles, or outdoor sunlight. Ultrasonic acoustic pulses reflect reliably off concrete, tiles, water surfaces, and curbs, and have a wide beam angle (~15°) that catches curbs and steps even if the cane sways.
- **Voltage Requirements**: Standard HC-SR04 requires 5V VCC and outputs a 5V echo pulse (needs resistor divider for 3.3V ESP32). Newer 3.3V-compatible sensors (US-015, RCWL-1601) operate directly on 3.3V.

### Decision
1. Implement a clean polymorphic / switchable driver abstraction `DownwardSensor` in firmware supporting both:
   - `UltrasonicSensor` (default pin mapping: Trig=18, Echo=5).
   - `TofDownSensor` (VL53L0X with `XSHUT` control on GPIO 19).
2. Hardware team can select the desired downward sensor using a single configuration flag (`#define DOWNWARD_SENSOR_TYPE SENSOR_TYPE_ULTRASONIC` or `SENSOR_TYPE_VL53L0X`) in `config.h`.

---

## ADR-003: GPIO Pin Mapping & Bus Allocation

### Status
**Accepted**

### Context
ESP32 DevKit has pin limitations: GPIOs 6–11 are connected to internal SPI flash; GPIOs 34, 35, 36, 39 are input-only without internal pull-ups; strapping pins (0, 2, 12, 15) must be handled carefully.

### Pin Allocation

| Subsystem | Component | Pin Name | ESP32 GPIO | Electrical Notes |
|:---|:---|:---|:---|:---|
| **I2C Bus** | Shared (VL53L1X, MPU6050, opt. VL53L0X) | `SDA` | **GPIO 21** | 3.3V logic; 4.7kΩ pull-up to 3.3V |
| **I2C Bus** | Shared (VL53L1X, MPU6050, opt. VL53L0X) | `SCL` | **GPIO 22** | 3.3V logic; 4.7kΩ pull-up to 3.3V |
| **Haptics** | ERM Vibration Motor | `PIN_VIBRATION_PWM` | **GPIO 25** | LEDC channel 0 (5 kHz PWM); driven via 2N2222/N-MOSFET with 1N4148 diode |
| **Audio** | Piezo Buzzer | `PIN_BUZZER` | **GPIO 26** | Active/Passive buzzer driver; LEDC tone / digital toggle |
| **Downward (US)** | Ultrasonic Trigger | `PIN_US_TRIG` | **GPIO 18** | Output; 10 µs trigger pulse |
| **Downward (US)** | Ultrasonic Echo | `PIN_US_ECHO` | **GPIO 5** | Input; 5V→3.3V resistor divider if using 5V HC-SR04 (1kΩ / 2kΩ) |
| **Downward (ToF)**| VL53L0X XSHUT | `PIN_TOF_DOWN_XSHUT` | **GPIO 19** | Output; used if second ToF sensor is populated to reprogram address to 0x30 |
| **Diagnostics** | Onboard / Cane LED | `PIN_LED_STATUS` | **GPIO 2** | Flashes on boot, steady when healthy, fast blink on fault |
| **Safety Button** | SOS / Mode Button | `PIN_BUTTON_SOS` | **GPIO 27** | Input with internal `INPUT_PULLUP` |

---

## ADR-004: Haptic Feedback Modulation & Drop-off Signature

### Status
**Accepted**

### Context
The user's hand needs intuitive, non-fatiguing tactile guidance. The cane must communicate two distinct conditions:
1. **Forward obstacles**: Distance proximity (continuous/graded).
2. **Drop-offs (descending stairs, curbs, holes)**: Immediate hazard requiring stopping.

### Decision
- **Forward Obstacle Range**: Continuous PWM intensity mapping:
  - Outside warning range (> 150 cm): **0% (Motor OFF)**.
  - Caution range (80 cm – 150 cm): Gentle proportional vibration (PWM 30% – 60%).
  - Warning range (30 cm – 80 cm): Moderate-to-high vibration (PWM 60% – 95%).
  - Critical range (< 30 cm): Full vibration (100% PWM) **plus** audio buzzer alert.
- **Drop-off Hazard Signature**:
  - A drop-off occurs when downward distance exceeds the calibrated ground baseline by more than threshold (e.g. baseline = 35 cm; detected distance > 55 cm).
  - Rather than steady PWM, drop-off triggers a distinctive **double-pulse rhythmic burst** (e.g. 150 ms ON, 80 ms OFF, 150 ms ON, 250 ms OFF).
  - This prevents confusion between "a wall is 40 cm ahead" and "a step down is immediately below".

---

## ADR-005: Sensor Library Selection

### Status
**Accepted**

### Selected Libraries:
1. **Forward ToF**: `pololu/VL53L1X@^1.3.1`
   - *Rationale*: Compact footprint, reliable ranging up to 4 meters, supports Short and Long distance modes, well tested on ESP32 without heavy STM32 HAL overhead.
2. **Downward ToF (optional)**: `pololu/VL53L0X@^1.3.1`
   - *Rationale*: Standard, compatible API with Pololu's VL53L1X, reliable address reassignment.
3. **IMU**: `adafruit/Adafruit MPU6050@^2.2.6` + `adafruit/Adafruit Unified Sensor@^1.1.14` + `adafruit/Adafruit BusIO@^1.16.1`
   - *Rationale*: Clean object-oriented interface, unit-normalized readings ($m/s^2$ and $rad/s$), reliable I2C register handling.
4. **Ultrasonic**: Custom lightweight, non-blocking timing driver
   - *Rationale*: Avoids blocking interrupts, provides timeout protection, and requires no external third-party library.

---

## ADR-006: Migration to ESP32-C3 SuperMini and HC-SR04 for Phase 1 Prototype

### Status
**Accepted**

### Context
During early hardware assembly and local component procurement:
1. VL53L1X and VL53L0X Time-of-Flight sensors were out of stock or prohibitively expensive locally.
2. The standard 30/38-pin NodeMCU ESP32 boards presented mechanical constraints (too bulky for an ergonomic cane handle) and bootstrapping/SPI flash pin contention issues during flashing.
3. The team selected the **ESP32-C3 SuperMini** as the primary microcontroller, paired with a reliable **HC-SR04** ultrasonic distance sensor.

### Trade-offs & Decisions
- **Form Factor**: The ESP32-C3 SuperMini (~22mm × 18mm) is dramatically smaller and lighter than standard devkits, allowing seamless integration inside the cane handle cavity.
- **Native USB-C Architecture**: The ESP32-C3 integrates native USB-JTAG/CDC directly on-chip, eliminating external USB-UART bridge driver issues and preventing flash-bus bootstrapping failures.
- **Ultrasonic Obstacle Sensing**: HC-SR04 provides reliable ranging from 2cm to 400cm, unhindered by ambient sunlight or dark surface absorption.
- **Pin Assignment for ESP32-C3**:
  - HC-SR04: `TRIG = GPIO 0`, `ECHO = GPIO 1` (Direct connection on bench prototype; 1kΩ/2kΩ divider recommended for production revision)
  - MPU6050: `SDA = GPIO 4`, `SCL = GPIO 5` (Hardware I2C at 400kHz)
  - Vibration Motor: `GPIO 6` (LEDC PWM at 200 Hz; driven directly from GPIO 6 on bench build; external 2N2222/MOSFET driver recommended for final production PCB)
  - Buzzer: `GPIO 7` (Universal Active/Passive driver)
  - Push Button: `GPIO 3` (`INPUT_PULLUP`; optional / unpopulated in bench build)
  - Status LED: `GPIO 8` (Onboard Blue LED, Active LOW)

### Consequences & Production Migration Path
- **Direct GPIO Motor Drive**: The vibration motor operates within safe bench testing limits without requiring an external transistor circuit. For commercial production, a discrete driver stage will be populated to prevent inductive EMF spikes and maximize tactile amplitude.
- **Direct Echo Signal**: The HC-SR04 Echo pin connects directly to GPIO 1 for bench testing. Production boards can implement a level shifter or adopt native 3.3V ultrasonic units (e.g. RCWL-1601).
- **Omission of Pushbutton & Orientation Reset**: The physical tactile pushbutton is omitted from the initial bench assembly. The firmware cleanly auto-clears and rearms the fall alarm once the user lifts the cane upright ($< 30^\circ\text{ tilt}$). A physical button can be connected to GPIO 3 as an optional hardware input.

---

## ADR-007: Smartphone-Bridged BLE Geolocation vs. Standalone Hardware GPS Module

### Status
**Accepted**

### Context
Initial architectural designs considered mounting a dedicated GPS receiver module (e.g. u-blox NEO-6M / NEO-8M) directly on the cane body with an onboard cellular or Wi-Fi transmitter to send geolocation coordinates to a family caregiver dashboard.

### Evaluation & Trade-offs
1. **Power Consumption**: A standalone GPS receiver draws ~50–80 mA continuously while tracking, plus additional power for a cellular modem or Wi-Fi transmission. This would drastically deplete the cane's compact battery within hours.
2. **Indoor Satellite Blind Spots**: Traditional GPS requires unobstructed line-of-sight to orbiting GNSS satellites. In typical daily usage (indoors, malls, bus shelters, subway stations), a standalone GPS fails to acquire a fix and incurs long 1–5 minute cold-start acquisition times.
3. **Weight, Ergonomics & BOM Cost**: Adding a GPS module and ceramic patch antenna increases physical cane handle weight and adds ~Rs. 2,800 to the bill of materials.
4. **Companion Smartphone Advantage**: Visually impaired cane users almost universally carry a smartphone running screen readers (VoiceOver, TalkBack). Smartphones provide instant, hybrid geolocation combining Assisted GPS (A-GPS), cellular tower multilateration, and Wi-Fi SSID trilateration that works seamlessly both indoors and outdoors.

### Decision
1. **Omit Standalone GPS Hardware**: Remove the dedicated GPS module from the cane hardware BOM.
2. **Implement BLE Geolocation Bridging**: Leverage the ESP32-C3's built-in Bluetooth Low Energy (BLE 5.0) subsystem to advertise and connect directly to the user's companion smartphone.
3. **Telemetry & Emergency Alert Bridging**:
   - The ESP32-C3 broadcasts real-time obstacle and fall status to the companion smartphone app over BLE.
   - When an emergency fall or SOS is detected, the smartphone app retrieves its high-precision geolocation and dispatches the alert and map pin to the caregiver dashboard over the phone's 4G/5G/Wi-Fi connection.

### Consequences
- **Cost Savings**: Eliminates ~Rs. 2,800 in unnecessary hardware components.
- **Extended Battery Life**: The ESP32-C3 BLE transmission operates with low power draw without heavy GPS RF processing.
- **Superior Geolocation Reliability**: Enables hybrid indoor/outdoor location fixes immediately without satellite cold-start delays.

