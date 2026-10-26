# AutoChair ESP32 — Software & Firmware Architecture Status

**Document Identification:** `AutoChair-alvira-dev/encoder today/04_SOFTWARE_AND_FIRMWARE_STATUS.md`  
**Date:** 2026-10-23  
**Project:** AutoChair — AI-Powered Smart Wheelchair  
**Subsystem:** ESP32 Firmware Stack, Sensor Drivers, Odometry & Safety Integration  
**Status:** VERIFIED FIRMWARE IMPLEMENTATION & BUILD STATUS  

---

## 1. Five Mandatory Engineering Distinctions

To maintain absolute safety integrity, the incoming engineering team must enforce the strict separation of these five distinct domains:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                       FIVE SEPARATED ENGINEERING DOMAINS                     │
├─────────────────────────────────────────────────────────────────────────────┤
│ 1. Software Unit Tests Passed       │ 119/119 native tests pass on host     │
│ 2. Firmware Compiled Successfully   │ Clean ESP32 ELF/BIN build with 0 errs │
│ 3. Firmware Uploaded and Booted     │ ESP32 booted on COM8 into IDLE state  │
│ 4. Physical Hardware Observed       │ Manual pulses & interrupts observed   │
│ 5. Physical Wheelchair Safety       │ BLOCKED — NOT ESTABLISHED             │
└─────────────────────────────────────────────────────────────────────────────┘
```

> [!CAUTION]
> **Safety Warning:**  
> Passing native unit tests, building clean firmware, and observing bench pulses **DOES NOT prove that the physical wheelchair is safe to operate**. Closed-loop motor control, mechanical calibration, wheel diameter measurements, and wheelchair battery power remain strictly unverified.

---

## 2. Firmware Architecture & Source Layout

The encoder subsystem is structured as a decoupled, multi-tier hardware abstraction layer:

```text
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
                         ┌────────────────┴────────────────┐
                         ▼                                 ▼
           ┌───────────────────────────┐     ┌───────────────────────────┐
           │     Left EncoderDriver    │     │    Right EncoderDriver    │
           │(sensors/encoder/          │     │(sensors/encoder/          │
           │ encoder_driver.cpp)       │     │ encoder_driver.cpp)       │
           └─────────────┬─────────────┘     └─────────────┬─────────────┘
                         │                                 │
                         ▼                                 ▼
           ┌───────────────────────────┐     ┌───────────────────────────┐
           │    HardwareEncoderHAL     │     │    HardwareEncoderHAL     │
           │ (Left Instance: ID=0)     │     │ (Right Instance: ID=1)    │
           │ PinA=25, PinB=26, PinZ=27 │     │ PinA=32, PinB=17, PinZ=14 │
           └─────────────┬─────────────┘     └─────────────┬─────────────┘
                         │                                 │
                         ▼                                 ▼
                   [Left Encoder]                    [Right Encoder]
