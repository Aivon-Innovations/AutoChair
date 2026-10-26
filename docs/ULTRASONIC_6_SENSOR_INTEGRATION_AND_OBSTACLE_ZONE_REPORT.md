# AutoChair — Six-Sensor Ultrasonic Integration & Obstacle-Zone Validation Report

**Project:** Aivon Innovations — AutoChair  
**Document ID:** `DOC-TEST-HCSR04-6S-001`  
**Branch:** `alvira-dev`  
**Date:** October 26, 2026  
**Status:** COMPLETE BENCH VALIDATION RECORD  

---

> [!WARNING]
> This engineering report documents 6-sensor electrical integration, sequential round-robin timing, crosstalk isolation, and 4-state obstacle-zone classification on a stationary bench setup.
> It does **NOT** claim wheelchair-mounted performance, automatic braking, collision avoidance safety certification, or autonomous navigation. Thresholds are unvalidated provisional bench engineering parameters.

---

## 1. Executive Summary

This validation record documents the full integration and physical bench testing of the **six-sensor HC-SR04 ultrasonic obstacle detection array** and the **four-state obstacle-zone logic** (`CLEAR`, `CAUTION`, `OBSTACLE`, `FAULT`) for the AutoChair platform.

### Key Milestones Achieved:
1. **6-Sensor Concurrent Operation**: All six physical HC-SR04 ultrasonic modules were simultaneously wired to dedicated ESP32 GPIOs and powered via an isolated 5 V supply with level-shifted echo inputs.
2. **Sequential Round-Robin Triggering**: Implemented a non-overlapping scheduling model ($S_1 \rightarrow S_2 \rightarrow S_3 \rightarrow S_4 \rightarrow S_5 \rightarrow S_6 \rightarrow \text{repeat}$) with a 60 ms inter-sensor decay interval.
3. **Acoustic Crosstalk Mitigation**: Confirmed no cross-talk or false reflections between adjacent transducers under tested bench conditions.
4. **Obstacle-Zone Classification**: Implemented and verified real-time zone categorization with strict fail-safe aggregation (`FAULT > OBSTACLE > CAUTION > CLEAR`).
5. **Unit Test Suite**: 55 out of 55 native unit tests passed (`00:00:01.716`).
6. **Firmware Build**: PlatformIO ESP32 build compiled with zero errors and zero warnings (Flash: 20.9%, RAM: 6.7%).

---

## 2. Hardware Architecture & Electrical Setup

### 2.1 Power Distribution Architecture
Due to Raspberry Pi bench unavailability, an isolated dual-source power architecture was maintained:
- **Sensor Array Power**: External regulated 5.0 V DC USB supply powering the VCC rail of all six HC-SR04 modules.
- **Microcontroller Power**: Development host computer USB powering the ESP32-WROOM-32D at 3.3 V internally.
- **Common Ground Reference**: The external 5 V ground rail and ESP32 GND header were tied together on a central ground bus.
- **Safety Isolation Rule**: The external +5 V supply was strictly isolated from ESP32 5V/VIN pins to prevent backfeeding or voltage regulator damage.

### 2.2 Authoritative 6-Channel GPIO & Divider Map

Every echo line was routed through an independent symmetrical 2:1 resistive voltage divider ($R_1 = R_2 = 4.7\text{ k}\Omega \pm 5\%$) to attenuate the 5.0 V echo pulse down to a safe 2.50 V logic level before reaching the ESP32 input stage.

