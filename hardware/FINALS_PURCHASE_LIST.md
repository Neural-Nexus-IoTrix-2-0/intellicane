# IoTrix 2.0 Finals: Materials Purchase & Power Guide

**Project:** IntelliCane (Neural-Nexus)  
**Track:** Track A — Embedded IoT System Development  
**Target Event Date:** 17 October 2026  
**Team:** Dulnith Liyanage, Thenul Sahansa, Chamith Chethana, Suneth Vidurasa  

---

## 1. System Architecture: Dual-MCU + Vision Pipeline

To maintain the safety guarantees demonstrated in Phase 1, we use a **two-microcontroller distributed architecture**:

```
                       ┌────────────────────────────────────────────────────────┐
                       │               5V Power Rail (Shared)                   │
                       └────────┬───────────────────────────────┬───────────────┘
                                │                               │
                     5V & GND   │                    5V & GND   │
                                ▼                               ▼
                 ┌───────────────────────────┐   ┌───────────────────────────┐
                 │  ESP32-C3 SuperMini       │   │  ESP32-CAM (OV2640)       │
                 │  (Primary Safety MCU)     │   │  (Vision & AI Subsystem)  │
                 └──────────────┬────────────┘   └──────────────┬────────────┘
                                │                               │
                 ┌──────────────┴────────────┐                  │ Wi-Fi Stream /
                 │ • HC-SR04 Ultrasonic      │                  │ Direct Snapshot
                 │ • MPU6050 Motion/Fall IMU │                  ▼
                 │ • 200 Hz Vibration Motor  │      ┌───────────────────────────┐
                 │ • Piezo Hazard Buzzer     │      │ Android App / AI Vision   │
                 │ • BLE NUS Telemetry       │      │ (Object Identification)   │
                 └───────────────────────────┘      └───────────────────────────┘
```

1. **Primary Safety Controller (ESP32-C3 SuperMini):** Runs the 100% offline, deterministic reflex loop (ultrasonic ranging, IMU tilt/fall sensing, haptic PWM, buzzer tone, and BLE telemetry). It never blocks or hangs on network I/O.
2. **Secondary Vision Controller (ESP32-CAM + OV2640):** Triggered on-demand (either when an obstacle enters the 1.0 m caution zone or when the user requests an audio scene description). Captures JPEG frames and streams them to the companion phone app for AI object recognition / text-to-speech.

---

## 2. Power Architecture: How to Power Both MCUs Reliably

### The Engineering Challenge: "Brownout Detector Triggered"
The ESP32-CAM is notorious for brownouts. During Wi-Fi transmission bursts and camera sensor capture, it draws **transient current spikes of up to 450 mA – 600 mA**. 
If powered from a weak 3.3V pin or an inadequate boost regulator, the supply line dips below 4.5V, causing the ESP32 onboard AMS1117-3.3 regulator to drop out and reset the board with a brownout panic.

### The Power Solution

```
 [ 18650 Li-ion Cell ] (3.7V - 4.2V, 2500-3000 mAh)
          │
          ▼
 [ TP4056 Type-C Charger + Protection (DW01A) ]
          │ (3.7V OUT)
          ▼
 [ Power Switch: Rocker/Slide ]
          │
          ▼
 [ High-Current 5V Boost Converter (MT3608 / 2A Booster) ]
          │
          ├──────────────────────────────────────────────────────┐
          │                                                      │
          ▼                                                      ▼
 [ 1000 µF 16V Electrolytic Cap ]                      [ 5V Common Bus ]
 (Absorbs 500mA Wi-Fi camera spikes)                             │
                                        ┌────────────────────────┴────────────────────────┐
                                        ▼                                                 ▼
                              ESP32-C3 (5V Pin)                                 ESP32-CAM (5V Pin)
                              HC-SR04 (VCC 5V)                                  OV2640 Camera
                              Vibration Motor (5V)
```

### Power Specifications:
- **Total System Current Budget:**
  - ESP32-C3 (BLE active): ~90 mA
  - ESP32-CAM (Idle): ~180 mA
  - ESP32-CAM (Active Wi-Fi transmit + image capture): ~450 mA peak
  - HC-SR04 Ultrasonic: ~15 mA
  - Vibration Motor (Full PWM): ~100 mA
  - IMU & Buzzer: ~20 mA
  - **Total Continuous Current:** ~350 mA – 450 mA @ 5V
  - **Total Peak Spike Current:** **~850 mA – 1.0 A @ 5V**
