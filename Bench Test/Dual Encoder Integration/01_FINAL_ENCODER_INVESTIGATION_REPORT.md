# AutoChair ESP32 — Final Encoder Investigation Report

**Document Identification:** `AutoChair-alvira-dev/encoder today/01_FINAL_ENCODER_INVESTIGATION_REPORT.md`  
**Date:** 2026-10-23  
**Project:** AutoChair — AI-Powered Smart Wheelchair  
**Company:** Aivon Innovations Pvt. Ltd.  
**Subsystem:** ESP32-WROOM-32D Firmware & Dual Optical Rotary Wheel Encoder Subsystem  
**Branch:** `alvira-dev`  
**Status:** COMPLETE INVESTIGATION & ENGINEERING HANDOVER  

---

## 1. Executive Summary & Purpose

This document provides a comprehensive, technically rigorous record of the wheel encoder investigation conducted on the AutoChair development bench. It consolidates all electrical measurements, firmware diagnostics, wiring configurations, physical test trials, and evidentiary evaluations completed to date.

The primary engineering objectives of this investigation were:
1. Resolve hardware and pin conflicts on the ESP32-WROOM-32D development board to support a dual-encoder subsystem.
2. Characterize the electrical and quadrature performance of the **Right Wheel Encoder**.
3. Investigate and diagnose the Right Encoder Channel Z (Index) behavior under stationary baselines and controlled rotations.
4. Establish clear empirical boundaries between what is proven by direct evidence, what remains unverified, and what must never be assumed.
5. Provide a self-contained engineering handover for incoming software and hardware engineers without requiring the bench operator to repeat previous testing.

---

## 2. Safety Boundaries & Operational Constraints

Throughout all testing, strict safety protocols were enforced:
* **High-Power Isolation:** Wheelchair motors, 24V motor controllers, chassis battery harnesses, and power relays remained **completely disconnected**.
* **Simulation Mode Enforced:** Firmware was executed exclusively in `WheelchairInterfaceMode::SIMULATION` (`Mode=0`) with `SafetyManager` in `IDLE` state (`State=3`).
* **Non-Destructive Testing:** No high-voltage injection or destructive electrical stress was applied.
* **Read-Only Code & Firmware Execution:** Following initial firmware upload, all diagnostics were performed strictly via read-only serial telemetry on `COM8` at 115200 baud without code modifications or configuration overwrites.
* **Physical Wheelchair Safety Boundary:** Passing firmware compilation, native host unit tests, and bench pulse accumulation **does NOT establish physical wheelchair safety**. Wheel calibration, odometry conversion, and closed-loop motor drive remain blocked pending mechanical integration.

---

## 3. Hardware & Software Under Test

### 3.1 Hardware Components
* **Microcontroller:** ESP32-WROOM-32D Development Board (38-pin, USB-C, CP2102 USB-to-UART bridge, Chip Revision 3.1, 4MB SPI Flash).
* **Wheel Encoders:**
  * **Reported Model:** Pro:Range 3806-OPTI-600-ABZ-OC Incremental Optical Rotary Encoders (claimed 600 pulses/revolution, NPN open-collector outputs, 5V–24V VCC rating).
  * **Left Encoder:** Pre-mounted on bench; disconnected during right-only testing.
  * **Right Encoder:** Integrated and evaluated during today's bench testing.
* **Passive Conditioning:** External $4.7\text{ k}\Omega \pm 5\%$ pull-up resistors connected between each signal line (A, B, Z) and the ESP32 3.3V power rail.
* **Host Workstation:** Windows 11 PC communicating via serial interface on `COM8` (115200 baud, 8-N-1).

### 3.2 Software & Firmware Architecture
* **Framework:** PlatformIO Core / Arduino ESP32 Framework (`framework = arduino`, `board = esp32dev`).
* **Architecture:** Decoupled multi-tier encoder stack:
  * `HardwareEncoderHAL`: Interrupt Service Routines (`isrA`, `isrB`, `isrZ`), 4-bit quadrature lookup table (`QUAD_TABLE[16]`), X4 decoding, and microsecond/millisecond index debounce filter.
  * `EncoderDriver`: Direction derivation, signed cumulative count, pulses per cycle, dynamic signed RPM calculation, and reset controls.
  * `EncoderManager`: Dual-wheel abstraction exposing synchronized `WheelEncoderData`.
  * `SafetyManager`: Health monitoring, stall detection, stale-data guards, coverage masks, and motion authorization.
  * `DifferentialOdometry`: Decoupled kinematic pose integration ($x, y, \theta$).

---

## 4. Chronological Investigation & Test History

