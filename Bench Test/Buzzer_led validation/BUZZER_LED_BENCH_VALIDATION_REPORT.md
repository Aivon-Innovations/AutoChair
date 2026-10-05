# Active Buzzer and LED Bench Validation Report

**Project:** AutoChair Safety Subsystem Hardware Bringup  
**Validation Type:** Standalone Bench Hardware Test (Isolated Test Environment)  
**Date:** 2026-10-05  
**Platform:** ESP32-WROOM-32D DevKit (Silicon Revision v3.1)  
**Baud Rate:** 115200  
**Test Status:** **OVERALL PASS**

---

## 1. Executive Summary

This report documents the isolated, bench-level hardware validation of the **3.5–5.5 V Standard Active Buzzer Module for Arduino** and an **LED warning indicator** controlled by an ESP32 microcontroller.

All tests were performed strictly in an isolated bench environment without connecting to the wheelchair chassis, motor drivers, brakes, battery, or 24 V power systems.

Both the active buzzer module and the LED indicator were independently and concurrently validated across 20-cycle repetition suites with complete software telemetry, GPIO control verification, timing validation, and physical user observation.

---

## 2. Hardware Configuration & Pin Identification

### 2.1 Active Buzzer Module Identification
- **Component:** 3.5–5.5 V Standard Active Buzzer Breakout Module (Keyes KY-012 standard layout)
- **Rated Operating Voltage:** 3.5 V – 5.5 V DC
- **Drive Mode:** Resonant square wave drive (2.7 kHz active oscillation)
- **Silkscreen Pinout Verification:**
  - **Pin 1 (Top / marked `−`):** GND $\rightarrow$ Connected to **ESP32 GND**
  - **Pin 2 (Middle / unmarked):** VCC $\rightarrow$ Connected to **ESP32 5V (USB 5V Rail)**
  - **Pin 3 (Bottom / marked `S`):** Signal / I/O $\rightarrow$ Connected to **ESP32 GPIO33**
- **Transducer Polarity:** Round transducer face stamped with `(+)` polarity indicator

### 2.2 LED Indicator Circuit
- **Component:** 5mm Diffused Green LED
- **Series Current-Limiting Resistor:** 220 Ω (Color Code: Red-Red-Black-Black-Brown / Red-Red-Brown-Gold)
- **Control Line:** **ESP32 GPIO32**
- **Circuit Schematic:**
  $$\text{ESP32 GPIO32} \longrightarrow 220\,\Omega\text{ Resistor} \longrightarrow \text{LED Anode (+)} \longrightarrow \text{LED Cathode (−)} \longrightarrow \text{ESP32 GND}$$

---

## 3. Test Execution & Phase Results

### Part A — Buzzer Hardware Validation

1. **Step A1 & A2 (Pinout & Power):**
   - Buzzer supply connected to ESP32 USB 5 V rail (ensuring supply voltage $\ge 3.5\text{ V}$).
   - Signal line routed to isolated temporary GPIO33.
   - **Result: PASS**

2. **Step A4 & A5 (Polarity & Drive Frequency Determination):**
   - Direct DC HIGH vs DC LOW resulted in mechanical boundary clicking ("tick tick" sound).
   - Driving with a resonant 2.7 kHz square wave frequency produced a loud, crisp, audible warning tone.
   - **Result: PASS (Resonant Drive Active)**

3. **Step A6 & A7 (20-Cycle Repetition & Audible Confirmation):**
   - 20 repetition cycles executed (1.0 s ON @ 2.7 kHz / 1.0 s OFF silence).
   - Cycle Duration: $2005\text{ ms} \pm 15\text{ ms}$ per cycle.
   - Software Success Count: **20 / 20** (0 Failed).
   - User Physical Observation: **AUDIBLE (Nice, loud & clear)**.
   - **Result: PASS**

---

### Part B — LED Hardware Validation

1. **Step B1 (Wiring & Circuit Continuity):**
   - Circuit wired from GPIO32 $\rightarrow$ 220 Ω resistor $\rightarrow$ LED $\rightarrow$ GND.
   - Ground return connection established and verified.
   - **Result: PASS**

2. **Step B2 & B3 (20-Cycle Repetition & Illumination Confirmation):**
   - 20 repetition cycles executed (1.0 s ON / 1.0 s OFF).
   - Cycle Duration: $2000\text{ ms} \pm 10\text{ ms}$ per cycle.
   - Software Success Count: **20 / 20** (0 Failed).
   - User Physical Observation: **LED VISIBLE ON (Flashing 1s ON / 1s OFF during repetition sequence)**.
   - **Result: PASS**

---

### Part C — Combined Buzzer + LED Validation