- **Battery Runtime:**
  - With a single 2600 mAh 3.7V cell ($9.62\text{ Wh}$) and 85% boost conversion efficiency ($8.17\text{ Wh}$ usable):
  - Operating continuously at 400 mA average ($2.0\text{ W}$), the cane runs **over 4 hours of nonstop active camera streaming and guidance**.
  - With the camera placed into light sleep between detections, runtime extends to **8–12 hours**.

---

## 3. Materials Purchase List (What to Buy)

### Category A: Vision Subsystem
| # | Component | Recommended Model / Spec | Qty | Estimated Cost (LKR) | Sourcing & Critical Notes |
|:--|:---|:---|:---:|:---:|:---|
| 1 | **ESP32-CAM Module** | AI-Thinker ESP32-CAM with OV2640 2MP Camera | 1 | 2,200 – 2,600 | Make sure the OV2640 camera ribbon is included in the package. |
| 2 | **ESP32-CAM-MB Micro-USB Daughterboard** *(Highly Recommended)* | CH340 USB-to-UART shield that plugs directly under the ESP32-CAM | 1 | 800 – 1,100 | **Crucial:** Standard ESP32-CAM has NO USB port. The MB shield allows direct micro-USB flashing and power without fiddly FTDI jumpers. |
| *2a* | *FTDI USB-to-TTL Adapter (Alternative)* | *FT232RL or CH340 3.3V/5V USB-UART adapter* | *1* | *750 – 950* | *Only needed if ESP32-CAM-MB shield is out of stock.* |

### Category B: Power & Regulation Subsystem
| # | Component | Recommended Model / Spec | Qty | Estimated Cost (LKR) | Sourcing & Critical Notes |
|:--|:---|:---|:---:|:---:|:---|
| 3 | **18650 Li-ion Battery** | 3.7V, 2500 mAh – 3000 mAh (Genuine grade or high-drain) | 1–2 | 1,200 – 1,800 ea | Buy from a reputable shop (avoid ultra-cheap fake 9900mAh cells). |
| 4 | **18650 Battery Holder** | Single-cell (or dual-cell 1S2P parallel) with solder tabs/leads | 1 | 150 – 250 | Dual-cell holder gives double runtime and lower voltage sag. |
| 5 | **TP4056 Type-C Charger with Protection** | Board with DW01A + 8205A chips (`B+`, `B-`, `OUT+`, `OUT-`) | 1–2 | 250 – 350 | **Must have 4 output pads** (separate battery pads and load pads) for safe charging. |
| 6 | **5V 2A DC-DC Step-Up Boost Module** | **MT3608** (adjustable) OR **Fixed 5V 2A Boost Module** | 1 | 300 – 450 | Boosts 3.7V Li-ion to solid 5.0V. If using MT3608, turn trimpot counter-clockwise until output reaches exactly 5.0V before connecting. |
| 7 | **Electrolytic Decoupling Capacitor** | **1000 µF / 16V** (or 2× 470 µF / 16V) Low-ESR | 2 | 50 – 100 | **MANDATORY for ESP32-CAM:** Placed across 5V and GND rail to stop Wi-Fi brownout resets. |
| 8 | **Ceramic Capacitor** | 100 nF (0.1 µF) ceramic (code 104) | 2 | 20 – 40 | High-frequency noise suppression across 5V and GND. |
| 9 | **Miniature Power Switch** | SPST / SPDT Rocker or Slide Switch (rated >1A) | 1 | 50 – 100 | Physical master power ON/OFF switch. |

### Category C: Solid Circuit Prototyping & Interconnects (Replacing Breadboard)
| # | Component | Recommended Model / Spec | Qty | Estimated Cost (LKR) | Sourcing & Critical Notes |
|:--|:---|:---|:---:|:---:|:---|
| 10 | **Double-Sided FR4 Perfboard / Proto-Board** | 5×7 cm or 4×6 cm green plated through-hole PCB | 2 | 250 – 350 | Solid mounting base for all modules and sockets. |
| 11 | **Female Pin Header Strips** | 2.54 mm pitch (40-pin breakable) | 2 strips | 120 – 180 | Solder to perfboard; socket both ESP32-C3 and ESP32-CAM so they can be unplugged easily. |
| 12 | **Male Pin Header Strips** | 2.54 mm pitch (40-pin breakable) | 1 strip | 80 – 120 | For jumper taps and testing probe points. |
| 13 | **Screw Terminal Blocks** | 2-pin & 3-pin, 2.54 mm or 5.08 mm pitch | 4 | 200 – 300 | Heavy-duty connections for battery leads, switch, and motor. |
| 14 | **Flexible Silicone Stranded Wire** | 26 AWG or 28 AWG (Red, Black, Yellow/Blue) | 2 meters | 250 – 350 | Stiff solid-core jumpers snap under movement; stranded silicone wire never fatigues. |
| 15 | **Heat Shrink Tubing Assortment** | 2 mm, 3 mm, 5 mm, 8 mm | 1 pack | 150 – 250 | Clean electrical insulation for soldered joints. |