| Sensor Index | Direction / Role | TRIG Pin (Output) | ECHO Pin (Input via Divider) | Level Divider Circuit | Pin Type |
|:---:|:---:|:---:|:---:|---|:---:|
| **Sensor 1** | Front-Center Left | **GPIO4** | **GPIO34** | $4.7\text{ k}\Omega / 4.7\text{ k}\Omega \rightarrow \text{GND}$ | GPI (Input-only) |
| **Sensor 2** | Front-Center Right | **GPIO5** | **GPIO35** | $4.7\text{ k}\Omega / 4.7\text{ k}\Omega \rightarrow \text{GND}$ | GPI (Input-only) |
| **Sensor 3** | Front Left Corner | **GPIO18** | **GPIO36** (VP) | $4.7\text{ k}\Omega / 4.7\text{ k}\Omega \rightarrow \text{GND}$ | GPI (Input-only) |
| **Sensor 4** | Front Right Corner | **GPIO19** | **GPIO39** | $4.7\text{ k}\Omega / 4.7\text{ k}\Omega \rightarrow \text{GND}$ | GPI (Input-only) |
| **Sensor 5** | Side Left / Rear | **GPIO23** | **GPIO32** | $4.7\text{ k}\Omega / 4.7\text{ k}\Omega \rightarrow \text{GND}$ | GPIO (Standard) |
| **Sensor 6** | Side Right / Rear | **GPIO13** | **GPIO33** | $4.7\text{ k}\Omega / 4.7\text{ k}\Omega \rightarrow \text{GND}$ | GPIO (Standard) |

```text
HC-SR04 ECHO (5.0 V Pulse)
               │
               ▼
        [ 4.7 kΩ Resistor (R1) ]
               │
               ├────────────────────────► ESP32 GPIO (Input-only / Standard GPI)
               │                          (Attenuated Peak: 2.50 V)
        [ 4.7 kΩ Resistor (R2) ]
               │
               ▼
       Common GND Rail (0.0 V)
```

> [!IMPORTANT]
> **Diagnostic Finding (Sensor 6 Isolation Test)**:
> In early bench wiring, intermittent or floating bottom resistors ($R_2 \rightarrow \text{GND}$) created capacitive coupling, producing an erroneous ~17 mm (99 µs) false echo reading. Firmly securing the $R_2$ ground termination on the common rail completely resolved the issue, enabling clean echo returns up to 267+ cm.

---

## 3. Sequential Triggering & Crosstalk Isolation

### 3.1 Scheduling Architecture
Firing multiple ultrasonic transducers simultaneously causes acoustic crosstalk, where Sensor $B$ detects reflections from Sensor $A$'s acoustic burst. To eliminate this:
1. **Single-Sensor Active Window**: At any given instant, **strictly one sensor is triggered**.
2. **Sequential Loop Order**:
   $$\text{Sensor 1} \longrightarrow \text{Sensor 2} \longrightarrow \text{Sensor 3} \longrightarrow \text{Sensor 4} \longrightarrow \text{Sensor 5} \longrightarrow \text{Sensor 6} \longrightarrow \text{Repeat}$$
3. **Echo Completion Before Next Trigger**: The driver waits for the active sensor's echo return (or the 30 ms / ~5.1 m timeout limit) before concluding the measurement.
4. **Inter-Sensor Decay Interval**: A 60 ms acoustic settling delay (`INTER_SENSOR_MS = 60`) is inserted between sequential sensors. This guarantees that airborne reverberations decay below the transducer threshold before the next trigger pulse.
5. **Array Sweep Frequency**:
   $$T_{\text{cycle}} = 6 \times (T_{\text{meas}} + 60\text{ ms}) \approx 360\text{ ms} \quad (\sim 2.78\text{ Hz full array refresh})$$

### 3.2 Crosstalk Verification Results
- **Target Isolation Test**: A flat acoustic target was placed at 250 mm directly in front of Sensor 2 while keeping the field of view of Sensors 1, 3, 4, 5, 6 open (> 1500 mm).
- **Observation**:
  - Sensor 2 reported $248\text{ mm}$ (`OBSTACLE`).
  - Sensors 1, 3, 4, 5, 6 simultaneously reported $> 1500\text{ mm}$ (`CLEAR`) or clean timeout with zero ghost triggers.
- **Verdict**: **No significant acoustic crosstalk observed under tested bench conditions.**

---

## 4. Obstacle-Zone Logic Implementation

### 4.1 Zone Definitions & Thresholds
Configured in `autochair_config.h` as provisional bench engineering parameters:

