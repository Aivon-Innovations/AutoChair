# ESP32 Emergency-Stop (E-Stop) Input Bench Validation Report

**Document ID:** `VAL-ESTOP-ESP32-001`  
**Test Subject:** ESP32 WROOM-32 Standalone E-Stop Input & Safety State Machine  
**Bench Pin Allocation:** GPIO32 (`INPUT_PULLUP`) — **BENCH VALIDATION ONLY**  
**Physical Operator:** Minaam  
**Test Automation & Verification:** Antigravity  
**Validation Date:** 2026-10-06  
**Final Verdict:** **PASS — All Defined Standalone Bench-Test Objectives Passed**  

---

## 1. Executive Summary & Test Verdict

This report documents the bench-level electrical input and firmware safety state-machine validation of a normally-closed (NC) emergency-stop mushroom switch interfaced with an ESP32 WROOM-32 microcontroller on **GPIO32** using an internal pull-up (`INPUT_PULLUP`).

All physical interactions (pushing the mushroom button, releasing/resetting the mechanism, disconnecting the wire) were performed manually on the bench by Minaam under explicit software protocol prompts. Antigravity managed firmware compilation, flashing, serial command execution, GPIO sampling, software debounce filtering, safety state transitions, command rejection, latch enforcement, recovery verification, and JSON telemetry logging.

### Overall Validation Verdict: **PASS — All Defined Standalone Bench-Test Objectives Passed**

| Test Section | Requirement | Executed / Recorded Evidence | Result | Status |
| :--- | :--- | :---: | :---: | :---: |
| **1. Basic GPIO Input Detection** | Released = LOW (0), Pressed = HIGH (1) | Verified during initial baseline and cycles | 100% Match | **PASS** |
| **2. SAFE Transition** | `READY` &rarr; `SAFE` on switch actuation | Verified across all cycles and explicit test | SAFE transition occurred after the configured 25 ms software debounce interval | **PASS** |
| **3. Explicit RESET-While-Pressed Test** | Block reset while physically pressed; unlatch only after physical release and explicit reset command | 1 dedicated explicit test (`reset_while_pressed_test`) | Reset rejected while pressed; recovery successful after release | **PASS** |
| **4. Command Rejection in SAFE** | `MOVE_FORWARD` rejected while in `SAFE` | 5 distinct recorded tests (`command_rejection_tests`) | 5/5 Rejected | **PASS** |
| **5. SAFE Latch Behavior** | Remains in `SAFE` after physical switch release | 5 distinct recorded latch tests (`safe_latch_tests`) | 5/5 Latch Maintained | **PASS** |
| **6. NC Disconnection Fault Detection** | Open-circuit detected as safe state | 1 dedicated in-situ test (`nc_fault_test`) | Detected as `SAFE`; latch held upon reconnect | **PASS** |
| **7. Cycle Repetition & Recovery** | Multi-cycle reliability and recovery verification | 10 full press/release cycles (`repetition_cycles`) | Recovery/reset was verified during the 10 repetition cycles, with successful explicit reset after release in each cycle | **PASS** |

---

## 2. Test Setup & Physical Hardware Configuration

### Hardware Inventory
* **Microcontroller:** ESP32 WROOM-32 Dev Module (powered strictly via 5V USB from PC)
* **Switch:** 22 mm ProMax Mushroom Emergency-Stop Switch
* **Contact Block Configuration:**
  * Terminal 11: Normally Closed (NC) contact &rarr; Connected to ESP32 **GPIO32**
  * Terminal 12: Normally Closed (NC) contact &rarr; Connected to ESP32 **GND**
  * Terminals 23 / 24: Normally Open (NO) &rarr; Unused
* **Power & Signal Isolation:** Standalone USB only. No 3.3 V, 5 V, VIN, or 24 V wheelchair battery/motor connection exists.
* **Firmware Configuration:** `pinMode(32, INPUT_PULLUP)` with a **25 ms software debounce interval**.

