# AutoChair ESP32 — Pending Issues & Engineering Next Steps

**Document Identification:** `AutoChair-alvira-dev/encoder today/05_PENDING_ISSUES_AND_ENGINEERING_NEXT_STEPS.md`  
**Date:** 2026-10-23  
**Project:** AutoChair — AI-Powered Smart Wheelchair  
**Subsystem:** Encoder Subsystem Pending Actions & Technical Roadmap  
**Status:** ENGINEERING ROADMAP & TECHNICAL DEBT INVENTORY  

---

## 1. Operating Premise & Boundaries

**Important Engineering Directive:**  
The bench operator has formally decided not to perform any further PulseView or logic analyzer testing. Incoming engineers must respect this constraint. No further hardware testing, wiring changes, or manual bench rotations are required from the operator.

All unresolved items documented below are categorized strictly into:
1. **Work that can be completed immediately from existing code and logs.**
2. **Future specialist hardware characterization (for future lab phases).**

---

## 2. Category A: Work That Can Be Completed from Existing Files

The following engineering tasks require no physical hardware access and can be executed entirely on host workstations:

### 2.1 Documentation Consistency & Pin-Map Harmonization
* **Action:** Update all legacy markdown files (`docs/ENCODER_PHASE_VALIDATION_REPORT.md`, `esp32 documents/`, `Architecture/`) to reflect the verified Right Encoder pinout:
  * Channel A: **GPIO32** (Definitively replacing rejected GPIO16).
  * Channel B: **GPIO17**.
  * Channel Z: **GPIO14**.
* **Rationale:** Eliminates confusing references where legacy documents still mention GPIO16 or discuss only the Left encoder.