1. **Synchronized 20-Cycle Repetition:**
   - Command sequence: Synchronized Buzzer ON + LED ON for 1000 ms, followed by Buzzer OFF + LED OFF for 1000 ms.
   - Repetitions: 20 continuous cycles.
   - Software Success Count: **20 / 20** (0 Failed).
   - Timing: $2015\text{ ms} \pm 15\text{ ms}$ per cycle.
   - Resets / Brownouts: 0.
   - User Physical Observation: **CONFIRMED (Buzzer beeping simultaneously with LED flashing 1s ON / 1s OFF in perfect synchrony, then turning off at the end of the test)**.
   - **Result: PASS**

---

## 4. Software Reliability & Stability Telemetry

| Reliability Metric | Observed Value | Threshold / Limit | Status |
| :--- | :--- | :--- | :--- |
| **CPU0 Reset Reason** | `1` (POWERON_RESET) | Expected Boot Reset | **PASS** |
| **CPU1 Reset Reason** | `14` (EXT_CPU_RESET) | Expected Clean Reset | **PASS** |
| **Watchdog Resets** | 0 | 0 | **PASS** |
| **Brownout Resets** | 0 | 0 | **PASS** |
| **Free Heap Memory** | 278,540 bytes | $> 100,000$ bytes | **PASS** (Zero Leaks) |
| **Firmware Crashes** | 0 | 0 | **PASS** |
| **Serial Communication**| 115200 baud, 100% telemetry capture | No packet drop | **PASS** |

---

## 5. Final Validation Matrix (Part G)

| Test Item | Verification Method | Result |
| :--- | :--- | :--- |
| **Buzzer identified** | Visual silkscreen & pinout analysis | **PASS** |
| **Buzzer physical wiring** | Isolated breadboard jumper connections | **PASS** |
| **Buzzer supply configuration** | USB 5V rail ($\ge 3.5\text{ V}$) | **PASS** |
| **ESP32 GPIO33 control** | Automated digital toggling & square wave modulation | **PASS** |
| **Buzzer logic identified** | Resonant Drive (2.7 kHz square wave) | **PASS** |
| **Buzzer ON operation** | Audible warning tone verified by user | **PASS** |
| **Buzzer OFF operation** | Complete silence phase verified by user | **PASS** |
| **Buzzer 20-cycle test** | Automated telemetry cycle count (20/20) | **PASS** |
| **LED physical wiring** | Series connection via 220 Ω resistor | **PASS** |
| **LED 220 Ω resistor** | Measured current-limiting in series | **PASS** |
| **ESP32 GPIO32 control** | Automated digital output control (HIGH/LOW) | **PASS** |
| **LED ON operation** | Physical light emission verified by user | **PASS** |
| **LED OFF operation** | Complete dark phase verified by user | **PASS** |
| **LED 20-cycle test** | Automated telemetry cycle count (20/20) | **PASS** |
| **Combined buzzer + LED test** | Synchronized 20-cycle output test (20/20) with simultaneous physical light & sound verification | **PASS** |
| **ESP32 stability** | Telemetry heap & CPU reset logging | **PASS** |
| **Overall bench validation** | All sub-criteria satisfied | **PASS** |

---

## 6. Constraints & Safety Limitations

- **Isolated Bench Test Only:** This test validates only the ESP32 GPIO electrical interfacing, signal modulation, and physical component functionality on a breadboard.
- **No Wheelchair Integration:** This test does NOT connect to or validate the AutoChair wheelchair chassis, motor drivers, electronic brake circuitry, 24 V power systems, or Safety Manager integration.
- **Temporary GPIOs:** GPIO33 (Buzzer) and GPIO32 (LED) are temporary bench-validation GPIOs and do not modify the production AutoChair firmware or hardware allocations.
- **Not an Emergency System Validation:** Passing this bench test does not constitute verification of emergency obstacle avoidance, automatic braking, or real-world wheelchair safety behavior.

---

## 7. Conclusion

### Overall Result: **PASS**

- **Buzzer Subsystem:**
  - Hardware: **PASS**
  - GPIO Control (GPIO33): **PASS**
  - Drive Mode: **Resonant 2.7 kHz Pulse Drive**
  - ON/OFF Operation: **PASS**
  - 20-Cycle Test: **20/20 PASS**
  - Physical Audible Confirmation: **PASS (Loud & Clear)**

- **LED Subsystem:**
  - Hardware: **PASS**
  - GPIO Control (GPIO32): **PASS**
  - Resistor: **220 Ω Verified**
  - ON/OFF Operation: **PASS**
  - 20-Cycle Test: **20/20 PASS**
  - Physical Illumination Confirmation: **PASS (Flashing 1s ON / 1s OFF)**

- **Combined Output Subsystem:**
  - Buzzer + LED Synchronization: **20/20 PASS**
  - Simultaneous Audible & Visual Emission: **PASS**
  - ESP32 Stability & Zero Leaks: **PASS**
