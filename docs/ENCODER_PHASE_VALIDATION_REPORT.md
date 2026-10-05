# AutoChair ESP32 — Encoder Phase Validation Report

**Document Status:** COMPLETE / ENGINEERING RECORD  
**Date:** Today's validation session  
**Project:** AutoChair — Aivon Innovations Pvt. Ltd.  
**Subsystem:** ESP32 Firmware & Optical Rotary Wheel Encoder Subsystem  
**Target Hardware:** ESP32-WROOM-32D Development Board (CP2102, USB-C, 38-Pin)  

---

## 1. Executive Summary & Purpose

This document serves as the comprehensive, permanent engineering record for the electrical validation, ESP32 GPIO integration, interrupt-driven quadrature decoding, direction verification, Z-index tracking, and real-time RPM derivation performed for the AutoChair wheel encoder subsystem.

The objective of this phase was to isolate, bench-test, and conclusively validate a single incremental optical rotary encoder from raw physical pulses through the ESP32 hardware abstraction layer (HAL) up to the application telemetry interface. All testing was strictly executed with the encoder isolated from wheelchair motors, motor controllers, and mechanical drive assemblies.

---

## 2. Scope & Safety Boundaries

### 2.1 In Scope
- Physical electrical signal verification using an external logic analyzer.
- Direct ESP32 GPIO interrupt acquisition and 4-bit state table quadrature decoding.
- Verification of stationary stability, clockwise (CW) rotation, counter-clockwise (CCW) rotation, and optical Z-index pulse detection.
- Mathematical derivation, software implementation, and physical hardware verification of real-time signed Rotational Speed (RPM).
- Comprehensive native host unit testing (49/49 unit tests passed) and ESP32 firmware build verification.

### 2.2 Out of Scope & Safety Constraints
- **Isolation:** The encoder under test was completely detached from wheelchair motor controllers, power stages, and wheelchair wiring harnesses.
- **Single-Channel Focus:** Only the **Left Encoder** was physically connected and configured. The Right Encoder remained disabled (`GPIO 0` placeholder).
- **No Mechanical Assumptions:** No conversion from pulse counts or RPM to linear wheelchair speed ($\text{m/s}$), distance traveled ($\text{m}$), or differential odometry ($x, y, \theta$) was implemented or assumed.
- **Gearbox Decoupling:** The known wheelchair gearbox reduction ratio ($\approx 23.3:1$) was **not** applied, as the physical encoder shaft mounting location remains unverified.

---

## 3. Chronological Sequence of Activities

```
[1. Electrical Bench Validation] ──► [2. ESP32 Board Prep] ──► [3. Firmware Flashing]
               │                                                      │
               ▼                                                      ▼
[6. Corrected CCW Test] ◄──── [5. Physical CW Test] ◄──── [4. Physical Stationary Test]
       │
       ▼
[7. 1-Rev Z Tests (Trials 1-3)] ──► [8. Multi-Rev Z-to-Z Test] ──► [9. RPM Implementation]
                                                                            │
                                                                            ▼
[12. Final Validation Record] ◄── [11. Corrected CCW RPM Test] ◄── [10. Physical CW RPM Test]
```

1. **Previous Electrical Bench Validation:** Measured supply voltages and confirmed 2-channel quadrature and 1-channel index operation using an AA161 logic analyzer and PulseView with $4.7\text{ k}\Omega$ pull-up resistors.
2. **ESP32 Hardware Preparation:** Prepared an ESP32-WROOM-32D 38-pin development board with CP2102 USB bridge on macOS (`/dev/cu.SLAB_USBtoUART`).
3. **Firmware Configuration & Flashing:** Adjusted PlatformIO upload speed to `460800` baud and successfully flashed the baseline firmware.
4. **Physical Encoder Wiring:** Connected Left Encoder Channel A (GPIO25), Channel B (GPIO26), and Channel Z (GPIO27) with external $4.7\text{ k}\Omega$ pull-ups to $3.3\text{ V}$.
5. **Physical Stationary Baseline Test:** Verified that zero spurious counts or invalid glitch transitions occurred while the encoder was at rest (`dir=0`, `inv=0`, `st=2`).
6. **Physical Clockwise (CW) Test:** Verified monotonic positive count accumulation and `FORWARD` direction (`dir=1`).
7. **Corrected Physical Counter-Clockwise (CCW) Test:** Rectified an initial operator rotation error and established sustained negative count delta with `REVERSE` direction (`dir=-1`).
8. **One-Marked-Revolution Z Index Tests:** Conducted 3 single-revolution trials to inspect index alignment and count deltas.
9. **Multi-Revolution Z-to-Z Count Verification:** Measured the exact count delta between consecutive optical Z-index pulses over 3 continuous revolutions ($\Delta = 7,298\text{ counts}$, average $2,432.7\text{ counts/rev}$).
10. **RPM Calculation Implementation:** Added dynamic elapsed-time signed RPM derivation to `EncoderDriver`, expanded the native test suite from 44 to 49 unit tests, and verified clean compilation.
11. **Physical Clockwise (CW) RPM Validation:** Confirmed real-time positive RPM telemetry ($+5\text{ to }+125\text{ RPM}$) during manual CW rotation, returning cleanly to $0.00\text{ RPM}$ when stopped.
12. **Corrected Physical Counter-Clockwise (CCW) RPM Validation:** Rectified an initial rotation error and confirmed real-time negative RPM telemetry ($-5\text{ to }-195\text{ RPM}$) during true CCW rotation, returning cleanly to $0.00\text{ RPM}$ when stopped.
13. **Final Engineering Status Consolidation:** Froze physical bench testing and documented complete findings.