```
       +---------------------------------------------+
       |             ESP32 WROOM-32                  |
       |                                             |
       |  GPIO32 [INPUT_PULLUP] <----+               |
       |  (Bench Validation Only)    |               |
       |                             |               |
       |  GND --------------------+  |               |
       +--------------------------|--|---------------+
                                  |  |
                                  |  |  Terminals 11 & 12 (NC)
                        +---------v--v-----------+
                        |  22mm ProMax Mushroom  |
                        |      E-Stop Switch     |
                        +------------------------+
```

### Electrical Logic Truth Table
| Physical Switch State | Contact State (11-12) | Electrical Path | GPIO32 Level | State Machine State |
| :--- | :--- | :--- | :--- | :--- |
| **RELEASED** (Normal) | Closed (Continuity) | Tied to GND | **LOW (0)** | `READY` (or Latched `SAFE` before reset) |
| **PRESSED** (E-Stop Active) | Open (No Continuity) | Pulled HIGH by internal pullup | **HIGH (1)** | `SAFE` |
| **DISCONNECTED** (Wire Break) | Open Circuit | Pulled HIGH by internal pullup | **HIGH (1)** | `SAFE` (Failsafe) |

---

## 3. Detailed Test Results & Evidence

### 3.1 Basic GPIO Input Detection
* **Multimeter Continuity Check:** Verified closed circuit across terminals 11–12 when released; open circuit when pressed.
* **Firmware Software Sampling:** Sampled via `digitalRead(32)` through a 25 ms software debounce interval.
* **Measured Levels:**
  * Physical RELEASED &rarr; `RAW_PIN = 0` (`LOW`), `STABLE_PIN = 0`.
  * Physical PRESSED &rarr; `RAW_PIN = 1` (`HIGH`), `STABLE_PIN = 1`.
  * No spurious noise or unintended state transitions occurred during stationary states.

---

### 3.2 Explicit RESET-While-Pressed Rejection & Recovery Test
A dedicated, fully sequenced test was conducted and recorded as `reset_while_pressed_test` to verify that software safety recovery is strictly impossible while the physical switch remains open/pressed:

1. **Initial State:** Switch RELEASED &rarr; `GPIO32 = LOW (0)`, `SAFETY = READY`, `LATCHED = NO`.
2. **Switch Pressed:** Minaam pressed E-stop &rarr; `GPIO32 = HIGH (1)`, `SAFETY = SAFE`, `LATCHED = YES`.
   * *SAFE transition occurred after the configured 25 ms software debounce interval.*
3. **Reset Attempted While PRESSED:** Issued `CMD:RESET` while button remained pressed down.
   * **ESP32 Response:** `[RESPONSE] RESET: REJECTED (ESTOP_ACTIVE_OR_OPEN) | GPIO32=1 | SAFETY = SAFE`
   * **Verdict:** Reset was successfully blocked.
4. **Switch Released (Pre-Reset Check):** Minaam released E-stop &rarr; `GPIO32 = LOW (0)`, `SAFETY = SAFE`, `LATCHED = YES`.
   * **Verdict:** System correctly maintained the `SAFE` latch after physical release.
5. **Reset After Release:** Issued `CMD:RESET`.
   * **ESP32 Response:** `[RESPONSE] RESET: SUCCESS | GPIO32=LOW | SAFETY = READY`
   * **Verdict:** Successfully returned to `READY` state.
6. **Motion Verification:** Issued `CMD:MOVE_FORWARD`.
   * **ESP32 Response:** `COMMAND = MOVE_FORWARD | SAFETY = READY | RESULT = ACCEPTED`
   * **Verdict:** Normal motion command accepted only after valid recovery.

---

### 3.3 Command Rejection While in SAFE State (5 Tests)
While the system was in the latched `SAFE` state, five (5) consecutive motion commands (`CMD:MOVE_FORWARD`) were dispatched:

