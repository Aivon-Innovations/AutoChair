# AutoChair — Ultrasonic Obstacle Zone Validation Report

**Project:** Aivon Innovations — AutoChair  
**Branch:** alvira-dev  
**Date started:** 23 October 2026  
**Status:** IN PROGRESS — Software tests pending; physical validation pending

---

> [!WARNING]
> This report documents obstacle detection and zone classification only.
> It does NOT claim automatic braking, collision prevention, safety certification,
> validated wheelchair-mounted performance, or fully autonomous navigation.
> Threshold values are UNVALIDATED PROVISIONAL ENGINEERING PARAMETERS.

---

## 1. Threshold Configuration and Rationale

### Source
All thresholds defined in [`firmware/esp32/include/autochair_config.h`](../firmware/esp32/include/autochair_config.h).

### Values

| Parameter | Value | Units | Label |
|-----------|-------|-------|-------|
| `ZONE_OBSTACLE_MM` | 400 | mm | UNVALIDATED PROVISIONAL |
| `ZONE_CAUTION_MM` | 800 | mm | UNVALIDATED PROVISIONAL |
| `ULTRASONIC_MIN_MM` | 20 | mm | HC-SR04 hardware blind zone |
| `ULTRASONIC_MAX_MM` | 4000 | mm | HC-SR04 maximum valid range |
| `ULTRASONIC_TIMEOUT_US` | 30,000 | µs | ~5.1 m round-trip at 343 m/s |
| `ULTRASONIC_STALE_THRESHOLD_MS` | 1,000 | ms | Reading age cutoff |
| `ULTRASONIC_MAX_FAILURES` | 5 | count | Consecutive failures before FAULT |

### Rationale for Provisional Values

- **400 mm (40 cm) obstacle threshold:** Approximately one arm's reach from the sensor face. Objects closer than this represent immediate proximity requiring operator awareness.
- **800 mm (80 cm) caution threshold:** Approach zone. At typical powered wheelchair speeds (≤1.5 m/s), this provides ~0.5 s reaction time — a provisional starting point only.
- **Neither value is related to wheelchair stopping distance or braking performance.**
- These values must be validated experimentally against actual chair speed and stopping distance before any safety use.

---

## 2. Zone Definitions and Boundary Behaviour

Implemented in [`firmware/esp32/src/sensors/ultrasonic/obstacle_zone.cpp`](../firmware/esp32/src/sensors/ultrasonic/obstacle_zone.cpp).

| Zone | Condition | Boundary behaviour |
|------|-----------|-------------------|
| `FAULT` | status ≠ OK, OR distance_mm < 0, OR reading age > 1000 ms | Evaluated first — overrides all distance checks |
| `OBSTACLE` | distance_mm < 400 mm AND not FAULT | 399.9 mm → OBSTACLE; 400.0 mm → CAUTION |
| `CAUTION` | 400 mm ≤ distance_mm < 800 mm AND not FAULT | 799.9 mm → CAUTION; 800.0 mm → CLEAR |
| `CLEAR` | distance_mm ≥ 800 mm AND not FAULT | Lowest severity |

**Priority order (highest → lowest):** FAULT > OBSTACLE > CAUTION > CLEAR  
Represented as uint8_t enum values 3, 2, 1, 0 — aggregate uses max().

---

## 3. Six-Sensor Aggregation and Fault Policy

**Aggregation rule:** Overall zone = most restrictive (maximum severity) across all six sensors.

**Fault policy:** A single FAULT sensor makes the overall zone FAULT. There is no degraded mode. Missing or stale data is never treated as CLEAR.

**Sensor-level fault escalation:**
- 1–5 consecutive timeouts → status = `TIMEOUT` (driver keeps updating)
- 6th consecutive timeout → status = `FAULT` (driver stops updating until reset)
- Any valid echo → failure counter resets to 0, status → `OK`

**Stale reading:** If a sensor's last update timestamp is more than 1000 ms old (at zone classification time), that reading is classified as `FAULT` regardless of the stored distance.

---

## 4. Six-Sensor Physical Validation — Connection Test Evidence

All six sensors confirmed working individually on 22–23 October 2026.

| Sensor | TRIG | ECHO | Confirmed | Best reading | R2→GND fixed |
|--------|------|------|:-:|---|:-:|
| 1 | GPIO4 | GPIO34 | ✅ | 4.6 cm | Pending |
| 2 | GPIO5 | GPIO35 | ✅ | 5.3 cm | ✅ |
| 3 | GPIO18 | GPIO36 (VP) | ✅ | 16.2 cm | ✅ |
| 4 | GPIO19 | GPIO39 (VN) | ✅ | 2.2 cm | ✅ |
| 5 | GPIO23 | GPIO32 | ✅ | TOO CLOSE | Pending |
| 6 | GPIO13 | GPIO33 | ✅ | 267.3 cm | Pending |