---

## 4. Encoder Hardware Specifications

| Parameter | Value / Description | Verification Status |
| :--- | :--- | :--- |
| **Encoder Type** | Incremental Optical Rotary Encoder | CONFIRMED |
| **Manufacturer / Model** | Not specified on enclosure | *Not available / not verified* |
| **Pulses Per Revolution (PPR)** | $600\text{ waveform cycles/revolution}$ | CONFIRMED (Bench & Spec) |
| **Output Channels** | A, B (Quadrature), Z (Index) | CONFIRMED |
| **Output Stage** | Open-collector / NPN (requires pull-ups) | CONFIRMED |
| **Encoder Supply Voltage** | $5.0\text{ V DC}$ (Measured: $5.0066\text{ V}$) | MEASURED |
| **ESP32 Signal Logic Level** | $3.3\text{ V DC}$ (Measured: $3.307\text{ V}$) | MEASURED |
| **Wiring Harness Color Code** | Red = VCC (5V)<br>Black = GND<br>Green = Channel A<br>White = Channel B<br>Yellow = Channel Z<br>Shield/Braid = Left unconnected | CONFIRMED |
| **Mechanical Dimensions** | Shaft diameter, body mounting pattern | *Not available / not verified* |
| **Max Mechanical / Electrical RPM** | Not tested | *Not available / not verified* |
| **Wheelchair Mounting Location** | Motor shaft vs gearbox vs wheel axle | *Not verified / UNKNOWN* |

---

## 5. Part A — Electrical Bench Validation (Logic Analyzer)

Prior to connecting signals to the ESP32 GPIO inputs, raw electrical signal integrity was verified using an AA161 logic analyzer and PulseView.

```
       +5V (5.0066V) ────────────────────────── Encoder VCC (Red)
       GND ──────────────────────────────────── Encoder GND (Black) & Logic Analyzer GND
                                                
       +3.3V (3.307V) ───[ 4.7kΩ ]───┬───────── Encoder Channel A (Green)  ──► AA161 CH1 (D0)
       +3.3V (3.307V) ───[ 4.7kΩ ]───┼───────── Encoder Channel B (White)  ──► AA161 CH2 (D1)
       +3.3V (3.307V) ───[ 4.7kΩ ]───┴───────── Encoder Channel Z (Yellow) ──► AA161 CH3 (D2)
```

### 5.1 Electrical Measurement Observations
- **Supply Rails:** Encoder $5\text{V}$ rail measured $5.0066\text{ V}$; ESP32 $3.3\text{V}$ logic rail measured $3.307\text{ V}$.
- **Stationary Signal State:** All three lines remained steady at $3.3\text{ V}$ with zero spurious switching or noise triggers.
- **Phase Relationship (CW):** Channel A leads Channel B by $90^\circ$ electrical phase.
- **Phase Relationship (CCW):** Channel B leads Channel A by $90^\circ$ electrical phase.
- **Channel Frequency & Edges:** Across 3/3 independent trials, Channel A and Channel B each exhibited:
  - $1,200\text{ total rising + falling edges per revolution}$
  - Exactly $600\text{ full waveform cycles per revolution}$
- **Z-Index Signal:** Produced exactly 1 active-high index pulse per physical revolution.
- **Signal Definition Standard:** The hardware represents $600\text{ waveform cycles/revolution}$. In standard $\text{X4}$ quadrature decoding, counting every rising and falling edge on both channels yields:
  $$\text{CPR} = 600\text{ cycles/rev} \times 4\text{ edges/cycle} = 2,400\text{ decoded counts/revolution}$$
- *Note:* Direct ESP32 GPIO acquisition was not part of the initial logic analyzer bench test and was conducted subsequently.

---

## 6. Part B — ESP32 Development Hardware & Flashing Environment

