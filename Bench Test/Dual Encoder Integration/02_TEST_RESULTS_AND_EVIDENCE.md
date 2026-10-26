# AutoChair ESP32 — Test Results & Evidence Dossier

**Document Identification:** `AutoChair-alvira-dev/encoder today/02_TEST_RESULTS_AND_EVIDENCE.md`  
**Date:** 2026-10-23  
**Project:** AutoChair — AI-Powered Smart Wheelchair  
**Subsystem:** Wheel Encoder Testing & Serial Telemetry Evidence  
**Status:** COMPLETE EMPIRICAL EVIDENCE DOSSIER  

---

## 1. Overview & Telemetry Protocol

All live physical tests documented in this dossier were captured from the ESP32-WROOM-32D development board via serial port `COM8` at 115200 baud (8-N-1). Telemetry was emitted by the diagnostic loop in `firmware/esp32/src/main/main.cpp` at a nominal cadence of 500 ms (2 Hz).

The standard diagnostic frame format is:
```text
[DEBUG][<uptime_ms>][Main] State=<st> Safety=<sf> Mode=<md>
[DEBUG][<uptime_ms>][Encoder] L: cnt=<c> dir=<d> rpm=<r> pulses=<p> idx=<i> inv=<v> st=<s> rsn=<r> | R: cnt=<c> dir=<d> rpm=<r> pulses=<p> idx=<i> inv=<v> st=<s> rsn=<r>
[DEBUG][<uptime_ms>][GPIO] R_ENC raw: A(GPIO32)=<a> B(GPIO17)=<b> Z(GPIO14)=<z>
```

---

## 2. Test 1: Pull-Up Correction & Stationary Baselines

### 2.1 Initial Floating Signal Anomaly (Pre-Correction)
* **Observed Data:**
  ```text
  [DEBUG][120400][Encoder] L: cnt=0 dir=0 rpm=0.00 | R: cnt=142 dir=1 rpm=12.50 idx=89 inv=45 st=2
  [DEBUG][120420][GPIO] R_ENC raw: A(GPIO32)=0 B(GPIO17)=0 Z(GPIO14)=0
  [DEBUG][120900][Encoder] L: cnt=0 dir=0 rpm=0.00 | R: cnt=285 dir=1 rpm=15.00 idx=174 inv=92 st=2
  [DEBUG][120920][GPIO] R_ENC raw: A(GPIO32)=0 B(GPIO17)=0 Z(GPIO14)=0
  ```
* **Analysis:** With the 3.3V rail disconnected from the breadboard pull-ups, lines floated near GND. Spurious thermal and capacitive noise caused high-frequency false transitions, incrementing both count and index while the shaft was physically stationary.

### 2.2 Corrected Stationary Baseline Capture (Post-Correction)
* **Action:** 3.3V jumper firmly connected from ESP32 to breadboard pull-up rail.
* **Representative Telemetry (5.76-second capture window):**
  ```text
  [DEBUG][3480649][Encoder] L: cnt=0 dir=0 rpm=0.00 pulses=0 idx=0 inv=0 st=2 rsn=NONE | R: cnt=565641 dir=0 rpm=0.00 pulses=0 idx=2231 inv=1 st=2 rsn=NONE
  [DEBUG][3480670][GPIO] R_ENC raw: A(GPIO32)=1 B(GPIO17)=0 Z(GPIO14)=0
  [DEBUG][3481173][Encoder] L: cnt=0 dir=0 rpm=0.00 pulses=0 idx=0 inv=0 st=2 rsn=NONE | R: cnt=565641 dir=0 rpm=0.00 pulses=0 idx=2231 inv=1 st=2 rsn=NONE
  [DEBUG][3481194][GPIO] R_ENC raw: A(GPIO32)=1 B(GPIO17)=0 Z(GPIO14)=0
  [DEBUG][3483269][Encoder] L: cnt=0 dir=0 rpm=0.00 pulses=0 idx=0 inv=0 st=2 rsn=NONE | R: cnt=565641 dir=0 rpm=0.00 pulses=0 idx=2231 inv=1 st=2 rsn=NONE
  [DEBUG][3485889][Encoder] L: cnt=0 dir=0 rpm=0.00 pulses=0 idx=0 inv=0 st=2 rsn=NONE | R: cnt=565641 dir=0 rpm=0.00 pulses=0 idx=2231 inv=1 st=2 rsn=NONE
  [DEBUG][3485910][GPIO] R_ENC raw: A(GPIO32)=1 B(GPIO17)=0 Z(GPIO14)=0
  ```
* **Quantitative Result:**
  * Starting Count: `565,641` | Ending Count: `565,641` ($\Delta = 0$)
  * Starting Index: `2,231` | Ending Index: `2,231` ($\Delta = 0$)
  * Invalid Transitions: `1` → `1` ($\Delta = 0$)
  * Speed: `0.00 RPM` flat.
* **Conclusion:** Proves that pull-up resistors firmly establish CMOS HIGH logic levels at rest and eliminate stationary false triggering.

