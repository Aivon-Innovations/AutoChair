# ESP32 Emergency-Stop (E-Stop) Standalone Bench Validation

This directory contains the standalone bench validation firmware, automated test harnesses, in-situ fault detection scripts, and official validation report for the **Normally-Closed (NC) Emergency-Stop Mushroom Switch** interfaced with an ESP32 WROOM-32.

---

## 📋 Directory Structure

```text
Bench Test/estop_testing/
├── src/
│   └── main.cpp                          # Standalone ESP32 E-Stop safety state machine firmware
├── platformio.ini                        # PlatformIO build configuration for ESP32
├── run_validation_suite.py               # Automated multi-test harness and telemetry recorder
├── estop_tester.py                       # Interactive CLI test utility
├── in_situ_fault_test.py                 # Open-circuit / wire-disconnect fault test script
├── estop_validation_data.json            # Recorded machine-readable telemetry dataset
├── ESP32_ESTOP_VALIDATION_REPORT.md      # Official validation report (All 7 Sections Passed)
└── README.md                             # This documentation
```

---

## ⚡ Electrical Wiring & Hardware Pinout

> [!NOTE]
> **Bench Validation Setup Only:** Driven purely via 5V USB from PC. No 24V wheelchair battery, motor, or power stage is connected.

* **Switch Model:** 22 mm ProMax Mushroom Emergency-Stop Switch
* **Contact Block Configuration:**
  * **Terminal 11 (NC):** Connected to ESP32 **GPIO32** (`INPUT_PULLUP`)
  * **Terminal 12 (NC):** Connected to ESP32 **GND**
  * **Terminals 23 / 24 (NO):** Unused
* **Debounce Filtering:** 25 ms software debounce interval in firmware.

### Electrical Logic Truth Table

| Physical Switch State | Contact State (11-12) | Electrical Path | GPIO32 Level | State Machine State |
| :--- | :--- | :--- | :--- | :--- |
| **RELEASED** (Normal) | Closed (Continuity) | Tied to GND | **LOW (0)** | `READY` (or Latched `SAFE` before reset) |
| **PRESSED** (E-Stop Active) | Open (No Continuity) | Pulled HIGH by internal pullup | **HIGH (1)** | `SAFE` |
| **DISCONNECTED** (Wire Break) | Open Circuit | Pulled HIGH by internal pullup | **HIGH (1)** | `SAFE` (Failsafe) |

---

## 🚀 How to Build & Run

### 1. Build & Flash Firmware
```bash
cd "Bench Test/estop_testing"
pio run --target upload
```

### 2. Run Automated Validation Harness
```bash
python3 run_validation_suite.py
```

### 3. Run In-Situ Fault Detection Test
```bash
python3 in_situ_fault_test.py
```

---

## 📊 Summary of Validation Results

| Test Section | Requirement | Result | Verdict |
| :--- | :--- | :---: | :---: |
| **1. Basic GPIO Detection** | Released = LOW (0), Pressed = HIGH (1) | Verified | **PASS** |
| **2. SAFE Transition** | `READY` &rarr; `SAFE` on switch actuation | Verified | **PASS** |
| **3. Explicit Reset-While-Pressed** | Block reset while physically pressed; unlatch after release & reset | Verified | **PASS** |
| **4. Command Rejection in SAFE** | `MOVE_FORWARD` rejected while in `SAFE` (5/5) | 100% Rejected | **PASS** |
| **5. SAFE Latch Behavior** | Remains in `SAFE` upon physical release until explicit reset (5/5) | 100% Latched | **PASS** |
| **6. NC Disconnection Fault** | Wire break detected as open-circuit `SAFE` | Verified | **PASS** |
| **7. Cycle Repetition & Recovery**| 10 full press/release/recovery cycles | 10/10 Cycles | **PASS** |

See [`ESP32_ESTOP_VALIDATION_REPORT.md`](ESP32_ESTOP_VALIDATION_REPORT.md) for full detailed test evidence and telemetry logs.