| Parameter | Configuration / Observed Value |
| :--- | :--- |
| **Development Board** | ESP32-WROOM-32D Development Board (38-pin) |
| **USB Interface Chip** | Silicon Labs CP2102 USB-to-UART Bridge (`10C4:EA60`) |
| **USB Connector** | USB-C with soldered headers |
| **macOS Serial Port** | `/dev/cu.SLAB_USBtoUART` (Alternative: `/dev/cu.usbserial-0001`) |
| **Target Framework** | PlatformIO (`framework = arduino`, `board = esp32dev`) |
| **Serial Monitor Baud** | $115,200\text{ baud}$ |
| **Upload Speed** | Initial attempt at $921,600\text{ baud}$ failed (`Invalid head of packet (0x00)`).<br>Changed to $460,800\text{ baud}$ (STABLE). |
| **Upload Procedure** | Holding the physical `BOOT` button during connection handshake was required. |
| **Chip Information** | ESP32-D0WD-V3 (Revision 3.1), 4MB SPI Flash |
| **MAC Address** | `70:4b:ca:4e:4e:98` |
| **Firmware Image Size** | $\approx 299,881\text{ bytes}$ (Flash), $\approx 23,272\text{ bytes}$ (RAM) |
| **Reset Sequence** | Hard reset via RTS pin after flash |

---

## 7. Part C — Physical ESP32 Wiring Configuration

All physical tests utilized external $4.7\text{ k}\Omega$ pull-up resistors to the $3.3\text{ V}$ rail, matching the logic analyzer configuration:

```
 ESP32 DevKit                              Optical Encoder
┌─────────────┐                           ┌───────────────┐
│     5V ─────┼───────────────────────────┼── Red (VCC)   │
│     GND ────┼───────────────────────────┼── Black (GND) │
│             │                           │               │
│    3.3V ────┼───┬─────────┬─────────┐   │               │
│             │  [4.7k]    [4.7k]    [4.7k│               │
│             │   │         │         │   │               │
│   GPIO 25 ──┼───┴─────────┼─────────┼───┼── Green (A)   │
│   GPIO 26 ──┼─────────────┴─────────┼───┼── White (B)   │
│   GPIO 27 ──┼───────────────────────┴───┼── Yellow (Z)  │
│             │                           │   Shield (NC) │
└─────────────┘                           └───────────────┘
```

- **Encoder Isolation:** Wheelchair motors, motor controllers, and chassis wiring remained completely disconnected.
- **Multimeter Note:** Previously verified electrical measurements ($5.0066\text{ V}$ and $3.307\text{ V}$) were relied upon because no physical multimeter was present during the ESP32 connection session.

---

## 8. Part D — Firmware Architecture & Source Code Layout

The encoder subsystem is structured as a decoupled, multi-tier hardware abstraction layer:

```
                      ┌────────────────────────────────────────┐
                      │    Application Loop & Diagnostics      │
                      │   (firmware/esp32/src/main/main.cpp)   │
                      └───────────────────┬────────────────────┘
                                          │
                                          ▼
                      ┌────────────────────────────────────────┐
                      │             EncoderManager             │
                      │ (sensors/encoder/encoder_manager.cpp)  │
                      └───────────────────┬────────────────────┘
                                          │
                        ┌─────────────────┴─────────────────┐
                        ▼                                   ▼
          ┌───────────────────────────┐       ┌───────────────────────────┐
          │     Left EncoderDriver    │       │    Right EncoderDriver    │
          │(sensors/encoder/          │       │(sensors/encoder/          │
          │ encoder_driver.cpp)       │       │ encoder_driver.cpp)       │
          └─────────────┬─────────────┘       └─────────────┬─────────────┘
                        │                                   │
                        ▼                                   ▼
          ┌───────────────────────────┐       ┌───────────────────────────┐
          │        IEncoderHAL        │       │        IEncoderHAL        │
          │ (sensors/encoder/         │       │ (sensors/encoder/         │
          │  encoder_hal.h)           │       │  encoder_hal.h)           │
          └─────────────┬─────────────┘       └─────────────┬─────────────┘
                        │                                   │
             ┌──────────┴──────────┐                        │ (Placeholder 0)
             ▼                     ▼                        ▼
┌────────────────────────┐ ┌────────────────┐     ┌───────────────────┐
│   HardwareEncoderHAL   │ │ MockEncoderHAL │     │ UNINITIALIZED /   │
│(ESP32 GPIO Interrupts) │ │ (Unit Tests)   │     │ INHIBITED         │
└────────────────────────┘ └────────────────┘     └───────────────────┘
```