### 2.2 Explicit Separation of Measured vs. Assumed Calibration Constants
* **Action:** Review [`firmware/esp32/include/autochair_config.h`](file:///c:/Users/targa/OneDrive/Desktop/AutoChair-alvira-dev/AutoChair-alvira-dev/firmware/esp32/include/autochair_config.h) and annotate all kinematic and resolution constants with prominent warning comments:
  ```cpp
  // UNVERIFIED BENCH PLACEHOLDER: Nominal 600 PPR * 4 = 2400 CPR.
  // Physical calibration on the right wheel has not been performed.
  constexpr uint32_t ENCODER_RIGHT_PPR = 600;
  
  // UNVERIFIED MECHANICAL PLACEHOLDERS: PRD §5 nominal values.
  // Physical wheelchair wheel diameter and track width remain unmeasured.
  constexpr float ODOMETRY_WHEEL_DIAMETER_M = 0.320f;
  constexpr float ODOMETRY_TRACK_WIDTH_M     = 0.580f;
  constexpr float GEARBOX_NOMINAL_RATIO      = 23.3f;
  ```

### 2.3 Diagnostic Serial Capture Script Refactoring
* **Action:** Inspect and refactor all serial logger scripts in the repository (e.g. `Bench Test/Wheel encoder_validation/serial_runner.py` and diagnostic tools).
* **Fix:** Remove any calls to `ser.reset_input_buffer()` or `flushInput()` executed *after* motion begins. Implement non-blocking circular buffers or streaming pipe loggers so that no telemetry frames are discarded during handshake or startup.

### 2.4 Firmware Z-Channel Gating Logic Review
* **Action:** Design a firmware-level quadrature-gating guard for `handleIndex()`.
* **Concept:** Currently, `handleIndex()` only evaluates a 20 ms time interval. If the shaft hovers near an optical slit and rocks back and forth for $> 20\text{ ms}$, multiple index events fire.
* **Proposed Enhancement:** Add a minimum quadrature pulse delta guard (e.g. require $|\Delta\text{count}| \ge 2,000$ counts between accepted index pulses). This ensures an index event is only recognized after approximately one full physical revolution has elapsed, immune to boundary oscillation.

---

## 3. Category B: Future Specialist Hardware Work (Future Lab Phases)

When the project advances to the mechanical assembly and dynamometer test phase, the following physical investigations must be completed by lab specialists:

### 3.1 Direct Oscilloscope / Logic Analyzer Waveform Characterization
* **Instrument:** Digital Storage Oscilloscope (DSO, $\ge 100\text{ MHz}$) or USB Logic Analyzer ($\ge 24\text{ MSa/s}$).
* **Probe Points:** Right Encoder A (GPIO32), B (GPIO17), Z (GPIO14), and GND.
* **Required Measurements:**
  1. **Z-Index Pulse Width:** Measure the exact electrical duration of the index pulse across manual and motorized speeds.
  2. **Phase Alignment:** Verify whether Z is gated to A=1 and B=1 ($90^\circ$ window) or spans a full quadrature cycle ($360^\circ$).
  3. **Rise and Fall Times:** Verify signal transition times under 4.7 kΩ pull-ups to assess line capacitance.
  4. **Noise Margins:** Measure ground bounce and crosstalk while wheelchair motors are active.

### 3.2 Optical Disc Slit Architecture Verification
* **Objective:** Conclusively determine whether the Right encoder model has **1 index slit per revolution** or a multi-slit reference track.
* **Method:** Rotate the uncoupled encoder shaft slowly under a microscope or optical inspection setup while capturing a continuous logic analyzer trace to establish the exact number of Z rising edges per 360° mechanical rotation.

### 3.3 Empirical Counts-Per-Revolution (CPR) Calibration
* **Objective:** Measure the true quadrature resolution of the Right encoder.
* **Method:** Mount the encoder to a motorized calibration test fixture with a high-resolution reference optical standard (e.g. 10,000 PPR standard). Measure count delta strictly across consecutive index pulses over 100 continuous revolutions.

### 3.4 Full Dual-Wheel Simultaneous Validation
* **Objective:** Execute Tests 1 through 7 of [`docs/DUAL_ENCODER_INTEGRATION_VALIDATION_REPORT.md`](file:///c:/Users/targa/OneDrive/Desktop/AutoChair-alvira-dev/AutoChair-alvira-dev/docs/DUAL_ENCODER_INTEGRATION_VALIDATION_REPORT.md).
* **Scope:** Reconnect Left encoder signals (GPIO25, 26, 27) alongside Right encoder signals (GPIO32, 17, 14) and verify simultaneous, non-interfering dual-wheel tracking, differential direction changes, and dual-axis odometry integration.

---

## 4. Mechanical Mounting & Kinematic Dependencies

Before differential odometry ($x, y, \theta$) or linear velocity ($m/s$) can be connected to the navigation stack, the following mechanical unknowns must be resolved physically:

```text
┌─────────────────────────────────────────────────────────────────────────────┐
│                       UNKNOWN MECHANICAL PARAMETERS                         │
├───────────────────────────────────┬─────────────────────────────────────────┤
│ 1. Physical Mounting Location     │ Motor shaft vs Gearbox vs Wheel Hub     │
│ 2. Shaft Coupling & Rigidity      │ Direct drive, timing belt, or gear mesh │
│ 3. Wheel Nominal Diameter         │ Exact uncompressed wheel diameter       │
│ 4. Effective Rolling Radius       │ Loaded radius under occupant weight     │
│ 5. Track Width (Wheelbase)        │ Centerline distance between drive tires │
│ 6. Drivetrain Backlash            │ Gear lash and coupling play             │
│ 7. Tire Construction & Deflection │ Solid rubber, pneumatic, or urethane    │
│ 8. Right Encoder Mechanical Mount │ Symmetry and mounting alignment         │
└───────────────────────────────────┴─────────────────────────────────────────┘
```

> [!CAUTION]
> **Odometry Block:**  
> The nominal gearbox ratio of $23.3:1$ documented in early PRD notes means that mounting the encoder on the motor shaft produces $\approx 55,920\text{ counts/wheel rev}$, whereas mounting on the wheel hub produces $\approx 2,400\text{ counts/wheel rev}$. **Attempting closed-loop speed control or navigation before physical mounting geometry is confirmed will result in catastrophic odometry scaling errors.**