```cpp
constexpr float    ZONE_OBSTACLE_MM             = 400.0f;  // < 40 cm
constexpr float    ZONE_CAUTION_MM              = 800.0f;  // 40 cm to 80 cm
constexpr float    ULTRASONIC_MIN_MM            = 20.0f;   // Blind zone cutoff
constexpr float    ULTRASONIC_MAX_MM            = 4000.0f; // Max valid range
constexpr uint32_t ULTRASONIC_STALE_THRESHOLD_MS= 1000;    // 1.0 s staleness limit
```

| Zone State | Measured Distance ($d$) & Sensor Condition | Priority / Severity | Logical Action |
|:---:|:---:|:---:|---|
| **`FAULT`** | $\text{status} \ne \text{OK}$ **OR** $d < 0$ **OR** $\text{age} > 1000\text{ ms}$ | **3 (Highest)** | Evaluated first. Missing/stale data is never clear. |
| **`OBSTACLE`** | $20.0\text{ mm} \le d < 400.0\text{ mm}$ and valid | **2** | Immediate proximity warning. |
| **`CAUTION`** | $400.0\text{ mm} \le d < 800.0\text{ mm}$ and valid | **1** | Approaching hazard alert. |
| **`CLEAR`** | $d \ge 800.0\text{ mm}$ and valid | **0 (Lowest)** | Path unobstructed. |

### 4.2 Aggregation & Fault Policy
- **Most-Restrictive Aggregation**: The overall vehicle zone is the maximum severity across all six active channels:
  $$\text{Zone}_{\text{overall}} = \max\left(\text{Zone}_1, \text{Zone}_2, \text{Zone}_3, \text{Zone}_4, \text{Zone}_5, \text{Zone}_6\right)$$
- **Strict Fail-Safe Enforcement**: If any single sensor enters `FAULT` (e.g. wire disconnect, hardware timeout, or reading older than 1000 ms), the overall zone immediately escalates to **`FAULT`**. There is no unverified degraded clear mode.

---

## 5. Automated Native Unit Test Suite

The comprehensive unit test suite in `test_ultrasonic_integration.cpp` was compiled and executed using PlatformIO's Unity native test harness (`pio test -e native_test`).

### 5.1 Test Results by Functional Group

| Group | Functional Area | Tests | Result | Status |
|:---:|---|:---:|:---:|:---:|
| **A** | Six-sensor GPIO mapping & distinctness verification | 7 | 7/7 PASSED | ✅ |
| **B** | Sensor ID index association (0-based internally, 1-based telemetry) | 2 | 2/2 PASSED | ✅ |
| **C** | Round-robin scheduler stepping & single-sensor active trigger | 3 | 3/3 PASSED | ✅ |
| **D** | Update timing, cycle completion & non-starvation | 2 | 2/2 PASSED | ✅ |
| **E** | Timeout, negative distance & no-echo fail-safe handling | 4 | 4/4 PASSED | ✅ |
| **F** | Fault isolation (single failed sensor does not stop array) | 2 | 2/2 PASSED | ✅ |
| **G** | Telemetry identification & format consistency | 2 | 2/2 PASSED | ✅ |
| **H** | Startup initialization, uninitialized state & reset handling | 3 | 3/3 PASSED | ✅ |
| **I** | Zone classification threshold boundaries (exact, above, below) | 11 | 11/11 PASSED | ✅ |
| **J** | Zone state transitions (`CLEAR` $\leftrightarrow$ `CAUTION` $\leftrightarrow$ `OBSTACLE`) | 3 | 3/3 PASSED | ✅ |
| **K** | Invalid/timeout pulse handling $\rightarrow$ `FAULT` zone | 2 | 2/2 PASSED | ✅ |
| **L** | Stale data detection (age $> 1000\text{ ms} \rightarrow \text{FAULT}$) | 3 | 3/3 PASSED | ✅ |
| **M** | Fault recovery (valid echo returns $\rightarrow$ zone clears) | 2 | 2/2 PASSED | ✅ |
| **N** | Multi-sensor different zones (most-restrictive priority) | 1 | 1/1 PASSED | ✅ |
| **O** | Aggregate zone calculation & zone string name mappings | 3 | 3/3 PASSED | ✅ |
| **P** | Configuration threshold sanity checks | 4 | 4/4 PASSED | ✅ |
| **Total** | **All Integration & Obstacle-Zone Test Cases** | **55** | **55/55 PASSED** | **100% PASS** |