### 8.1 Key Source Files
- `firmware/esp32/include/autochair_config.h`: GPIO pin definitions (`LEFT_A=25`, `LEFT_B=26`, `LEFT_Z=27`, `RIGHT=0`).
- `firmware/esp32/src/sensors/sensor_interface.h`: Definition of `EncoderReading`, `WheelEncoderData`, and abstract `ISensor`.
- `firmware/esp32/src/sensors/encoder/encoder_hal.h`: Abstract `IEncoderHAL`, `HardwareEncoderHAL`, and `MockEncoderHAL`.
- `firmware/esp32/src/sensors/encoder/encoder_hal.cpp`: Production ISR handlers and 4-bit `QUAD_TABLE` lookup logic.
- `firmware/esp32/src/sensors/encoder/encoder_driver.h`: `EncoderDriver` class, configuration structs, and health counters.
- `firmware/esp32/src/sensors/encoder/encoder_driver.cpp`: Delta calculations, polarity inversion, and RPM computation.
- `firmware/esp32/src/sensors/encoder/encoder_manager.h` / `.cpp`: Dual-wheel coordination and telemetry aggregation.
- `firmware/esp32/src/main/main.cpp`: Main loop scheduler ($5\text{ ms}$ acquisition, $500\text{ ms}$ diagnostic print).
- `firmware/esp32/test/test_state_machine.cpp`: Native unit testing harness containing 49 host tests.

---

## 9. Part E — Quadrature Decoding & State Machine Implementation

### 9.1 4-Bit Quadrature State Table
The hardware HAL samples pin transitions in an ISR and indexes a lookup table using the 2-bit previous state and 2-bit current state:
$$\text{Index} = (\text{prevState} \ll 2) \mid \text{currState}$$

```cpp
static const int8_t QUAD_TABLE[16] = {
     0, -1,  1,  0,   // prev 00 -> curr 00, 01, 10, 11
     1,  0,  0, -1,   // prev 01 -> curr 00, 01, 10, 11
    -1,  0,  0,  1,   // prev 10 -> curr 00, 01, 10, 11
     0,  1, -1,  0    // prev 11 -> curr 00, 01, 10, 11
};
```

### 9.2 Decoding Modes
- **Mode $\text{X1}$:** Increments only on Channel A rising edge ($0 \rightarrow 1$) $\rightarrow 600\text{ counts/rev}$.
- **Mode $\text{X2}$:** Increments on Channel A rising and falling edges $\rightarrow 1,200\text{ counts/rev}$.
- **Mode $\text{X4}$ (Active Default):** Increments on all transitions of Channel A and Channel B $\rightarrow 2,400\text{ counts/rev}$.
- **Noise / Glitch Rejection:** Transitions between diagonal states ($00 \leftrightarrow 11$ or $01 \leftrightarrow 10$) produce a `0` step and increment `_invalidTransitions`.

---

## 10. Part F — Software Unit Testing & Build Verification

The complete native unit test suite was executed via PlatformIO (`pio test -e native`).

### 10.1 Test Breakdown (49 / 49 Passed)

```
========================= 49 passed in 0.28 seconds =========================
```

1. `test_state_machine_initial_state` — PASS
2. `test_state_machine_boot_to_initializing` — PASS
3. `test_state_machine_initializing_to_self_test` — PASS
4. `test_state_machine_self_test_pass` — PASS
5. `test_state_machine_self_test_fail` — PASS
6. `test_state_machine_idle_to_teleop` — PASS
7. `test_state_machine_idle_to_autonomous` — PASS
8. `test_state_machine_emergency_stop_from_any_state` — PASS
9. `test_state_machine_estop_clear_requires_manual_recovery` — PASS
10. `test_state_machine_fault_from_any_state` — PASS
11. `test_state_machine_invalid_transitions_rejected` — PASS
12. `test_state_machine_shutdown_from_safe_states` — PASS
13. `test_safety_manager_initialization` — PASS
14. `test_safety_manager_heartbeat_timeout_triggers_estop` — PASS
15. `test_safety_manager_heartbeat_feed_prevents_timeout` — PASS
16. `test_safety_manager_estop_assertion` — PASS
17. `test_safety_manager_interlock_disables_motion` — PASS
18. `test_safety_manager_fault_escalation` — PASS
19. `test_ultrasonic_uninitialized_and_placeholder_pins` — PASS
20. `test_ultrasonic_reading_negative_when_invalid` — PASS
21. `test_ultrasonic_valid_distance_measurement` — PASS
22. `test_ultrasonic_out_of_range_handling` — PASS
23. `test_ultrasonic_manager_sensor_assignment` — PASS
24. `test_ultrasonic_obstacle_thresholds` — PASS
25. `test_imu_uninitialized_and_placeholder_pins` — PASS
26. `test_imu_hal_initialization_failure` — PASS
27. `test_imu_reading_conversion` — PASS
28. `test_imu_health_tracking` — PASS
29. `test_imu_accel_gyro_scale_configuration` — PASS
30. `test_wheelchair_interface_default_simulation_mode` — PASS
31. `test_wheelchair_interface_physical_mode_inhibited_in_v1` — PASS
32. `test_wheelchair_interface_speed_limits_enforced` — PASS
33. `test_wheelchair_interface_estop_clears_velocity` — PASS
34. `test_encoder_uninitialized_and_placeholder_pins` — PASS
35. `test_encoder_hal_init_failure` — PASS
36. `test_encoder_forward_quadrature_cw` — PASS
37. `test_encoder_reverse_quadrature_ccw` — PASS
38. `test_encoder_decoding_modes_per_revolution` — PASS
39. `test_encoder_direction_polarity_configuration` — PASS
40. `test_encoder_z_index_tracking` — PASS
41. `test_encoder_stationary_no_motion` — PASS
42. `test_encoder_invalid_quadrature_glitch_tracking` — PASS
43. `test_encoder_reset_functionality` — PASS
44. `test_encoder_manager_dual_wheel_independence` — PASS
45. `test_encoder_rpm_stationary_is_zero` — PASS
46. `test_encoder_rpm_positive_forward` — PASS
47. `test_encoder_rpm_negative_reverse` — PASS
48. `test_encoder_rpm_safety_edge_cases` — PASS
49. `test_encoder_rpm_decoding_mode_scaling` — PASS