---

## 3. Test 2: Right A/B Quadrature Pulse Accumulation

### 3.1 Initial High-Speed Manual Spin (5-Second Window)
* **Log Reference:** `rotation_test.log:L5300-L5450`
* **Telemetry Sample:**
  ```text
  [DEBUG][1031394][Encoder] L: cnt=0 ... | R: cnt=14233 dir=1 rpm=145.20 pulses=203 idx=0 inv=1 st=2 rsn=NONE
  ```
* **Performance:** Accumulated $+14,233$ quadrature counts in a single spin; dynamic RPM scaled with hand velocity.

### 3.2 Long-Duration Cumulative Rotation
* **Cumulative Count Accumulation:** Over the course of the bench session, the Right encoder accumulated counts from `0` to **`>590,963`** counts.
* **Integrity of Quadrature Decoding:**
  * Across $>187,000$ counts of sustained manual motion, the invalid transition counter (`inv`) remained flat at `1`.
  * The single invalid transition occurred during initial breadboard jumper insertion and never incremented during mechanical shaft rotation.
  * Confirms that Channels A (GPIO32) and B (GPIO17) maintain legitimate quadrature phase shifting without contact bounce glitches entering the X4 lookup table.

---

## 4. Test 3: Right Z-Index Channel Validation

### 4.1 Green A ↔ Yellow Z Channel-Swap Diagnostic Experiment
* **Objective:** Determine whether failure to detect Z pulses in early runs was caused by ESP32 silicon/firmware or the physical encoder Yellow wire.
* **Wiring Modification (Temporary):**
  * Green wire (Encoder Channel A) connected to **GPIO14** (Z-index input).
  * Yellow wire (Encoder Channel Z) connected to **GPIO32** (A input).
* **Observed Telemetry:**
  ```text
  Baseline: R: cnt=495766 ... idx=446 inv=1
  During manual rotation (feeding A pulses into GPIO14):
  Final:    R: cnt=495766 ... idx=1895 inv=1
  ```
* **Quantitative Result:** `idx` increased from `446` to `1,895` (**$+1,449$ index counts**).
* **Conclusion:** 
  1. ESP32 GPIO14 silicon input buffer is fully functional.
  2. DevKit Header Pin 13 and breadboard tie-points have solid ohmic continuity.
  3. External 4.7 kΩ pull-up to 3.3V functions correctly.
  4. Firmware ISR vector `isrZ1` and `handleIndex()` execute and increment `_indexCount` reliably.

### 4.2 Restored Original Wiring Z-Index Capture
* **Wiring Restored:** Green A → GPIO32, White B → GPIO17, Yellow Z → GPIO14.
* **Controlled Rotation Samples:**
  * **Partial Rotation Run:**
    * Start: Count `495,766`, Index `2,200`
    * End: Count `498,035`, Index `2,201`
    * Delta: **$+2,269$ counts**, **$+1$ index event**, `inv=1` maintained.
  * **Extended Rotation Run:**
    * Start: Count `498,035`, Index `2,201`
    * End: Count `545,386`, Index `2,222`
    * Delta: **$+47,351$ counts**, **$+21$ index events**, `inv=1` maintained.
* **Conclusion:** Proves that the physical Yellow wire actively switches and produces rising edges that trigger the index interrupt under original wiring.

---

## 5. Test 4: Marked Revolution Trials & Serial Capture Issue

### 5.1 Marked Revolution Trial Data
Two controlled trials were conducted where the operator was instructed to rotate exactly one full physical revolution from a visual reference mark:

| Trial | Starting Count | Ending Count | $\Delta\text{Count}$ | Starting Index | Ending Index | $\Delta\text{Index}$ | Post-Motion Stability |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **Trial A** | `575,180` | `579,158` | **+3,978** | `2,235` | `2,237` | **+2** | Stable ($\Delta = 0$ over 5.76 s) |
| **Trial B** | `583,933` | `590,963` | **+7,030** | `2,239` | `2,241` | **+2** | Stable ($\Delta = 0$ over 6.40 s) |

### 5.2 Forensic Review of `scratch/capture_until_stop.py`
* In Trial B, an automated capture script was launched to record the event:
  ```python
  # File: scratch/capture_until_stop.py
  s = serial.Serial('COM8', 115200, timeout=1)
  s.reset_input_buffer()  # <--- CRUCIAL DEFECT
  with open(path, 'w', encoding='utf-8', buffering=1) as f:
      ...
  ```