| Test # | Pre-Command State | Command Dispatched | ESP32 Firmware Response | Rejection Result |
| :---: | :---: | :---: | :--- | :---: |
| **1** | `SAFETY=SAFE, LATCHED=YES` | `CMD:MOVE_FORWARD` | `COMMAND = MOVE_FORWARD \| SAFETY = SAFE \| RESULT = REJECTED` | **PASS** |
| **2** | `SAFETY=SAFE, LATCHED=YES` | `CMD:MOVE_FORWARD` | `COMMAND = MOVE_FORWARD \| SAFETY = SAFE \| RESULT = REJECTED` | **PASS** |
| **3** | `SAFETY=SAFE, LATCHED=YES` | `CMD:MOVE_FORWARD` | `COMMAND = MOVE_FORWARD \| SAFETY = SAFE \| RESULT = REJECTED` | **PASS** |
| **4** | `SAFETY=SAFE, LATCHED=YES` | `CMD:MOVE_FORWARD` | `COMMAND = MOVE_FORWARD \| SAFETY = SAFE \| RESULT = REJECTED` | **PASS** |
| **5** | `SAFETY=SAFE, LATCHED=YES` | `CMD:MOVE_FORWARD` | `COMMAND = MOVE_FORWARD \| SAFETY = SAFE \| RESULT = REJECTED` | **PASS** |

---

### 3.4 SAFE Latch Behavior (5 Tests)
Five (5) distinct latch tests were executed to confirm that releasing the mushroom button does **not** auto-clear the `SAFE` state:

| Test # | Pre-Release State | Switch Action | Post-Release State | Latch Maintained | Reset Response | Final State | Verdict |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **1** | `RAW=1, SAFE` | Released (`RAW=0`) | `RAW=0, SAFE, LATCHED=YES` | **YES** | `RESET: SUCCESS` | `READY` | **PASS** |
| **2** | `RAW=1, SAFE` | Released (`RAW=0`) | `RAW=0, SAFE, LATCHED=YES` | **YES** | `RESET: SUCCESS` | `READY` | **PASS** |
| **3** | `RAW=1, SAFE` | Released (`RAW=0`) | `RAW=0, SAFE, LATCHED=YES` | **YES** | `RESET: SUCCESS` | `READY` | **PASS** |
| **4** | `RAW=1, SAFE` | Released (`RAW=0`) | `RAW=0, SAFE, LATCHED=YES` | **YES** | `RESET: SUCCESS` | `READY` | **PASS** |
| **5** | `RAW=1, SAFE` | Released (`RAW=0`) | `RAW=0, SAFE, LATCHED=YES` | **YES** | `RESET: SUCCESS` | `READY` | **PASS** |

---

### 3.5 NC Open-Circuit / Disconnection Fault Detection Test
To verify failsafe behavior against wire breaks or contact disconnections, Minaam disconnected the wire from terminal 11 on the bench:

1. **Terminal 11 Disconnected (Open Circuit Fault):**
   * GPIO32 was pulled HIGH via internal pullup (`RAW_PIN = 1`).
   * State Machine entered `SAFETY = SAFE` with `LATCHED = YES` after the 25 ms software debounce interval.
   * Motion command rejected: `COMMAND = MOVE_FORWARD | SAFETY = SAFE | RESULT = REJECTED`.
   * Reset command rejected: `[RESPONSE] RESET: REJECTED (ESTOP_ACTIVE_OR_OPEN) | GPIO32=1 | SAFETY = SAFE`.
2. **Terminal 11 Reconnected:**
   * GPIO32 returned to LOW (`RAW_PIN = 0`).
   * State Machine **remained latched in `SAFE`** across physical reconnection (`LATCHED = YES`).
3. **Recovery After Reconnection:**
   * Software `CMD:RESET` dispatched &rarr; `[RESPONSE] RESET: SUCCESS | GPIO32=LOW | SAFETY = READY`.
   * Motion command verified &rarr; `COMMAND = MOVE_FORWARD | SAFETY = READY | RESULT = ACCEPTED`.

*Note on Fault Coverage:* This test validates single NC open-circuit/disconnection fault detection. It does not imply detection of every possible electrical fault (such as a short circuit between terminal 11 and GND).

---

### 3.6 Repetition Cycles Summary (10 Cycles)
Ten (10) consecutive physical press/release cycles were performed to evaluate switch endurance, debounce stability, and recovery reliability. Recovery/reset was verified during the 10 repetition cycles, with successful explicit reset after release in each cycle.