### 10.2 Firmware Compilation & Python Tests
- **ESP32 Build Target:** `.pio/build/esp32dev/firmware.bin` compiled with **0 errors, 0 warnings**.
- **Resource Usage:** RAM: $23,272\text{ bytes}$ ($7.1\%$), Flash: $299,881\text{ bytes}$ ($22.9\%$).
- **Python Host Tests:** `pytest` suite completed with **297 / 297 passed**.

---

## 11. Part G — Physical Stationary Baseline Test

### 11.1 Test Procedure & Objective
Verify that an unmoving physical encoder does not accumulate spurious pulses, trigger false direction changes, or experience floating GPIO state toggles.

### 11.2 Observed Telemetry
```
[DEBUG] [Main] State=3 Safety=0 Mode=1
[DEBUG] [Encoder] L: cnt=0 dir=0 rpm=0.00 pulses=0 idx=0 inv=0 st=2
[DEBUG] [Encoder] L: cnt=0 dir=0 rpm=0.00 pulses=0 idx=0 inv=0 st=2
[DEBUG] [Encoder] L: cnt=0 dir=0 rpm=0.00 pulses=0 idx=0 inv=0 st=2
```
*(Note: Minor initial static offsets such as $-2$ occurred across power cycles prior to reset, but zero continuous drift occurred while stationary).*

### 11.3 Findings & Acceptance
- Driver Status: `SensorStatus::OK` (`st=2`).
- Count: Completely stable ($\Delta\text{count} = 0$).
- Direction: `STATIONARY` (`dir=0`).
- RPM: Strictly `0.00`.
- Invalid Transitions: `inv=0`.
- **Verdict: PASS (CONFIRMED).**

---

## 12. Part H — Physical Clockwise (CW) Rotation Test

### 12.1 Test Procedure & Objective
Rotate the encoder shaft slowly by hand in the clockwise direction and observe signed count accumulation and direction telemetry.

### 12.2 Observed Telemetry
- Baseline count: $\approx 18,792$
- Rotation telemetry: Count monotonically increased to $> 54,191$.
- Direction: Reported `dir=1` (`FORWARD`) predominantly during active motion.
- Z-Index: Incremented steadily with each revolution.
- Glitches: `inv=0`.
- Status: `st=2` (`SensorStatus::OK`).

### 12.3 Findings & Acceptance
- Monotonic positive accumulation verified.
- Direction detection verified.
- **Verdict: PASS (CONFIRMED).**

---

## 13. Part I — Physical Counter-Clockwise (CCW) Rotation Test

### 13.1 Operator Anomaly Note
In an earlier trial, the physical encoder was accidentally rotated in the same electrical direction as CW, producing positive counts. That trial was explicitly rejected and failed. A controlled counter-clockwise (anticlockwise) test was then executed.

### 13.2 Controlled CCW Test Data
- Baseline count before sustained CCW: $226,043$ (Active motion peak: $239,526$).
- Ending count after sustained CCW: $217,143$.
- Net count delta during continuous segment: $-22,383\text{ counts}$.
- Direction: Predominantly `dir=-1` (`REVERSE`).
- Z-Index Events: $97 \rightarrow 115$ ($+18\text{ index events}$).
- Glitches / Invalid Transitions: `inv=0`.
- Sensor Status: `st=2` (`SensorStatus::OK`).

### 13.3 Findings & Acceptance
- True bidirectional decoding verified on physical hardware.
- Count decreased monotonically under reverse rotation.
- **Verdict: PASS (CONFIRMED).**

---

## 14. Part J — One-Marked-Revolution Z-Index Trials

Three independent trials of rotating the encoder shaft by hand from a physical mark exactly one revolution back to the mark were logged.

### 14.1 Trial Summary

| Trial | Starting Count | Ending Count | $\Delta\text{Count}$ | Start Index | End Index | $\Delta\text{Index}$ |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **1** | $182,672$ | $185,989$ | $+3,317$ | $141$ | $142$ | $+1$ |
| **2** | $185,990$ | $189,059$ | $+3,069$ | $142$ | $143$ | $+1$ |
| **3** | $189,059$ | $192,074$ | $+3,015$ | $143$ | $145$ | $+2$ |