### 4.1 Resolution of Hardware Pin Conflict (GPIO16 Rejection)
* **Problem:** Initial pin allocation assigned Right Encoder Channel A to `GPIO16`.
* **Failure Analysis:** In prior bench tests ([`docs/ULTRASONIC_PHASE_VALIDATION_REPORT.md`](file:///c:/Users/targa/OneDrive/Desktop/AutoChair-alvira-dev/AutoChair-alvira-dev/docs/ULTRASONIC_PHASE_VALIDATION_REPORT.md) §4.2), connecting external circuitry to `GPIO16` triggered an unrecoverable ROM bootloader failure (`flash read err, 988`) and `esptool` communication failure (`ff ffff`). On ESP32-WROOM-32D, `GPIO16` shares internal lines with the SPI flash chip-select. Optical encoders with NPN open-collector outputs hold signal lines at GND whenever a slotted disc window aligns with the phototransistor (~50% duty cycle at rest). Powering up with `GPIO16` grounded locks the bootloader.
* **Resolution:** `GPIO16` was definitively rejected. Right Encoder Channel A was remapped to **`GPIO32`**, an ADC1 general-purpose I/O with hardware interrupts and zero boot-strapping or SPI flash dependencies.

### 4.2 Diagnosis and Rectification of Breadboard 3.3V Pull-Up Rail
* **Initial Observation:** Upon initial wiring of the Right encoder, raw pin snapshots reported `A=0, B=0, Z=0`, invalid transitions (`inv`) rapidly incremented into the hundreds while stationary, and spurious index pulses fired continuously.
* **Root Cause:** The external $4.7\text{ k}\Omega$ pull-up resistors were seated on a breadboard bus rail that was mechanically open-circuit / disconnected from the ESP32 3V3 header pin. Open-collector NPN outputs without pull-ups floated near GND, inducing electrical floating noise on high-impedance CMOS inputs.
* **Rectification:** The operator connected the breadboard distribution rail to the ESP32 3.3V header pin.
* **Result:** Signal lines pulled firmly HIGH (`A=1, B=1, Z=0` or `1` depending on optical slit alignment). Stationary count and index drift ceased immediately.

### 4.3 Right A/B Quadrature Validation
* **Mechanical Connection:** The operator seated the signal wire contacts firmly in the breadboard tie-points.
* **Observed Behavior:** Manual rotation produced sustained, monotonic count accumulation.
  * Initial test window: $+14,233$ counts in 5 seconds.
  * Cumulative testing: $>187,000$ quadrature counts logged.
  * Invalid transitions: Maintained strictly at `inv=1` (0 invalid state transitions added across all rotation sessions).
  * RPM tracking: Telemetry dynamically tracked manual speeds up to $>200\text{ RPM}$ and cleanly returned to `0.00 RPM` when stopped.

### 4.4 Right Z-Index Channel Investigation
* **Initial Anomaly:** Early rotation tests accumulated thousands of A/B counts while `idx` remained flat at 0.
* **Channel-Swap Diagnostic Experiment (Green A ↔ Yellow Z):**
  * To isolate whether the issue was in the ESP32 firmware/silicon or in the encoder's yellow wire, Green (A) was temporarily wired to GPIO14 (Z input) and Yellow (Z) to GPIO32 (A input).
  * Manual rotation immediately caused `idx` to count up by **$+1,449$ index counts**.
  * **Proof:** This proved conclusively that ESP32 GPIO14 silicon, DevKit header pin 13, breadboard pull-up, the `isrZ1` interrupt vector, and telemetry dispatch were 100% operational.
* **Restoration to Intended Wiring:** The operator restored Green A → GPIO32 and Yellow Z → GPIO14.
* **Subsequent Z-Index Detection:** Under original wiring, physical rotation tests successfully triggered index increments:
  * A partial rotation incremented `idx` from `2,200` to `2,201` ($\Delta\text{count} = +2,269$).
  * A subsequent rotation produced `idx` $2,201 \rightarrow 2,222$ ($\Delta\text{idx} = +21$, $\Delta\text{count} = +47,351$).
  * A later controlled rotation produced `idx` $2,228 \rightarrow 2,230$ ($\Delta\text{idx} = +2$, $\Delta\text{count} = +5,486$).
  * Two subsequent marked rotation trials each produced $\Delta\text{idx} = +2$.

### 4.5 Analysis of the Marked Revolution and Serial Capture Buffer Issue
* In the latest controlled marked-revolution test, two trials yielded:
  * Trial A: Count `575,180` → `579,158` ($\Delta\text{count} = +3,978$), Index `2,235` → `2,237` ($\Delta\text{idx} = +2$).
  * Trial B: Count `583,933` → `590,963` ($\Delta\text{count} = +7,030$), Index `2,239` → `2,241` ($\Delta\text{idx} = +2$).
* **Serial Capture Disconnect:**
  * Investigation of `scratch/capture_until_stop.py` revealed that line 8 executed `ser.reset_input_buffer()` upon opening COM8.
  * Because the operator began moving during or immediately prior to the connection handshake, intermediate 500 ms telemetry frames were discarded by the buffer flush.
  * The resulting log `scratch/active_run_log.txt` contained only 13 identical post-rotation stationary frames (`cnt = 590,963`, `idx = 2,241`).
  * Telemetry logged at 2 Hz cannot capture sub-millisecond optical transitions or identify the exact physical start and stop coordinates of a manual turn.

---

## 5. Reconciliation: Left Encoder vs. Right Encoder Evidence

A strict distinction must be maintained between the two wheel encoders:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                   LEFT ENCODER vs. RIGHT ENCODER EVIDENCE                   │
├───────────────────────────────┬─────────────────────────────────────────────┤
│ Left Wheel Encoder (GPIO 25,26,27) │ Right Wheel Encoder (GPIO 32,17,14)     │
├───────────────────────────────┼─────────────────────────────────────────────┤
│ 1. Tested on macOS test bench │ 1. Tested on Windows bench (COM8)           │
│ 2. AA161 / PulseView reported │ 2. NO direct logic-analyzer capture exists  │
│ 3. 600 PPR / 2,400 CPR stated │ 3. Exact CPR/PPR uncalibrated / unproven    │
│ 4. Measured avg: 2,432.7 cpr  │ 4. Only 2 Hz ESP32 telemetry recorded       │
│ 5. CW (dir=1) & CCW (dir=-1)  │ 5. CW (dir=1) observed; CCW polarity unrun  │
│ 6. Z-index multi-rev verified │ 6. Z increments observed; 1 Z/rev unproven  │
└───────────────────────────────┴─────────────────────────────────────────────┘
```

> [!WARNING]
> Empirical measurements from the Left encoder (such as $2,432.7\text{ counts/rev}$ or verified CCW phase order) **must NOT be transferred or assumed for the Right encoder** without independent direct measurement.

---

## 6. PulseView Logic Analyzer Search & Operator Decision

### 6.1 Exhaustive File Search Summary
An exhaustive search was conducted across the workspace, parent directories, and host profile:
* `Get-ChildItem -Path C:\Users\targa\ -Filter '*.sr' -Recurse` → **0 files found**.
* `Get-ChildItem -Path C:\Users\targa\ -Filter '*.sal' -Recurse` → **0 files found**.
* `Get-ChildItem -Path C:\Users\targa\ -Filter '*.vcd' -Recurse` → **0 files found**.
* Recursive image scan (`*.png`, `*.jpg`) in repository → **0 waveform screenshots found**.

Historical records in [`docs/ENCODER_PHASE_VALIDATION_REPORT.md`](file:///c:/Users/targa/OneDrive/Desktop/AutoChair-alvira-dev/AutoChair-alvira-dev/docs/ENCODER_PHASE_VALIDATION_REPORT.md) confirm that the prior PulseView/AA161 investigation was conducted exclusively on the Left encoder on a macOS system (`/Users/admin/Desktop/AutoChair/`). The raw `.sr` files were not preserved in the git tree.

### 6.2 Operator Decision
**The operator has formally decided not to perform any further PulseView or logic analyzer testing.**  
This decision is respected. All conclusions in this handover are based strictly on available documentary evidence, firmware unit tests, and recorded serial telemetry. No further hardware testing is requested from the operator.

---

## 7. Evidence Limitations

1. **Telemetry Sampling Rate:** ESP32 diagnostic telemetry streams at 2 Hz (every 500 ms). An optical index slit passing at $>30\text{ RPM}$ lasts $<2\text{ ms}$, rendering raw GPIO polling snapshots (`Z=0` or `1`) incapable of verifying pulse width or waveform cleanliness.
2. **Manual Visual Alignment:** Rotating an encoder shaft by hand to visual pencil/tape marks introduces substantial visual parallax, acceleration variations, and stopping overshoot.
3. **Absence of Raw Traces:** Without digital storage oscilloscope (DSO) or logic analyzer (LA) captures for the Right encoder, analog signal parameters (rise time, fall time, voltage high/low thresholds, ground bounce) remain unmeasured.

---

## 8. Master Validation Status Table

| # | Validation Item | Evidence Reference | Result | Detailed Engineering Status |
| :-: | :--- | :--- | :-: | :--- |
| **1** | **3.3V Pull-Up Rail Correction** | `rotation_test.log:L10-L40` | **PASS** | Connecting 3.3V to the breadboard distribution rail eliminated floating noise and stabilized CMOS levels. |
| **2** | **Right A/B Electrical Activity** | Serial telemetry logs | **PASS** | Over 187,000 counts accumulated dynamically during manual rotation; optical channels actively switch. |
| **3** | **Right A/B Quadrature Validity** | `rotation_test.log`, `active_run_log.txt` | **PASS** | Firmware state machine maintained `inv=1` (0 invalid transitions added across $>187,000$ counts). |
| **4** | **Right CW/CCW Phase Order** | Firmware telemetry | **PARTIAL** | Forward rotation (`dir=1`, positive RPM) verified; controlled reverse (`dir=-1`) polarity check remains unexecuted. |
| **5** | **Right Z-Index Interrupt Path** | Swap test log, COM8 telemetry | **PASS** | Physical switching on Yellow wire triggers GPIO14 hardware interrupt and increments `_indexCount`. |
| **6** | **Exactly 1 Z Pulse / Mechanical Rev** | `one_rev_log.txt`, `active_run_log.txt` | **NOT PROVEN** | Marked trials produced $+2$ index increments; single-slit vs multi-slit vs mechanical overshoot remains unproven. |
| **7** | **Right Encoder CPR / PPR** | Serial telemetry logs | **NOT PROVEN** | Theoretical 600 PPR / 2,400 CPR is not empirically calibrated on the Right encoder. |
| **8** | **Z Pulse Width & Phase Alignment** | No LA capture on Right | **NOT PROVEN** | Sub-millisecond pulse duration and alignment relative to A/B quadrature states are unmeasured. |
| **9** | **Stationary Baseline Stability** | Step 1 baseline capture | **PASS** | Completely flat counters across multiple stationary intervals (22.5s, 5.76s, 6.4s with 0 count/index drift). |
| **10** | **Signal Noise Immunity (Loaded)** | Lab bench only | **NOT PROVEN** | Immunity against motor PWM noise, EMI, and chassis vibration under actual wheelchair load is unverified. |
| **11** | **Native Firmware Unit Tests** | `task5_completion_report.md` | **PASS** | 119 / 119 native unit tests passed (0 failures) covering HAL, drivers, odometry, and safety fault-injection. |
| **12** | **ESP32 Firmware Compilation** | PlatformIO build output | **PASS** | Clean build (`pio run -e esp32dev`), 0 errors, 0 warnings (RAM: 7.2%, Flash: 23.6%). |
| **13** | **Firmware Upload & Boot** | COM8 boot log | **PASS** | Flashed via esptool; ESP32 boots cleanly into `State=3` (IDLE), `Safety=0` (Clear), `Mode=0` (Simulation). |
| **14** | **Dual-Wheel Simultaneous Operation** | Bench wiring state | **NOT PROVEN** | Left encoder was disconnected during Right encoder bench testing; concurrent dual-wheel test is pending. |
| **15** | **Full Physical Wheelchair Safety** | Architecture PRD | **NOT PROVEN** | Uncalibrated odometry, unverified mounting geometry, and disconnected power stages preclude safety claims. |

---

## 9. Next Steps for Incoming Engineering Team

Because the bench operator will not perform further hardware tests or logic-analyzer captures, the engineering team must proceed along two distinct tracks:

### Track 1: Work That Can Be Completed Immediately from Existing Files
1. **Reconcile Documentation:** Update all project documents to reference `GPIO32` for Right Channel A and harmonize Left vs. Right evidence.
2. **Isolate Calibration Constants:** Ensure nominal calibration values (`600 PPR`, `2,400 CPR`, `23.3:1 gearbox ratio`) are marked as uncalibrated placeholders in code comments.
3. **Refactor Serial Capture Tools:** Remove `reset_input_buffer()` from diagnostic scripts to prevent discarding transition frames.
4. **Firmware Z-Channel Gating Review:** Investigate gating the Z-index interrupt in firmware with quadrature counts (requiring $\approx 2,000+$ counts before accepting a subsequent index event) to reject boundary chatter.

### Track 2: Future Specialist Hardware Work (When Hardware Re-Enters Lab)
1. **Direct Waveform Characterization:** Probe Right Encoder pins (GPIO32, 17, 14) with a bench oscilloscope or logic analyzer to capture raw rise times and pulse widths.
2. **Optical Disc Verification:** Rotate the uncoupled shaft slowly under instrumented observation to establish the physical number of index slits per revolution.
3. **Rigid Mechanical Fixturing:** Mount encoders in precision mechanical jigs or directly to wheelchair wheel hubs with calibrated reference encoders to measure true CPR.
4. **Dual-Wheel Concurrent Validation:** Reconnect Left encoder signals (GPIO25, 26, 27) and execute simultaneous dual-wheel validation (Tests 1–7 in [`docs/DUAL_ENCODER_INTEGRATION_VALIDATION_REPORT.md`](file:///c:/Users/targa/OneDrive/Desktop/AutoChair-alvira-dev/AutoChair-alvira-dev/docs/DUAL_ENCODER_INTEGRATION_VALIDATION_REPORT.md)).