### Category D: Cane Chassis & Mechanical Mounts (Holding off 3D Enclosure)
| # | Component | Recommended Model / Spec | Qty | Estimated Cost (LKR) | Sourcing & Critical Notes |
|:--|:---|:---|:---:|:---:|:---|
| 16 | **White Cane / Walking Stick Chassis** | 4-section folding white cane (or lightweight aluminum cane) | 1 | 2,000 – 3,500 | Sourced from local medical supply, pharmacy, or Daraz Express. |
| 17 | **Heavy-Duty Zip Ties & Velcro Straps** | Small & medium black cable ties | 1 pack | 150 – 250 | Securely strapping the perfboard, battery, and sensor harness to the cane shaft while holding off on 3D printing. |
| 18 | **Double-Sided Foam Tape / Acrylic Mounting Tape** | 3M VHB or heavy-duty foam tape | 1 roll | 250 – 400 | Vibration-dampened mounting of the sensor bracket and board. |

---

## 4. Total Estimated Budget Breakdown

| Subsystem | Estimated Cost Range (LKR) |
|:---|:---:|
| **Vision Subsystem (ESP32-CAM + MB Daughterboard)** | LKR 3,000 – 3,700 |
| **Power Subsystem (18650 Battery + Holder + TP4056 + Boost + Caps + Switch)** | LKR 2,200 – 3,100 |
| **Soldering & Wiring Hardware (Perfboard, Headers, Terminals, Silicone Wire)** | LKR 1,000 – 1,500 |
| **Cane Chassis & Fasteners (White Cane + Ties + Tape)** | LKR 2,400 – 4,000 |
| **Total Estimated Expenditure** | **LKR 8,600 – 12,300** |

*Even with the AI camera and rechargeable power subsystem added, the complete multi-sensor smart cane costs less than LKR 15,000 total—well within student project accessibility.*

---

## 5. Local Sourcing Locations (Fast Sri Lankan Delivery / Walk-In)

Since the finals are on **17 October 2026**:
1. **TRONIC.LK (Nugegoda / Online):** Stock items: ESP32-CAM with MB board, TP4056 Type-C with protection, MT3608 boost, 18650 holders, pin headers.
2. **LankaTronix (Maharagama / Online):** Fast dispatch for sensors, perfboards, silicone wires, capacitors, and terminal blocks.
3. **MicroP.lk / Scioneers:** Excellent stock of genuine 18650 cells and ESP32 camera modules.
4. **First Cross Street, Pettah:** Walk-in shopping for 18650 batteries, rocker switches, heat shrink, zip ties, and perfboards immediately.
5. **Medical Supply / Surgical Stores (Maradana / Colombo):** Over-the-counter standard 4-section folding white mobility canes.

---

## 6. Critical Implementation Rules & Pitfall Prevention

### Rule 1: Always Power ESP32-CAM via its 5V Pin (Never 3.3V)
The ESP32-CAM board has an onboard 3.3V low-dropout regulator (AMS1117-3.3) dedicated to the ESP32 chip and OV2640 camera. Feed clean **5.0V** into the `5V` pin on the ESP32-CAM. Do not attempt to run it directly from the 3.7V battery into the 3.3V pin—it will brown out immediately.

### Rule 2: Install the 1000 µF Reservoir Capacitor
Solder the **1000 µF 16V electrolytic capacitor** directly between the 5V and GND pins of the ESP32-CAM (observing polarity: negative stripe to GND). This guarantees that sudden 500 mA Wi-Fi current bursts draw from the capacitor instead of causing a voltage dip on the main line.

### Rule 3: Tie All Grounds Together
The ESP32-C3 ground, ESP32-CAM ground, TP4056 ground, boost converter ground, and all sensors must be connected to a single common `GND` bus.

### Rule 4: Disable Camera Flash LED in Firmware
The ESP32-CAM onboard bright white flash LED is wired to `GPIO 4`. In firmware, ensure `GPIO 4` is kept `LOW` (`digitalWrite(4, LOW)`) unless an intentional picture strobe is required; leaving it on draws 150 mA of heat and wastes battery.