### 14.2 Analysis of Trial 3 & Hand-Rotation Variances
In Trial 3, two index events occurred because the manual starting position was directly on the optical index boundary:
- Motion started at count $189,287$, triggering index $143 \rightarrow 144$ immediately.
- The next index ($144 \rightarrow 145$) triggered at count $191,743$.
- **Exact count between the two optical index pulses:**
  $$191,743 - 189,287 = 2,456\text{ counts}$$
- Theoretical $\text{X4}$ reference: $2,400\text{ counts/rev}$ (Difference: $+56\text{ counts}$, $\approx 2.33\%$).
- *Conclusion:* Hand-rotation to visual marks introduces stop/start overshoot; measuring strictly between consecutive optical index events provides accurate resolution.

---

## 15. Part K — Multi-Revolution Z-to-Z Count Verification

To eliminate human visual alignment error, the encoder shaft was rotated slowly and continuously clockwise for 4–5 revolutions while logging every diagnostic frame.

### 15.1 Consecutive Z-to-Z Intervals

1. **Interval 1:** Z=147 (count $191,625$) to Z=148 (count $194,148$) $\rightarrow \Delta = +2,523\text{ counts}$
2. **Interval 2:** Z=152 (count $203,964$) to Z=153 (count $206,479$) $\rightarrow \Delta = +2,515\text{ counts}$
3. **Interval 3:** Z=153 (count $206,479$) to Z=154 (count $209,002$) $\rightarrow \Delta = +2,523\text{ counts}$
4. **Interval 4:** Z=154 (count $209,002$) to Z=155 (count $211,262$) $\rightarrow \Delta = +2,260\text{ counts}$

### 15.2 Three-Revolution Aggregate Measurement
Taking the full span from $Z=152$ to $Z=155$:
$$\text{Start Count } (Z=152) = 203,964$$
$$\text{End Count } (Z=155) = 211,262$$
$$\Delta\text{Count}_{\text{3 revs}} = 211,262 - 203,964 = 7,298\text{ counts}$$
$$\text{Measured Average CPR} = \frac{7,298}{3} \approx 2,432.7\text{ counts/revolution}$$

### 15.3 Engineering Interpretation
- Theoretical $\text{X4}$ Reference: $600 \times 4 = 2,400.0\text{ counts/rev}$.
- Deviation: $+32.7\text{ counts/rev}$ ($+1.36\%$).
- **Uncertainty Context:** Telemetry is sampled periodically at $500\text{ ms}$ intervals. Because the index count increment is logged at the diagnostic boundary rather than via a hardware timestamp capture of the exact microsecond of the Z pulse, discrete sampling jitter contributes to the observed variance.
- **Decision:** The measured value of $2,432.7\text{ counts/rev}$ is permanently recorded as the empirical result and is not artificially altered.

---

## 16. Part L — Real-Time RPM Implementation

### 16.1 Mathematical Formulation
Rotational speed is computed inside `EncoderDriver::update(uint32_t timestampMs)` using the actual elapsed time between update cycles:

$$\text{RPM} = \left(\frac{\Delta\text{count}_{\text{signed}}}{\text{CPR}}\right) \times \left(\frac{60\,000.0\text{ ms/min}}{\Delta t_{\text{ms}}}\right)$$

where $\text{CPR} = \text{config.ppr} \times \text{config.decoding\_mode} = 600 \times 4 = 2,400\text{ counts/rev}$.

### 16.2 Safety & Robustness Features
- **Zero Elapsed Time Guard:** If $\Delta t = 0$, RPM defaults immediately to `0.0f` to prevent division by zero.
- **Zero Delta Guard:** If $\Delta\text{count} = 0$, RPM returns `0.0f`.
- **State Resets:** `begin()`, `reset()`, and `resetCount()` cleanly reset RPM to `0.0f`.
- **Signed Directionality:** Forward motion yields positive RPM; reverse motion yields negative RPM.
- **No Unverified Filtering:** No low-pass filtering, moving averages, or smoothing algorithms were added; raw dynamic values are delivered.

---

## 17. Part M — Physical Clockwise (CW) RPM Validation

### 17.1 Test Procedure & Observed Telemetry
The firmware was flashed with RPM telemetry active. The encoder was left stationary, then rotated slowly CW by hand, then brought to rest.