| Cycle # | Pressed Level (GPIO32=1) | State &rarr; SAFE | Released Level (GPIO32=0) | Latch Maintained | Reset Response | Final State | Cycle Verdict |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **1** | PASS (`RAW=1`) | `SAFE` | PASS (`RAW=0`) | `SAFE (LATCHED)` | `SUCCESS` | `READY` | **PASS** |
| **2** | PASS (`RAW=1`) | `SAFE` | PASS (`RAW=0`) | `SAFE (LATCHED)` | `SUCCESS` | `READY` | **PASS** |
| **3** | PASS (`RAW=1`) | `SAFE` | PASS (`RAW=0`) | `SAFE (LATCHED)` | `SUCCESS` | `READY` | **PASS** |
| **4** | PASS (`RAW=1`) | `SAFE` | PASS (`RAW=0`) | `SAFE (LATCHED)` | `SUCCESS` | `READY` | **PASS** |
| **5** | PASS (`RAW=1`) | `SAFE` | PASS (`RAW=0`) | `SAFE (LATCHED)` | `SUCCESS` | `READY` | **PASS** |
| **6** | PASS (`RAW=1`) | `SAFE` | PASS (`RAW=0`) | `SAFE (LATCHED)` | `SUCCESS` | `READY` | **PASS** |
| **7** | PASS (`RAW=1`) | `SAFE` | PASS (`RAW=0`) | `SAFE (LATCHED)` | `SUCCESS` | `READY` | **PASS** |
| **8** | PASS (`RAW=1`) | `SAFE` | PASS (`RAW=0`) | `SAFE (LATCHED)` | `SUCCESS` | `READY` | **PASS** |
| **9** | PASS (`RAW=1`) | `SAFE` | PASS (`RAW=0`) | `SAFE (LATCHED)` | `SUCCESS` | `READY` | **PASS** |
| **10** | PASS (`RAW=1`) | `SAFE` | PASS (`RAW=0`) | `SAFE (LATCHED)` | `SUCCESS` | `READY` | **PASS** |

---

## 4. Observed Problems & Anomalies

* **Observed Anomalies:** None.
* **Debouncing Performance:** The 25 ms software debounce interval completely eliminated mechanical contact chatter without introducing perceptible latency.
* **Serial Interface Stability:** Serial DTR/RTS assertions were configured to prevent micro-reboots during host connection, confirming that state latches persist continuously in ESP32 memory.

---

## 5. Artifacts & Evidence Traceability Index

1. **Validation Report:** [ESP32_ESTOP_VALIDATION_REPORT.md](ESP32_ESTOP_VALIDATION_REPORT.md)
2. **Complete Recorded Evidence JSON:** [estop_validation_data.json](estop_validation_data.json)
3. **ESP32 Firmware Source:** [src/main.cpp](src/main.cpp)
4. **PlatformIO Build Configuration:** [platformio.ini](platformio.ini)
5. **Automated Validation Harness:** [run_validation_suite.py](run_validation_suite.py)
6. **In-Situ Fault Script:** [in_situ_fault_test.py](in_situ_fault_test.py)

---

## 6. Important Safety Limitation & Scope Boundary

> [!CAUTION]
> **SAFETY SCOPE LIMITATION:**
> This validation verifies the electrical E-stop input and ESP32 firmware safety-state behavior on a standalone bench setup. It does NOT validate actual wheelchair braking, motor shutdown, motor-controller behavior, mechanical safety, or production-level wheelchair safety.
>
> Specifically, this test:
> 1. Does **NOT** validate or claim high-power motor shutdown or power-stage isolation.
> 2. Does **NOT** validate wheelchair mechanical or electromagnetic brake engagement.
> 3. Does **NOT** certify wheelchair controller safety, 24 V power bus safety, or production vehicle certification.
> 4. Uses GPIO32 strictly as a **temporary bench validation pin**. It does not modify production AutoChair GPIO mappings and must not be merged into production firmware without full integration engineering.
