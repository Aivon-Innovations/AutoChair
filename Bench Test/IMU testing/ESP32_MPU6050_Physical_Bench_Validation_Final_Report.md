# ESP32 + MPU6050 — Standalone Bench Validation Report

**Date:** 2026-10-05  
**Sensor:** MPU6050 (Silicon verified via WHO_AM_I = 0x68)  
**MCU:** ESP32-D0WD-V3 (rev 3.1), 240 MHz  
**I²C Pins:** SDA = GPIO21, SCL = GPIO22  
**I²C Address:** 0x68  
**Supply:** VCC = 3.3V, GND = ESP32 GND  
**Firmware:** ESP32 Arduino / Wire.h, PlatformIO esp32dev @ 115200 baud  
**COM Port:** COM8 (Silicon Labs CP210x)  

---

## Overall Result: ✅ PASS — All tests passed

---

## Test Results Summary

| # | Test | Result | Key Measurement |
|---|------|--------|-----------------|
| T1 | I²C Bus Detection | ✅ PASS | Device found at 0x68, 10/10 probes stable |
| T2 | Sensor Identification | ✅ PASS | WHO_AM_I = 0x68 → Confirmed MPU6050 |
| T3 | Stationary Accelerometer (5s) | ✅ PASS | \|A\| avg ≈ 0.984 g (expected ~1.000 g) |
| T4 | Stationary Gyroscope (5s) | ✅ PASS | Bias: X=+1.92, Y=+1.16, Z=+0.33 °/s |
| T5.1 | X-Axis Tilt | ✅ PASS | Clear ΔAx response detected |
| T5.2 | Y-Axis Tilt | ✅ PASS | Clear ΔAy response detected |
| T5.3 | X-Axis Rotation (Gyro) | ✅ PASS | Peak \|Gx\| ≥ 20 °/s confirmed |
| T5.4 | Y-Axis Rotation (Gyro) | ✅ PASS | Peak \|Gy\| ≥ 20 °/s confirmed |
| T5.5 | Z-Axis Rotation (Gyro) | ✅ PASS | Peak \|Gz\| ≥ 20 °/s confirmed |
| T6 | 60s Stationary Stability | ✅ PASS | 300 samples, 0 errors, 0 spikes, StdDev < 0.05 g |
| T7A | Communication Fault Detection | ✅ PASS | NACK detected at t = 0.00s (Error Code 2) |
| T7B | Communication Bus Recovery | ✅ PASS | Full recovery in 0.27s, \|A\| = 0.955 g post-recovery |

**Total: 12 / 12 PASS — 0 FAIL**

---

## Silicon Identification

| Register | Value | Interpretation |
|---|---|---|
| WHO_AM_I (0x75) | 0x68 | MPU6050 (authentic silicon) |
| Repeated reads (5×) | 5/5 = 0x68 | Stable, no corruption |

> **Conclusion:** The IC die is confirmed as an MPU6050. The breakout board label was consistent with the silicon identity.

---

## Stationary Noise Characterization

| Axis | Stationary Bias | Notes |
|---|---|---|
| Gyro X | +1.92 °/s | Normal zero-rate offset |
| Gyro Y | +1.16 °/s | Normal zero-rate offset |
| Gyro Z | +0.33 °/s | Excellent — minimal bias |
| Accel \|A\| | 0.984 g avg | Within ±1.6% of 1g — excellent |

---

## Dynamic Motion Response

| Test | Axis | Threshold | Verdict |
|---|---|---|---|
| Tilt X | Accel X | ΔAx ≥ 0.15 g | ✅ Exceeded |
| Tilt Y | Accel Y | ΔAy ≥ 0.15 g | ✅ Exceeded |
| Rotation X | Gyro X | ΔGx ≥ 20 °/s | ✅ Exceeded |
| Rotation Y | Gyro Y | ΔGy ≥ 20 °/s | ✅ Exceeded |
| Rotation Z | Gyro Z | ΔGz ≥ 20 °/s | ✅ Exceeded |

---

## Communication Fault & Recovery

| Phase | Outcome | Detail |
|---|---|---|
| Fault Injection | ✅ Detected instantly | SDA wire pulled → NACK Error Code 2 at t = 0.00s |
| Bus Recovery | ✅ Recovered in 0.27s | Wire.end()/Wire.begin() hard reset + initMPU6050() |
| Post-Recovery Data | ✅ Valid | \|A\| = 0.955 g immediately after reconnection |

> **Note:** Recovery required an explicit I²C peripheral reset (`Wire.end()` + `Wire.begin()`) — simple re-probe without bus reset was insufficient on ESP32.

---

## Known Issues & Observations

- **ESP32 I²C repeated-start bug:** `Wire.endTransmission(false)` caused read dropouts. Fixed by using explicit `Wire.endTransmission(true)` + `Wire.requestFrom(..., true)` (full stop-restart cycle).
- **Stuck bus after disconnect:** After physical SDA removal, the ESP32 I²C peripheral enters a hung state. Recovery requires `Wire.end()` / `Wire.begin()` before re-probing.

---

## Interface Readiness Assessment

| Interface | Status |
|---|---|
| I²C Bus | ✅ Ready |
| MPU6050 Accelerometer | ✅ Ready |
| MPU6050 Gyroscope | ✅ Ready |
| Long-duration Stability | ✅ Ready |
| Fault Detection & Recovery | ✅ Ready |

### Verdict: **SENSOR INTERFACE VALIDATED — READY FOR INTEGRATION**

The MPU6050 sensor connected to the ESP32 via GPIO21/GPIO22 has passed all 12 bench validation tests. The hardware interface is stable, responsive, and fault-tolerant. The sensor is cleared for the next phase of development.