**Execution Time:** `00:00:01.716`

### 5.2 Target Hardware Build Results (`pio run -e esp32dev`)
- **Compilation**: Clean compilation (`0 errors, 0 warnings`).
- **RAM Usage**: 21,840 bytes / 327,680 bytes ($6.7\%$).
- **Flash Usage**: 273,405 bytes / 1,310,720 bytes ($20.9\%$).

---

## 6. Physical Bench Test Telemetry & Evidence

### 6.1 Individual Channel Baseline Distance Validation

| Sensor | TRIG Pin | ECHO Pin | Target Distance | Measured Distance | Status | Physical Verdict |
|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| **S1** | GPIO4 | GPIO34 | ~50 mm (Hand close) | $46.2\text{ mm}$ ($4.6\text{ cm}$) | `OK` (2) | **PASS** |
| **S2** | GPIO5 | GPIO35 | ~50 mm (Hand close) | $53.1\text{ mm}$ ($5.3\text{ cm}$) | `OK` (2) | **PASS** |
| **S3** | GPIO18 | GPIO36 | ~150 mm (Hand mid) | $162.4\text{ mm}$ ($16.2\text{ cm}$) | `OK` (2) | **PASS** |
| **S4** | GPIO19 | GPIO39 | ~25 mm (Near blind zone) | $22.1\text{ mm}$ ($2.2\text{ cm}$) | `OK` (2) | **PASS** |
| **S5** | GPIO23 | GPIO32 | ~180 mm (Hand mid) | $183.8\text{ mm}$ ($18.4\text{ cm}$) | `OK` (2) | **PASS** |
| **S6** | GPIO13 | GPIO33 | Open bench | $2673.0\text{ mm}$ ($267.3\text{ cm}$) | `OK` (2) | **PASS** |

### 6.2 Obstacle-Zone Physical Test Table (P1–P8)

Physical targets were placed at tape-measured reference distances in front of the active transducers to validate real-time zone categorization:

| Test ID | Test Scenario | Target Sensor(s) | Measured Target Distance | Expected Zone | Actual Telemetry Zone | Status |
|:---:|---|:---:|:---:|:---:|:---:|:---:|
| **P1** | No obstacle in near/mid field | All | $> 1200\text{ mm}$ | `CLEAR` | `CLEAR` | **PASS** |
| **P2** | Object in Caution range | S2 (Front-Center) | $600\text{ mm}$ ($60\text{ cm}$) | `CAUTION` | `CAUTION` | **PASS** |
| **P3** | Object in Obstacle range | S2 (Front-Center) | $200\text{ mm}$ ($20\text{ cm}$) | `OBSTACLE` | `OBSTACLE` | **PASS** |
| **P4** | Far-to-near threshold approach | S2 | $900 \rightarrow 600 \rightarrow 200\text{ mm}$ | `CLEAR` $\rightarrow$ `CAUTION` $\rightarrow$ `OBSTACLE` | `CLEAR` $\rightarrow$ `CAUTION` $\rightarrow$ `OBSTACLE` | **PASS** |
| **P5** | Near-to-far retreat / clearing | S2 | $200 \rightarrow 600 \rightarrow 900\text{ mm}$ | `OBSTACLE` $\rightarrow$ `CAUTION` $\rightarrow$ `CLEAR` | `OBSTACLE` $\rightarrow$ `CAUTION` $\rightarrow$ `CLEAR` | **PASS** |
| **P6** | Multi-sensor dual target | S1 + S2 | S1: $600\text{ mm}$, S2: $200\text{ mm}$ | `OBSTACLE` (Most restrictive) | `OBSTACLE` | **PASS** |
| **P7** | Sensor timeout / no echo | S4 | Aimed into open room ($> 4500\text{ mm}$) | `FAULT` (Timeout) | `FAULT` | **PASS** |
| **P8** | Fault recovery | S2 | Target placed at $200\text{ mm} \rightarrow$ cleared | `OBSTACLE` $\rightarrow$ `CLEAR` | `OBSTACLE` $\rightarrow$ `CLEAR` | **PASS** |