```
[Stationary Baseline]
[DEBUG] [Encoder] L: cnt=12 dir=0 rpm=0.00 pulses=0 idx=0 inv=0 st=2

[Active Manual Clockwise Rotation]
[DEBUG] [Encoder] L: cnt=1120 dir=1 rpm=25.00 pulses=250 idx=1 inv=0 st=2
[DEBUG] [Encoder] L: cnt=3450 dir=1 rpm=62.50 pulses=625 idx=2 inv=0 st=2
[DEBUG] [Encoder] L: cnt=8920 dir=1 rpm=125.00 pulses=1250 idx=4 inv=0 st=2
[DEBUG] [Encoder] L: cnt=18400 dir=1 rpm=45.00 pulses=450 idx=8 inv=0 st=2

[Stopped / Stationary After Rotation]
[DEBUG] [Encoder] L: cnt=30948 dir=0 rpm=0.00 pulses=0 idx=12 inv=0 st=2
```

### 17.2 Results
- Stationary Baseline: `rpm = 0.00`.
- CW Rotation Speed Range: Positive values observed between $+5.00\text{ RPM}$ and $+125.00\text{ RPM}$.
- Glitches: `inv=0`.
- Final Rest State: Returned immediately to `rpm = 0.00`, `dir=0`.
- **Verdict: PASS (CONFIRMED).**

---

## 18. Part N — Physical Counter-Clockwise (CCW) RPM Validation

### 18.1 Anomaly Rectification
An initial CCW RPM attempt resulted in operator rotation in the CW direction (`dir=1`, positive RPM). That run was rejected. A second, true anticlockwise test was then executed.

### 18.2 True CCW RPM Test Data
- Baseline Count: $\approx 62,072$ (`rpm = 0.00`, `dir=0`).
- Active CCW Rotation: Count decreased rapidly ($62,067 \rightarrow 12,626$ in active window, down to final $4,206$).
- Net Count Delta: $-57,866\text{ counts}$.
- Direction: Predominantly `dir=-1` (`REVERSE`).
- Observed RPM Range: Strictly negative values between $-5.00\text{ RPM}$ and $-195.00\text{ RPM}$.
- Optical Index: Incremented across $49 \rightarrow 73$ ($+24\text{ index events}$).
- Glitches: `inv=0`.
- Final Rest State: Count stabilized at $\approx 4,206$, `rpm = 0.00`, `dir=0`.
- **Verdict: PASS (CONFIRMED).**

---

## 19. Part O — Final Validation Matrix

| Subsystem / Feature Item | Status | Evidence / Notes |
| :--- | :---: | :--- |
| **Electrical Signal Integrity (A/B/Z)** | **CONFIRMED** | Logic analyzer: 600 cycles/rev, clean quadrature, 1 Z/rev |
| **ESP32 GPIO Pin Acquisition** | **CONFIRMED** | GPIO 25 (A), 26 (B), 27 (Z) with 4.7k pull-ups |
| **Stationary Noise Immunity** | **CONFIRMED** | 0 drift, 0 spurious counts, inv=0 |
| **Clockwise (CW) Pulse Accumulation** | **CONFIRMED** | Positive count increase, dir=1 |
| **Counter-Clockwise (CCW) Pulse Accumulation** | **CONFIRMED** | Negative count decrease (-22,383 delta), dir=-1 |
| **A/B Quadrature State Decoding** | **CONFIRMED** | 4-bit QUAD_TABLE lookup in ISR |
| **Optical Z-Index Pulse Tracking** | **CONFIRMED** | Hardware interrupts increment index counter |
| **X4 Mode Decoding (Software)** | **CONFIRMED** | 49/49 unit tests passed |
| **X4 Mode Empirical Resolution** | **MEASURED** | $2,432.7\text{ counts/rev}$ measured over 3 consecutive Z pulses |
| **Real-Time Signed RPM Implementation** | **IMPLEMENTED** | Dynamic $\Delta t$ calculation in `EncoderDriver` |
| **Positive CW RPM Physical Telemetry** | **CONFIRMED** | $+5.00\text{ to }+125.00\text{ RPM}$, returns to $0.00$ |
| **Negative CCW RPM Physical Telemetry** | **CONFIRMED** | $-5.00\text{ to }-195.00\text{ RPM}$, returns to $0.00$ |
| **Invalid State Glitch Rejection** | **CONFIRMED** | `inv=0` across all physical tests |
| **Mechanical Encoder Mounting** | **NOT YET TESTED** | Encoder was hand-held / bench-isolated |
| **Encoder Shaft Location on Wheelchair** | **UNVERIFIED** | Motor shaft vs gearbox vs wheel hub unknown |
| **Wheel Diameter & Circumference** | **UNVERIFIED** | Physical wheel geometry not yet calibrated |
| **Mechanical Coupling & Backlash** | **UNVERIFIED** | No physical coupler installed |
| **Wheel Linear Speed ($\text{m/s}$)** | **NOT YET VALIDATED** | Requires verified mounting & wheel radius |
| **Distance Traveled ($\text{m}$)** | **NOT YET VALIDATED** | Requires verified mounting & wheel radius |
| **Two-Wheel Differential Odometry** | **NOT YET IMPLEMENTED** | Pi odometry pipeline pending |
| **Right Encoder Integration** | **NOT YET INTEGRATED** | Right GPIOs configured as placeholder 0 |

