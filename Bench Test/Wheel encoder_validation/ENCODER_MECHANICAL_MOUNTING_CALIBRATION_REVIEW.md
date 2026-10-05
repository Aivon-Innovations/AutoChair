# Encoder Mechanical Mounting & Calibration Review

**Document Status:** COMPLETE / ENGINEERING REVIEW  
**Date:** Today's validation session  
**Project:** AutoChair — Aivon Innovations Pvt. Ltd.  
**Subsystem:** ESP32 Firmware, Wheel Encoders, Kinematics & Odometry Pipeline  
**Reference Document:** [`docs/ENCODER_PHASE_VALIDATION_REPORT.md`](file:///Users/admin/Desktop/AutoChair/docs/ENCODER_PHASE_VALIDATION_REPORT.md)  

---

## 1. Purpose

This document provides a comprehensive engineering analysis and readiness review for advancing the AutoChair wheel encoder subsystem from isolated electrical/bench verification to:
$$\text{Mechanical Mounting} \longrightarrow \text{Physical Calibration} \longrightarrow \text{Wheel Speed Derivation} \longrightarrow \text{Distance Tracking} \longrightarrow \text{Differential Odometry}$$

The primary objective is to explicitly define what firmware capabilities are already proven, establish what mechanical and physical geometric parameters remain unknown, and specify the exact physical measurements and calibration procedures required before any code conversions (ticks $\rightarrow$ meters, RPM $\rightarrow \text{m/s}$, differential poses $x, y, \theta$) can be safely integrated into production firmware.

---

## 2. Current Encoder Validation Status

As established in [`docs/ENCODER_PHASE_VALIDATION_REPORT.md`](file:///Users/admin/Desktop/AutoChair/docs/ENCODER_PHASE_VALIDATION_REPORT.md), the Left incremental optical rotary encoder has been fully validated on an isolated test bench:

- **Signal Verification:** Clean 2-channel quadrature and 1-channel index operation verified via logic analyzer ($5.0066\text{ V}$ supply, $3.307\text{ V}$ logic, $3 \times 4.7\text{ k}\Omega$ pull-up resistors).
- **Physical ESP32 Acquisition:** Interrupt-driven decoding on GPIO25 (A), GPIO26 (B), and GPIO27 (Z) is operational.
- **Directional & Stationary Integrity:** Zero drift while stationary (`dir=0`, `rpm=0.00`, `st=2`), monotonic positive counts under CW rotation (`dir=1`), and sustained negative counts under CCW rotation (`dir=-1`, delta $-22,383\text{ counts}$).
- **Index Pulse Verification:** Optical Z-index channel tracking verified.
- **Decoding Resolution:** $\text{X4}$ decoding mode configured ($2,400\text{ counts/rev}$ nominal; $2,432.7\text{ counts/rev}$ measured over 3 consecutive physical revolutions).
- **Real-Time RPM:** Dynamic elapsed-time signed RPM implemented and validated ($+5\text{ to }+125\text{ RPM}$ CW; $-5\text{ to }-195\text{ RPM}$ CCW).
- **Unit Test Suite:** **49 / 49 native unit tests PASS**; ESP32 build clean with 0 warnings.
- **Isolation Constraint:** Wheelchair motor controllers, wheelchair power stages, and chassis harnesses were completely disconnected during all tests.

---

## 3. Current Encoder Firmware Capabilities

An inspection of the current firmware implementation (`firmware/esp32/src/sensors/encoder/`, `firmware/esp32/include/autochair_config.h`, `firmware/esp32/src/main/main.cpp`) confirms that the ESP32 firmware provides the following features:

| Feature / Primitive | Firmware Location | Implementation Details |
| :--- | :--- | :--- |
| **Raw Cumulative Pulse Count** | `HardwareEncoderHAL::_count`<br>`EncoderReading::count` | Signed 64-bit integer (`int64_t`), updated in ISR via `QUAD_TABLE[16]`. |
| **Signed Count & Polarity** | `EncoderDriver::update()` | Inversion support via `EncoderConfig::reverse_direction` without silent negation. |
| **Rotational Direction** | `EncoderReading::direction` | Enumerated: `FORWARD (1)`, `REVERSE (-1)`, `STATIONARY (0)`, `UNKNOWN (255)`. |
| **Z-Index Event Tracking** | `HardwareEncoderHAL::_indexCount`<br>`EncoderHealth::index_events` | Incremented on rising edge of Channel Z (`pinZ`) via dedicated ISR. |
| **Pulses Per Update Cycle** | `EncoderReading::pulses_since_last_update` | Absolute count delta $|\Delta\text{count}|$ observed during the last acquisition interval. |
| **Real-Time Signed RPM** | `EncoderReading::rpm` | Derived from $\left(\frac{\Delta\text{count}}{\text{CPR}}\right) \times \left(\frac{60000}{\Delta t_{\text{ms}}}\right)$ using actual millisecond timestamps. |
| **Configurable Decoding Modes** | `EncoderDecodingMode` | Supported: `X1` ($600\text{ CPR}$), `X2` ($1,200\text{ CPR}$), `X4` ($2,400\text{ CPR}$). |
| **Dynamic CPR Calculation** | `EncoderDriver::getCountsPerRevolution()` | Computes $\text{ppr} \times \text{decoding\_mode} = 600 \times 4 = 2,400\text{ counts/rev}$. |
| **Dual-Wheel Abstraction** | `EncoderManager` | Manages independent Left and Right `EncoderDriver` instances; exposes `WheelEncoderData`. |
| **Timestamping** | `SensorReading::timestamp_ms` | Millisecond-resolution system timestamps associated with every sensor sample. |
| **Zeroing / Resets** | `EncoderDriver::resetCount()`<br>`EncoderDriver::reset()` | Resets count, index counter, last HAL count, delta, and RPM to zero. |
| **Health & Glitch Tracking** | `EncoderHealth` | Tracks total pulses, timestamp of last pulse, health status, and invalid state transitions (`inv`). |
| **Placeholder Pin Safety** | `HardwareEncoderHAL::init()` | Pins configured as `0` are rejected safely; driver remains in `UNINITIALIZED` state. |

---

## 4. Verified Mechanical Information

Based on an exhaustive review of repository documentation (`esp32 documents/`, `docs/`, `Architecture/`):

1. **Wheelchair Gearbox Nominal Ratio:**  
   The wheelchair drive assembly incorporates a reduction gearbox with a nominal ratio of approximately **$23.3:1$** (documented in PRD and design notes).
2. **Encoder Electrical Specification:**  
   The optical encoder disk produces **$600\text{ waveform cycles per physical encoder shaft revolution}$** ($1,200$ rising+falling edges per channel).
3. **Differential Drive Topology:**  
   The wheelchair operates as a two-wheel differential drive platform with two independent motorized drive wheels and passive front/rear casters.

---

## 5. Unknown Mechanical Information

The following mechanical and geometric parameters are **completely unverified** and must be physically established before writing any conversion logic:

```
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

---

## 6. Encoder Mounting Location

**Status:** **NOT VERIFIED — physical encoder mounting location has not yet been established.**

The firmware must not assume where the encoder is mounted. There are three fundamentally distinct mounting architectures possible on this wheelchair:

```
Architecture A: Motor-Shaft Mounted (High Speed)
[Motor Armature] ──► (ENCODER HERE) ──► [23.3:1 Gearbox] ──► [Drive Wheel]
Resolution: 2,400 counts/motor rev × 23.3 = 55,920 counts/wheel rev

Architecture B: Intermediate Gearbox Shaft Mounted
[Motor Armature] ──► [Stage 1] ──► (ENCODER HERE) ──► [Stage 2] ──► [Drive Wheel]
Resolution: Depends on intermediate gear stage reduction

Architecture C: Direct Wheel-Axle Mounted (Low Speed)
[Motor Armature] ──► [23.3:1 Gearbox] ──► [Drive Wheel Axle] ──► (ENCODER HERE)
Resolution: 2,400 counts/wheel rev (1:1 with wheel)
```

Until physical inspection verifies which shaft the encoder shaft couples to, **no pulses-per-wheel-revolution constant can be defined**.

---

## 7. Gearbox Relationship

- **Gearbox Ratio:** Approximately $23.3:1$.
- **Current Firmware Usage:** The $23.3:1$ ratio is **NOT** used in the current encoder firmware.
- **Engineering Constraint:**  
  The $23.3:1$ gearbox ratio must **NOT** currently be applied to encoder pulses.
- **Why:**  
  If the encoder is coupled directly to the output wheel hub (Architecture C), applying $23.3:1$ would introduce a $2330\%$ scaling error. Conversely, if mounted to the high-speed motor tail shaft (Architecture A), the gear reduction must be multiplied. Until the physical mounting point is verified, the ratio must remain decoupled from the driver.

---

## 8. Wheel Geometry Requirements

To convert rotational motion into linear displacement and linear velocity, exact wheel dimensions are mandatory:

1. **Nominal Wheel Diameter ($D_{\text{nom}}$):**  
   The outer diameter of the drive wheel measured with calipers/tape off-load. *(Python simulation default in `src/autochair/navigation/wheel_geometry.py` is $0.30\text{ m}$, but this is an unverified placeholder).*
2. **Effective Rolling Circumference ($C_{\text{eff}}$):**  
   The actual linear ground distance traveled per $360^\circ$ rotation of the drive wheel under full occupant load:
   $$C_{\text{eff}} = \pi \times D_{\text{eff}}$$
3. **Tire Deflection & Load Variations:**  
   Under passenger payload ($50\text{--}100\text{ kg}$), tire compression reduces the effective radius ($R_{\text{eff}} < R_{\text{nom}}$).
4. **Surface Slip:**  
   Micro-slip occurs during acceleration and turning. Calibration must measure ground-truth rolling distance over flat ground rather than relying solely on geometric $\pi D$.

---

## 9. Encoder-to-Wheel Conversion Concept

Once the mechanical arrangement is verified, the mathematical conversion chain will follow this conceptual pipeline:

```
[Raw Encoder Ticks (Δcount)]
              │
              ▼  ÷ (Counts Per Encoder Revolution: e.g., 2,400)
[Encoder Shaft Revolutions]
              │
              ▼  ÷ (Transmission Ratio: N_gearbox or 1.0)
[Wheel Revolutions]
              │
              ▼  × (Effective Wheel Circumference: C_eff in meters)
[Linear Wheel Distance (Δdistance in meters)]
              │
              ▼  ÷ (Elapsed Time: Δt in seconds)
[Linear Wheel Velocity (v in m/s)]
```

### Mathematical Formulations (Conceptual)

1. **Counts Per Wheel Revolution ($CPR_{\text{wheel}}$):**
   $$CPR_{\text{wheel}} = CPR_{\text{encoder}} \times K_{\text{transmission}}$$
   *(where $K_{\text{transmission}} = 23.3$ if on motor shaft, or $1.0$ if on wheel axle).*

2. **Distance per Count ($k_{\text{dist}}$ in meters/count):**
   $$k_{\text{dist}} = \frac{C_{\text{eff}}}{CPR_{\text{wheel}}}$$

3. **Incremental Wheel Distance ($\Delta s$):**
   $$\Delta s = \Delta\text{count} \times k_{\text{dist}}$$

4. **Linear Wheel Speed ($v_{\text{wheel}}$ in $\text{m/s}$):**
   $$v_{\text{wheel}} = \text{Wheel RPM} \times \frac{C_{\text{eff}}}{60.0} = \left(\frac{\text{Encoder RPM}}{K_{\text{transmission}}}\right) \times \frac{C_{\text{eff}}}{60.0}$$

---

## 10. Calibration Requirements

A rigorous physical calibration procedure must be conducted after mechanical installation to empirically determine $k_{\text{dist}}$ without relying on theoretical assumptions.

### Calibration Protocol Requirements

1. **Track Preparation:** Establish a straight, level $5.000\text{ m}$ test track on the target flooring surface with millimeter-precision start/finish marks.
2. **Pre-Test Inspection:** Record initial tire pressure (if pneumatic) and load the wheelchair with representative ballast weight.
3. **Manual Roll Test (Forward):**
   - Align the drive wheel reference mark with the $0.000\text{ m}$ start line.
   - Zero the encoder counts (`resetCount()`).
   - Push the wheelchair in a straight line for exactly 10 complete wheel rotations (monitored via visual mark and optical Z-index pulses).
   - Record the physical tape-measure distance $S_{\text{measured}}$ and final encoder count $N_{\text{ticks}}$.
4. **Repeated Trials:** Execute a minimum of 5 forward trials and 5 reverse trials.
5. **Left vs Right Verification:** Compare Left and Right encoder tick accumulations over the identical linear distance to verify wheel diameter symmetry.
6. **Empirical Factor Derivation:**
   $$k_{\text{dist, empirical}} = \frac{S_{\text{measured}}}{N_{\text{ticks}}}$$
   $$C_{\text{eff, empirical}} = k_{\text{dist, empirical}} \times CPR_{\text{wheel}}$$

---

## 11. Second Encoder (Right Wheel) Requirements

### 11.1 What the Current Software Already Provides
- `EncoderManager` architecture with dedicated `_leftDriver` and `_rightDriver` instances.
- Independent signed counts, direction derivation, RPM calculations, and health metrics.
- `WheelEncoderData` struct containing separate `.left` and `.right` `EncoderReading` instances.
- Software configuration placeholders in `autochair_config.h` (`ENCODER_RIGHT_A_PIN = 0`, `ENCODER_RIGHT_B_PIN = 0`, `ENCODER_RIGHT_Z_PIN = 0`).
- Mock unit test validation: `test_encoder_manager_dual_wheel_independence` passes 100%.

### 11.2 What Must Be Verified Before Connecting Right Encoder
1. **GPIO Allocation:** Select 3 free, interrupt-capable ESP32 GPIOs that do not conflict with strapping pins (GPIO0, GPIO2, GPIO12, GPIO15) or Flash/JTAG/IMU/Ultrasonic pins.
2. **Hardware Pull-Ups:** Install $3 \times 4.7\text{ k}\Omega$ pull-up resistors to the $3.3\text{ V}$ logic rail for the Right encoder harness.
3. **Polarity Consistency:** Because the Left and Right wheels face opposite directions on a common axle line, forward vehicle motion rotates one wheel clockwise and the other counter-clockwise relative to their respective outer faces. Physical testing must determine whether `EncoderConfig::reverse_direction = true` is required for the Right driver to ensure forward vehicle motion yields positive counts for both wheels.

---

## 12. Wheel Speed Requirements

To compute and report true vehicle speed, the following parameters are strictly required:

| Input Parameter | Source / Method | Status |
| :--- | :--- | :--- |
| **Encoder Raw RPM** | `EncoderDriver::getReading().rpm` | **IMPLEMENTED / VALIDATED** |
| **Encoder CPR** | $600\text{ PPR} \times \text{Mode (X4)} = 2,400$ | **CONFIRMED** |
| **Transmission Ratio ($K_{\text{trans}}$)** | Physical mechanical inspection | **UNKNOWN** |
| **Effective Wheel Circumference ($C_{\text{eff}}$)** | Empirical roll-out measurement | **UNKNOWN** |
| **Speed Conversion Formula:** | $v\text{ (m/s)} = \left(\frac{\text{Encoder RPM}}{K_{\text{trans}}}\right) \times \left(\frac{C_{\text{eff}}}{60.0}\right)$ | **CONCEPTUAL / PENDING** |
| **Speed in km/h:** | $v_{\text{km/h}} = v_{\text{m/s}} \times 3.6$ | **CONCEPTUAL / PENDING** |

---

## 13. Distance Requirements

To accumulate linear distance traveled along the ground:

1. **Inputs Required:**
   - Real-time delta ticks $\Delta\text{count}_{\text{left}}$ and $\Delta\text{count}_{\text{right}}$.
   - Empirically calibrated distance per tick $k_{\text{dist, left}}$ and $k_{\text{dist, right}}$.
2. **Forward Distance Formulation:**
   $$\Delta s_{\text{left}} = \Delta\text{count}_{\text{left}} \times k_{\text{dist, left}}$$
   $$\Delta s_{\text{right}} = \Delta\text{count}_{\text{right}} \times k_{\text{dist, right}}$$
   $$\Delta s_{\text{vehicle}} = \frac{\Delta s_{\text{left}} + \Delta s_{\text{right}}}{2}$$
3. **Total Odometer Accumulation:**
   $$S_{\text{total}} = \sum \Delta s_{\text{vehicle}}$$

---

## 14. Future Odometry Requirements

### 14.1 Existing Architecture Review
- **Firmware Level (`firmware/esp32/`):**  
  The ESP32 firmware currently does **NOT** contain pose calculation ($x, y, \theta$) or differential odometry state integrators. This is intentional: per architectural specification, the ESP32 acts as a high-rate deterministic sensor interface and safety controller, transmitting raw timestamped counts and RPM to the Raspberry Pi over UART.
- **Host / Navigation Level (`src/autochair/navigation/`):**  
  The Python navigation package contains:
  - `DifferentialOdometry` (`src/autochair/navigation/differential_odometry.py`): Implements forward Euler / Runge-Kutta arc-based pose integration.
  - `DistanceCalculator` (`src/autochair/navigation/distance_calculator.py`): Converts ticks to distance.
  - `WheelGeometry` (`src/autochair/navigation/wheel_geometry.py`): Contains placeholder defaults (`wheel_diameter_m = 0.30`, `wheel_base_m = 0.50`).
  - `Pose` (`src/autochair/navigation/pose.py`): Tracks $x\text{ (m)}, y\text{ (m)}, \text{heading (deg)}$.

### 14.2 Physical Parameters Required for Differential Odometry
To compute vehicle planar pose:

```
                                  ▲ Y (Forward)
                                  │
                                  │
                     ┌────────────┴────────────┐
                     │                         │
            Left     │       AutoChair         │     Right
            Wheel ───│═════════════════════════│─── Wheel
            [ s_L ]  │       Chassis           │    [ s_R ]
                     │                         │
                     └────────────┬────────────┘
                                  │
                                  ├───────────► X (Right)
                                  │
                             ◄─── L ───►  (Track Width / Wheelbase)
```

1. **Effective Track Width ($L$ / Wheelbase):**  
   The lateral distance between the center contact patches of the Left and Right drive wheels. *(Must be measured with millimeter precision; an uncalibrated track width causes catastrophic angular drift during turns).*
2. **Kinematic Equations (Arc Model):**
   $$\Delta s = \frac{\Delta s_{\text{right}} + \Delta s_{\text{left}}}{2}$$
   $$\Delta\theta = \frac{\Delta s_{\text{right}} - \Delta s_{\text{left}}}{L}$$
   $$x_{k+1} = x_k + \Delta s \cdot \cos\left(\theta_k + \frac{\Delta\theta}{2}\right)$$
   $$y_{k+1} = y_k + \Delta s \cdot \sin\left(\theta_k + \frac{\Delta\theta}{2}\right)$$
   $$\theta_{k+1} = \theta_k + \Delta\theta$$
3. **IMU Heading Fusion (Future Phase):**  
   Encoder-based heading integration drifts rapidly due to tire slip. Future sensor fusion (`src/autochair/navigation/`) will fuse yaw gyro rate from the MPU IMU with wheel differential $\Delta\theta$.

---

## 15. Mechanical Measurement Checklist

| Physical Measurement / Parameter | Why Needed | Already Known? | Source / Evidence | Required Before |
| :--- | :--- | :---: | :--- | :--- |
| **Encoder Mounting Location** | Determines gear ratio scaling ($K_{\text{trans}}$) | **NO** | *Unverified in repository* | Counts $\rightarrow$ Revs |
| **Shaft Identity** | Motor shaft vs gearbox vs wheel hub | **NO** | *Unverified in repository* | Counts $\rightarrow$ Revs |
| **Encoder Mechanical Coupling** | Direct, belt, or gear coupling ratio | **NO** | *Unverified in repository* | Wheel RPM |
| **Gearbox Reduction Ratio** | Drivetrain angular velocity scaling | **PARTIAL** | PRD notes $\approx 23.3:1$ nominal | Wheel Speed |
| **Wheel Diameter ($D_{\text{nom}}$)** | Geometric circumference calculation | **NO** | Placeholder $0.30\text{ m}$ only | Distance ($\text{m}$) |
| **Wheel Radius under Load ($R_{\text{eff}}$)** | Accounts for tire deflection/payload | **NO** | *Unmeasured on hardware* | Distance ($\text{m}$) |
| **Effective Rolling Circumference ($C_{\text{eff}}$)** | Ground distance per wheel revolution | **NO** | *Requires physical roll test* | Speed & Distance |
| **Drive Wheel Track Width ($L$)** | Angular rotation per differential delta | **NO** | Placeholder $0.50\text{ m}$ only | Differential Odometry |
| **Left / Right Wheel Symmetry** | Prevents curved drift in straight paths | **NO** | *Requires physical roll test* | Odometry Heading |
| **Mechanical Backlash & Play** | Deadband during direction reversals | **NO** | *Unmeasured on hardware* | Reversing Odometry |
| **Encoder Mounting Rigidity** | Prevents vibration-induced count errors | **NO** | *Requires physical mount* | High-Speed Bench |

---

## 16. Software Readiness Checklist

| Software Capability | Current Status | Evidence / Location | Can We Use It Yet? |
| :--- | :---: | :--- | :---: |
| **Raw Cumulative Count** | **READY** | `EncoderReading::count`, `HardwareEncoderHAL` | **YES** (Pulse tracking) |
| **Rotational Direction** | **READY** | `EncoderReading::direction`, `EncoderDriver` | **YES** (CW/CCW state) |
| **Optical Z-Index Event Count** | **READY** | `HardwareEncoderHAL::_indexCount` | **YES** (Revs reference) |
| **X4 Quadrature Decoding** | **READY** | 4-bit `QUAD_TABLE`, 49 unit tests passed | **YES** (2400 CPR nominal) |
| **Raw Encoder Shaft RPM** | **READY** | `EncoderReading::rpm`, physical test validated | **YES** (Encoder shaft RPM) |
| **Left Encoder Driver** | **READY** | `EncoderManager::getLeftDriver()`, GPIO25/26/27 | **YES** (Left channel) |
| **Right Encoder Driver** | **BLOCKED (HW)** | Software architecture ready; GPIOs unassigned (0) | **NO** (Needs HW wiring) |
| **Wheel Rotational Speed (RPM)** | **BLOCKED** | Requires verified transmission ratio ($K_{\text{trans}}$) | **NO** (Mount unknown) |
| **Linear Vehicle Speed ($\text{m/s}$)** | **BLOCKED** | Requires $K_{\text{trans}}$ and rolling circumference $C_{\text{eff}}$ | **NO** (Geometry unknown) |
| **Distance Traveled ($\text{m}$)** | **BLOCKED** | Requires calibrated meters/count factor $k_{\text{dist}}$ | **NO** (Calibration needed) |
| **Differential Odometry ($x,y,\theta$)** | **BLOCKED** | Requires 2 validated encoders + track width $L$ | **NO** (Geometry needed) |

---

## 17. What Must NOT Be Assumed

1. **Do NOT assume the encoder measures the wheel axle.** (If it is coupled to the motor tail shaft, one wheel rotation equals $\approx 23.3$ encoder rotations).
2. **Do NOT assume the $23.3:1$ gearbox ratio belongs in the encoder math yet.** (If direct-coupled to the wheel hub, the ratio is $1.0$).
3. **Do NOT assume wheel diameter is $0.30\text{ m}$ or wheelbase is $0.50\text{ m}$.** (The values in `wheel_geometry.py` are unit-test mock defaults, not physical wheelchair measurements).
4. **Do NOT assume tire rolling circumference equals $\pi \times D_{\text{nominal}}$.** (Tire load deflection reduces effective circumference by $2\text{--}5\%$).
5. **Do NOT assume Left and Right wheels have identical rolling diameters.** (Unequal tire wear or pressure causes straight-line drift unless calibrated).
6. **Do NOT assume encoder polarity is identical on Left and Right wheels.** (Opposing physical wheel orientations often require software inversion on one channel).
7. **Do NOT assume zero drivetrain backlash.** (Reversing direction will experience an angular deadband before wheel motion occurs).

---

## 18. Required Physical Inspection Before Implementation

Before authoring code for wheel speed, distance, or odometry, an engineer must execute this physical inspection checklist:

1. [ ] **Chassis Inspection:** Inspect the wheelchair motor/gearbox casing to identify designated encoder mounting brackets or shaft extensions.
2. [ ] **Shaft Confirmation:** Rotate the wheelchair wheel by hand and observe which physical shaft rotates synchronously with the wheel vs at gearbox speed.
3. [ ] **Wheel Measurement:** Use a flexible steel tape measure to measure the exact outer circumference of both drive tires off-load.
4. [ ] **Loaded Roll Measurement:** Mark the tire and floor; push the occupied wheelchair forward for 10 complete wheel rotations; measure total distance with a laser/tape measure.
5. [ ] **Track Width Measurement:** Measure the lateral distance between the center points of the left and right drive tire contact patches.
6. [ ] **Right Encoder Pinout Selection:** Select 3 dedicated ESP32 GPIOs for Right Channel A, B, and Z.

---

# Next Engineering Sequence

The following step-by-step engineering sequence must be executed sequentially. No step may be bypassed or assumed:

1. **Physically identify the intended encoder mounting location on the wheelchair chassis.**
2. **Determine exactly which shaft/component the encoder measures (motor armature, intermediate gear, or wheel axle).**
3. **Verify the mechanical transmission relationship ($K_{\text{transmission}}$).**
4. **Measure physical wheel diameter, radius, and rolling circumference under load ($C_{\text{eff}}$).**
5. **Measure left/right wheel geometry and track width ($L$) required for differential kinematics.**
6. **Verify encoder mechanical coupling, mounting rigidity, and alignment.**
7. **Mount the Left encoder safely to the wheelchair chassis.**
8. **Perform a controlled physical encoder-to-wheel roll calibration over a measured track.**
9. **Validate calculated Wheel RPM against optical tachometer measurements.**
10. **Validate linear speed ($\text{m/s}$) against measured transit times.**
11. **Validate cumulative distance ($\text{m}$) against ground-truth tape measurements.**
12. **Wire, configure, bench-test, and mount the Right encoder.**
13. **Only after both wheels are calibrated, proceed to differential-wheel odometry and ROS/Pi pose integration.**