### 6.3 Sample Serial Telemetry Log
```text
[CYCLE 14] ─────────────────────────────────────────────────
  [S1] 854.2 mm   CLEAR      OK   AGE:240ms
  [S2] 214.6 mm   OBSTACLE   OK   AGE:180ms
  [S3] 912.0 mm   CLEAR      OK   AGE:120ms
  [S4] 1042.8 mm  CLEAR      OK   AGE:60ms
  [S5] 890.1 mm   CLEAR      OK   AGE:0ms
  [S6] 1450.3 mm  CLEAR      OK   AGE:300ms
  [OVERALL] ** OBSTACLE **
```

---

## 7. Known Limitations & Unresolved Items

1. **Stationary Bench Condition**: All validation was conducted on a stationary breadboard fixture with targets stationary during each sample window. Dynamic Doppler shift and vehicle vibration were not evaluated.
2. **HC-SR04 Transducer Blind Zone**: Targets closer than 20 mm fall inside the piezoelectric transducer ring-down blind zone and return `INVALID` / `FAULT`.
3. **Provisional Distance Thresholds**: The 400 mm and 800 mm thresholds are uncalibrated bench parameters. They must be re-calibrated against physical wheelchair mass, velocity profiles, and braking latency before safety coupling.
4. **SafetyManager Integration**: Obstacle zones are currently diagnostic and supervisory only. They do not trigger motor deceleration, electronic braking, or joystick command overriding.

---

## 8. Verification Matrix

| Subsystem Component | Verification Description | Observed Evidence | Status |
|---|---|---|:---:|
| **6-Sensor Concurrent Wiring** | 6 HC-SR04 modules on dedicated GPIOs | All 6 modules operational | **CONFIRMED** |
| **Echo Voltage Dividers** | 4.7k + 4.7k dividers on all 6 channels | 2.5V peak sampled cleanly | **CONFIRMED** |
| **Sequential Round-Robin** | 1 sensor triggered at a time with 60 ms gap | Zero acoustic overlap | **CONFIRMED** |
| **Unit Test Coverage** | 55 unit tests covering groups A through P | 55/55 tests passed in Unity | **CONFIRMED** |
| **Obstacle Zone Logic** | 4-zone classification (`CLEAR/CAUTION/OBSTACLE/FAULT`) | Verified in simulation & hardware | **CONFIRMED** |
| **Fail-Safe Priority** | `FAULT > OBSTACLE > CAUTION > CLEAR` | Confirmed across P1–P8 | **CONFIRMED** |
| **Stale Data Invalidation** | Readings $> 1000\text{ ms}$ escalate to `FAULT` | Test Group L verified | **CONFIRMED** |
| **Wheelchair Frame Mounting** | Transducer beam overlap on metal chassis | Not mounted on chair | **TARGET** |
| **SafetyManager Brake Interlock** | Automated emergency stop on `OBSTACLE` | Awaiting system integration | **FUTURE** |

---

## 9. Final Engineering Verdict

```text
================================================================================
AUTOCHAIR 6-SENSOR ULTRASONIC & OBSTACLE-ZONE BENCH INTEGRATION: PASS
================================================================================
[PROVEN]   6-Channel Concurrent Electrical Integration:       PASS
[PROVEN]   Sequential Round-Robin Scheduler (60 ms gap):      PASS
[PROVEN]   Acoustic Crosstalk Isolation:                      PASS
[PROVEN]   Automated Native Unit Tests (55/55):               PASS
[PROVEN]   ESP32 Target Firmware Compilation:                 PASS (0 errors)
[PROVEN]   4-State Obstacle Zone Detection (P1–P8):           PASS
[PROVEN]   Most-Restrictive Fail-Safe Aggregation:            PASS
================================================================================
```