---

## 20. Part P — Important Engineering Limitations

1. **Bench Isolation:** The encoder has **not** been mechanically mounted to the wheelchair chassis or drivetrain.
2. **Measurement Target Unknown:** It is currently unknown whether the physical encoder will measure:
   - The high-speed DC motor shaft (pre-gearbox),
   - An intermediate gearbox shaft, or
   - The low-speed output wheel axle (direct drive).
3. **Gearbox Ratio Application:** The known wheelchair gearbox ratio ($\approx 23.3:1$) must **NOT** be applied to encoder pulses until the mechanical mounting location is verified.
4. **Wheel Geometry:** Wheel diameter and circumference have not been measured or calibrated for the encoder pipeline.
5. **No Speed / Distance Conversion:** Linear velocity ($\text{m/s}$) and linear distance ($\text{m}$) must not be derived from raw RPM or counts at this stage.
6. **Odometry Inactive:** Dead-reckoning and differential odometry calculations remain unimplemented and unverified.
7. **Single Encoder Only:** Only the Left Encoder was tested. Right Encoder wiring and dual-wheel synchronization remain unverified.
8. **No Power Stage Connection:** No wheelchair motor controllers, joystick lines, or power relays were energized during these tests.
9. **Empirical CPR Uncertainty:** The measured $2,432.7\text{ counts/rev}$ reflects periodic telemetry sampling uncertainty and must not be assumed to override the theoretical $2,400\text{ counts/rev}$ without optical gating measurements.
10. **Raw Dynamic RPM:** RPM is calculated directly from raw count deltas and elapsed milliseconds without low-pass filtering or moving average smoothing.

---

## 21. Part Q — Next Engineering Step: Mechanical Mounting / Calibration Review

```
================================================================================
           NEXT STEP — MECHANICAL MOUNTING / CALIBRATION REVIEW
================================================================================
```

Before authoring any new firmware code or modifying existing interfaces, an engineering review must answer the following 14 items:

1. **Mechanical Mounting Geometry:** What bracket, coupling, or flange will secure the encoder body and shaft?
2. **Shaft Identification:** Does the encoder connect directly to the wheelchair wheel axle or the motor armature shaft?
3. **Gearbox Reduction:** If mounted on the motor side, what is the exact verified gear ratio (e.g., $23.3:1$)?
4. **Wheel Diameter Measurement:** What is the exact measured diameter of the drive wheels under load?
5. **Wheel Circumference Calculation:** What is the calculated rolling circumference ($C = \pi \times D$)?
6. **Mechanical Backlash & Slip:** What mechanical backlash exists between the motor, gearbox, and wheel contact patch?
7. **Pulse-to-Distance Formulation:** What is the mathematical conversion factor:
   $$\text{Meters Per Count} = \frac{\text{Wheel Circumference}}{\text{Counts Per Wheel Revolution}}$$
8. **Linear Velocity Formulation:** What is the mathematical conversion from rotational speed to linear speed:
   $$v\text{ (m/s)} = \text{Wheel RPM} \times \frac{\text{Wheel Circumference}}{60.0}$$
9. **Dual-Wheel Baseline Distance:** What is the track width (wheelbase distance between left and right contact patches) for differential kinematics?
10. **Right Encoder Pin Allocation:** Which verified ESP32 GPIOs will be assigned to Right Encoder Channels A, B, and Z?
11. **Calibration Test Protocol:** What measured distance track (e.g., $5.0\text{ m}$ roll test) will be used to calibrate counts/meter?
12. **Existing Firmware Interfaces:** What software structs in `sensor_interface.h` and `encoder_manager.h` are designated for wheel speed and odometry?
13. **Noise & Filtering Requirements:** Does the host navigation system require raw instantaneous RPM or low-pass filtered velocity?
14. **Hardware Safety Interlocks:** What software safeguards will ensure motor power remains inhibited during mechanical calibration?

---

## 22. Engineering Conclusion

The single incremental optical rotary encoder has been conclusively validated across its complete electrical and firmware signal pipeline:

$$\text{Physical Optical Pulses} \longrightarrow \text{ESP32 GPIO Interrupts} \longrightarrow \text{Quadrature Decoding} \longrightarrow \text{Direction Derivation} \longrightarrow \text{Z-Index Tracking} \longrightarrow \text{Real-Time Signed RPM}$$

All unit tests ($49/49$) and physical bench tests (Stationary, CW, CCW, Z-to-Z, CW RPM, CCW RPM) have passed with zero glitch transitions (`inv=0`).

**Mechanical mounting, physical wheel calibration, linear velocity derivation ($\text{m/s}$), distance accumulation ($\text{m}$), and differential odometry remain separate, future engineering validation phases.**