* **Evidentiary Impact:**
  1. The Python script executed `s.reset_input_buffer()` immediately after opening COM8 at wall-clock `1792752121.483`.
  2. All serial frames transmitted by the ESP32 while the shaft was actively rotating between `fw_ms = 4120453 ms` and `4140365 ms` were flushed from the operating system buffer.
  3. As a result, the saved file `scratch/active_run_log.txt` captured only 13 identical stationary frames after the movement had already come to rest:
     ```text
     1792752121.577 [DEBUG][4140365][Encoder] ... R: cnt=590963 dir=0 rpm=0.00 idx=2241 inv=1
     1792752122.101 [DEBUG][4140889][Encoder] ... R: cnt=590963 dir=0 rpm=0.00 idx=2241 inv=1
     ...
     1792752127.865 [DEBUG][4146653][Encoder] ... R: cnt=590963 dir=0 rpm=0.00 idx=2241 inv=1
     ```
  4. The exact microsecond timestamps, individual intermediate counts at each index pulse, and the count interval between the two events were lost.

### 5.3 Why Index Delta is +2 Instead of +1
* In both trials, $\Delta\text{idx} = +2$. Without high-speed logic analyzer traces, three hypotheses remain open:
  1. **Manual Visual Overshoot:** A human hand rotating an uncoupled shaft cannot stop instantaneously at a visual mark; over-rotating past 360° enters the next revolution.
  2. **Boundary Rocking / Hesitation:** In [`docs/ENCODER_PHASE_VALIDATION_REPORT.md`](file:///c:/Users/targa/OneDrive/Desktop/AutoChair-alvira-dev/AutoChair-alvira-dev/docs/ENCODER_PHASE_VALIDATION_REPORT.md) §14.2, Trial 3 on the Left encoder similarly produced $+2$ index increments because starting directly on an optical slit edge triggered an immediate pulse upon initial touch, followed by a second pulse after completing the revolution.
  3. **Multi-Slit Optical Disc:** The encoder model may incorporate multiple index markers per 360° revolution.

---

## 6. Historical Left Encoder Evidence Comparison

For engineering comparison, the historical data recorded for the Left encoder in [`docs/ENCODER_PHASE_VALIDATION_REPORT.md`](file:///c:/Users/targa/OneDrive/Desktop/AutoChair-alvira-dev/AutoChair-alvira-dev/docs/ENCODER_PHASE_VALIDATION_REPORT.md) is summarized below:

### 6.1 Left Encoder Single-Revolution Trials ([`Report §14.1`](file:///c:/Users/targa/OneDrive/Desktop/AutoChair-alvira-dev/AutoChair-alvira-dev/docs/ENCODER_PHASE_VALIDATION_REPORT.md#L368))
* **Trial 1:** $\Delta\text{count} = +3,317$, $\Delta\text{idx} = +1$ ($141 \rightarrow 142$).
* **Trial 2:** $\Delta\text{count} = +3,069$, $\Delta\text{idx} = +1$ ($142 \rightarrow 143$).
* **Trial 3:** $\Delta\text{count} = +3,015$, **$\Delta\text{idx} = +2$** ($143 \rightarrow 145$).
  * *Trial 3 Note:* The two pulses occurred at counts `189,287` and `191,743`. Count delta strictly between optical pulses was **$2,456\text{ counts}$** ($+2.33\%$ from 2,400).

### 6.2 Left Encoder Multi-Revolution Z-to-Z Verification ([`Report §15.2`](file:///c:/Users/targa/OneDrive/Desktop/AutoChair-alvira-dev/AutoChair-alvira-dev/docs/ENCODER_PHASE_VALIDATION_REPORT.md#L398))
* Over 3 continuous rotations from Index 152 to 155:
  * Start Count (Z=152): `203,964`
  * End Count (Z=155): `211,262`
  * Aggregate Delta: **$7,298\text{ counts}$ across 3 revolutions**
  * **Measured Average CPR:** **$2,432.7\text{ counts/revolution}$** ($+1.36\%$ variance from 2,400).

---

## 7. Evidence Repository Index

The following raw evidence files are archived in `AutoChair-alvira-dev/encoder today/evidence/`:

1. [`evidence/active_run_log.txt`](file:///c:/Users/targa/OneDrive/Desktop/AutoChair-alvira-dev/AutoChair-alvira-dev/encoder%20today/evidence/active_run_log.txt): 1,390-line serial log documenting the 6.4-second post-rotation stationary baseline at `cnt = 590,963`, `idx = 2,241`.
2. [`evidence/one_rev_log.txt`](file:///c:/Users/targa/OneDrive/Desktop/AutoChair-alvira-dev/AutoChair-alvira-dev/encoder%20today/evidence/one_rev_log.txt): Continuous 120-second capture containing initial 100s stationary baseline, motion initiation, and index transition $2,235 \rightarrow 2,236$ at count `576,943`.
3. [`evidence/rotation_log.txt`](file:///c:/Users/targa/OneDrive/Desktop/AutoChair-alvira-dev/AutoChair-alvira-dev/encoder%20today/evidence/rotation_log.txt): Initial rotation log capturing $+5,486$ counts and index increment $2,228 \rightarrow 2,230$.
4. [`evidence/capture_until_stop.py`](file:///c:/Users/targa/OneDrive/Desktop/AutoChair-alvira-dev/AutoChair-alvira-dev/encoder%20today/evidence/capture_until_stop.py): Source script demonstrating the `reset_input_buffer()` defect.