Root cause of all NO ECHO failures: voltage divider R2 bottom not connected to GND rail. Full details in `SIX_SENSOR_CONNECTION_TEST_REPORT.md`.

---

## 5. Automated Test Results

**Test file:** [`firmware/esp32/test/test_ultrasonic_integration.cpp`](../firmware/esp32/test/test_ultrasonic_integration.cpp)  
**Test framework:** Unity  
**Build environment:** `native_test` (no ESP32 hardware required)

### Test Groups

| Group | Area tested | Tests | Result |
|-------|-------------|-------|--------|
| A | GPIO pin configuration and mapping | 7 | PENDING |
| B | Sensor ID association | 2 | PENDING |
| C | Round-robin scheduling | 3 | PENDING |
| D | Update timing / starvation detection | 2 | PENDING |
| E | Timeout and invalid/no-echo handling | 4 | PENDING |
| F | Fault isolation (one bad sensor) | 2 | PENDING |
| G | Telemetry identification | 2 | PENDING |
| H | Startup / reset behaviour | 3 | PENDING |
| I | Zone classification (threshold values) | 11 | PENDING |
| J | Zone state transitions | 3 | PENDING |
| K | Invalid and timed-out → FAULT zone | 2 | PENDING |
| L | Stale data detection | 3 | PENDING |
| M | Recovery after fault | 2 | PENDING |
| N | Multiple sensors / different zones | 1 | PENDING |
| O | Aggregate zone (most-restrictive) | 3 | PENDING |
| P | Threshold configuration values | 4 | PENDING |
| **Total** | | **54** | **PENDING** |

> [!NOTE]
> Results will be populated after `pio test -e native_test` completes.

### Build Result

| Target | Result | Flash | RAM |
|--------|--------|-------|-----|
| native_test | PENDING | N/A | N/A |
| esp32dev | PENDING | TBD | TBD |

---

## 6. Physical Validation Table

Tests to be conducted after software build and tests pass. One test at a time, guided by Antigravity.

**Hardware:** ESP32-WROOM-32D + 6 × HC-SR04 + breadboard voltage dividers (R1=R2=4.7 kΩ)  
**Measurement tool:** Tape measure  
**Telemetry capture:** Serial Monitor at 115200 baud

| # | Test | Sensor(s) | Target Distance | Expected Zone | Actual Telemetry | Actual Zone | Result |
|---|------|-----------|---------------:|---------------|-----------------|-------------|--------|
| P1 | No obstacle — all CLEAR | All | > 800 mm | CLEAR | PENDING | — | — |
| P2 | Object in CAUTION range | S2 (front-centre) | 600 mm | CAUTION | PENDING | — | — |
| P3 | Object in OBSTACLE range | S2 (front-centre) | 200 mm | OBSTACLE | PENDING | — | — |
| P4 | Far-to-near threshold crossing | S2 | 900→600→200 mm | CLEAR→CAUTION→OBSTACLE | PENDING | — | — |
| P5 | Near-to-far recovery | S2 | 200→600→900 mm | OBSTACLE→CAUTION→CLEAR | PENDING | — | — |
| P6 | Multi-sensor: S1 CAUTION, S2 OBSTACLE | S1+S2 | S1:600mm, S2:200mm | OBSTACLE | PENDING | — | — |
| P7 | Sensor timeout behaviour (no target) | S4 | > 4000 mm | FAULT (timeout) | PENDING | — | — |
| P8 | Recovery after obstacle removed | S2 | 200 mm → clear | OBSTACLE→CLEAR | PENDING | — | — |

---

## 7. Known Limitations and Unresolved Issues

| # | Issue | Severity | Status |
|---|-------|----------|--------|
| 1 | HC-SR04 blind zone: readings < 20 mm are INVALID | Low | By design |
| 2 | Acoustic crosstalk between adjacent sensors not yet characterised | Medium | Pending physical test |
| 3 | Stale threshold (1000 ms) not validated against actual update latency | Medium | Pending timing analysis |
| 4 | Sensors 1, 5, 6: R2→GND connection not confirmed — may still intermittent | High | Action required before physical tests |
| 5 | Threshold values not related to actual wheelchair stopping distance | High | Explicitly labelled PROVISIONAL |
| 6 | Single-sensor FAULT causes overall FAULT — no degraded mode defined | Medium | Matches current safety requirement (fail-safe) |

---

## 8. Final Status

| Area | Status |
|------|--------|
| Six-sensor physical connection validation | ✅ All 6 sensors individually confirmed |
| Obstacle-zone logic implementation | ✅ Complete (`obstacle_zone.h/.cpp`) |
| Supporting firmware architecture | ✅ Complete (driver, manager, types, IO abstraction) |
| Native test suite | ⏳ Pending test run |
| ESP32 firmware build | ⏳ Pending build |
| Physical validation tests P1–P8 | ⏳ Pending |

---

*Report in progress — Aivon Innovations AutoChair, alvira-dev branch*