```

### 2.1 Key Source Code Locations
* **Configuration:** [`firmware/esp32/include/autochair_config.h`](file:///c:/Users/targa/OneDrive/Desktop/AutoChair-alvira-dev/AutoChair-alvira-dev/firmware/esp32/include/autochair_config.h)
  * Defines pin allocations, debounce intervals, odometry dimensions, and timeout thresholds.
* **Hardware Abstraction Layer:** [`firmware/esp32/src/sensors/encoder/encoder_hal.cpp`](file:///c:/Users/targa/OneDrive/Desktop/AutoChair-alvira-dev/AutoChair-alvira-dev/firmware/esp32/src/sensors/encoder/encoder_hal.cpp)
  * Implements `isrA`, `isrB`, `isrZ` interrupt handlers, the 4-bit `QUAD_TABLE[16]`, X4 edge decoding, and the timestamped `handleIndex()` filter.
* **Driver Tier:** [`firmware/esp32/src/sensors/encoder/encoder_driver.cpp`](file:///c:/Users/targa/OneDrive/Desktop/AutoChair-alvira-dev/AutoChair-alvira-dev/firmware/esp32/src/sensors/encoder/encoder_driver.cpp)
  * Tracks cumulative count, direction, signed RPM, pulses per cycle, and zero-drift health.
* **Manager Tier:** [`firmware/esp32/src/sensors/encoder/encoder_manager.cpp`](file:///c:/Users/targa/OneDrive/Desktop/AutoChair-alvira-dev/AutoChair-alvira-dev/firmware/esp32/src/sensors/encoder/encoder_manager.cpp)
  * Exposes synchronized dual-wheel telemetry (`WheelEncoderData`), health monitoring, and stall detection.
* **Odometry:** [`firmware/esp32/src/navigation/differential_odometry.cpp`](file:///c:/Users/targa/OneDrive/Desktop/AutoChair-alvira-dev/AutoChair-alvira-dev/firmware/esp32/src/navigation/differential_odometry.cpp)
  * Forward kinematics ($x, y, \theta$) driven by wheel tick deltas.
* **Safety Integration:** [`firmware/esp32/src/safety/safety_manager.cpp`](file:///c:/Users/targa/OneDrive/Desktop/AutoChair-alvira-dev/AutoChair-alvira-dev/firmware/esp32/src/safety/safety_manager.cpp)
  * Evaluates encoder health, sensor staleness, coverage masks, and motion authorization.

---

## 3. Subsystem Implementation Details

### 3.1 Z-Index Debounce Filter Analysis
* **Configuration:** [`autochair_config.h:L125`](file:///c:/Users/targa/OneDrive/Desktop/AutoChair-alvira-dev/AutoChair-alvira-dev/firmware/esp32/include/autochair_config.h#L125) sets `constexpr uint32_t ENCODER_MIN_INDEX_INTERVAL_MS = 20;`.
* **HAL Implementation ([`encoder_hal.cpp:L164-L179`](file:///c:/Users/targa/OneDrive/Desktop/AutoChair-alvira-dev/AutoChair-alvira-dev/firmware/esp32/src/sensors/encoder/encoder_hal.cpp#L164-L179)):**
  ```cpp
  void HardwareEncoderHAL::handleIndex(uint32_t timestampMs) {
  #ifndef ENV_NATIVE
      uint32_t now = (timestampMs != 0) ? timestampMs : millis();
  #else
      uint32_t now = timestampMs;
  #endif

      // Reject events that occur faster than the configured minimum interval.
      // Handles 32-bit unsigned timer wraparound safely via modular arithmetic.
      if (!_hasIndex || (static_cast<uint32_t>(now - _lastIndexMs) >= _minIntervalMs)) {
          _indexCount++;
          _lastIndexMs = now;
          _hasIndex = true;
      }
      _lastInterruptMs = now;
  }
  ```
* **What the 20 ms Debounce Filters:**
  * High-frequency contact bounce, capacitive ringing, and noise spikes occurring within $< 20\text{ ms}$ of a preceding rising edge.
  * Pulse rates up to $3,000\text{ RPM}$ ($\frac{60,000}{20\text{ ms}} = 3,000\text{ RPM}$).
* **What the 20 ms Debounce Does NOT Guarantee:**
  * **Spurious Edges $> 20\text{ ms}$:** Low-frequency EMI or 50/60 Hz noise spikes separated by $\ge 20\text{ ms}$ will each trigger an index increment.
  * **Mechanical Rocking / Boundary Hesitation:** If the wheel stops on an optical slit edge and oscillates back and forth with a period $> 20\text{ ms}$, multiple index events will be accepted from the same physical slit.
  * **Quadrature Tick Gating:** It does not verify that $\approx 2,400$ quadrature pulses have occurred between index events.
  * **Pulse Width / Hold Time:** It triggers on the rising edge without verifying high-level duration.

### 3.2 Differential-Drive Odometry
* **Kinematics:** Implements standard midpoint Euler integration for two-wheel differential drive:
  $$\Delta s = \frac{\Delta s_L + \Delta s_R}{2}, \quad \Delta \theta = \frac{\Delta s_R - \Delta s_L}{W}$$
  $$x_{k+1} = x_k + \Delta s \cos\left(\theta_k + \frac{\Delta \theta}{2}\right), \quad y_{k+1} = y_k + \Delta s \sin\left(\theta_k + \frac{\Delta \theta}{2}\right)$$
* **Calibration Warning:** Odometry computations currently use nominal placeholder values ($W = 0.58\text{ m}$, wheel diameter $= 0.32\text{ m}$, $2,400\text{ CPR}$). These values are uncalibrated placeholders.

### 3.3 Encoder Health Monitoring & Stall Detection
* Evaluates dynamic encoder state:
  * `EncoderHealthStatus::OK` (Normal operation, valid pulse progression).
  * `EncoderHealthStatus::STALLED` (Target speed commanded but pulse delta is zero for $> \text{threshold}$).
  * `EncoderHealthStatus::FAULT` (Invalid transitions detected in `QUAD_TABLE`, `inv > 0`).

### 3.4 Safety Manager Integration & Fault Injection
* Completed in Task 5 Stages A and B:
  * `SafetyManager` monitors dual encoder health, IMU freshness, and ultrasonic coverage.
  * Latches hard faults on unrecoverable sensor failures; requires explicit `CMD:RESET_FAULT` to clear.
  * E-stop assertion overrides all motion authorization within $< 10\text{ ms}$.

---

## 4. Test Suite Execution & Build Verification

### 4.1 Host Native Unit Test Suite (`pio test -e native`)
* **Total Tests:** **119 / 119 PASSED (100% Success)**
* **Category Breakdown:**
  * Core State Machine & Transitions: 15 tests
  * HC-SR04 Driver & Ultrasonic Manager: 15 tests
  * IMU (MPU6050) Driver: 8 tests
  * Wheel Encoder Driver & Manager: 16 tests
  * Differential Odometry Kinematics: 18 tests
  * Encoder Health & Stall Diagnostics: 9 tests
  * Safety Manager Integration & Motion Permits: 13 tests
  * System-Level Fault Injection (Stage A): 10 tests
  * Sensor-Safety Integration & Obstacle Policy (Stage B): 9 tests

### 4.2 ESP32 Target Build Verification (`pio run -e esp32dev`)
* **Target:** Espressif ESP32 Dev Module (ESP32-WROOM-32D)
* **Toolchain:** xtensa-esp32-elf-g++ 8.4.0
* **Status:** **SUCCESS (0 errors, 0 warnings)**
* **Memory Footprint:**
  * RAM Usage: **23,540 bytes** (7.2% of 327,680 bytes available)
  * Flash Usage: **309,925 bytes** (23.6% of 1,310,720 bytes available)

### 4.3 Flashing and Boot Verification
* Upload to `COM8` at 460800 baud was verified (23.5s upload time).
* Boot output confirms clean initialization:
  ```text
  [DEBUG][0][Main] State=3 Safety=0 Mode=0
  [DEBUG][0][Encoder] L: cnt=0 dir=0 rpm=0.00 idx=0 inv=0 st=2 rsn=NONE | R: cnt=0 dir=0 rpm=0.00 idx=0 inv=0 st=2 rsn=NONE
  [DEBUG][0][Odometry] Pose: x=0.000 m y=0.000 m th=0.00 deg v=0.00 m/s w=0.00 rad/s valid=1
  ```
* System runs in `IDLE` state (`State=3`), `Safety=0` (Clear), `Mode=0` (Simulation).
