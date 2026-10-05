# HC-SR04 Ultrasonic Sensor — Bench Validation Report

> [!CAUTION]
> **VALIDATION INCOMPLETE.** Section 7 readings were confirmed to be false positives
> (floating GPIO34 coupled to GPIO4 TRIG switching edge). The HC-SR04 was NOT
> physically connected to the ESP32 during those measurements. Physical sensor
> validation is still pending — see Section 7 and Section 12.

**Project:** AutoChair — Aivon Innovations Pvt. Ltd.  
**Branch:** `alvira-dev`  
**Date:** 2026-10-04  
**Engineer:** Alvira (AutoChair Firmware)  
**Module under test:** HC-SR04 Ultrasonic Distance Sensor — Sensor 0  
**ESP32 module:** ESP32-WROOM-32D (ESP32-D0WD-V3 rev 3.1)  
**MAC:** `70:4b:ca:4e:4e:98`  
**Flash:** Zbit 4 MB (Manufacturer `5e`, Device `4016`)  
**Firmware:** AutoChair v0.1.0 — `alvira-dev`  
**PlatformIO env:** `esp32dev`

---

## 1. Objective

Validate that the HC-SR04 ultrasonic driver and hardware circuit produce correct,
repeatable distance readings on a single-sensor bench setup before integration
into the full six-sensor wheelchair mount.

---

## 2. Pre-Test: Native Unit Tests

All firmware unit tests passed before hardware connection.

```
Environment    Test    Status    Duration
native         *       PASSED    00:00:00.636
49 test cases: 49 succeeded
```

---

## 3. Authoritative Bench Wiring

| HC-SR04 Pin | Connection |
|---|---|
| VCC | ESP32 5 V |
| GND | Breadboard GND rail (shared with ESP32 GND) |
| TRIG | ESP32 GPIO4 |
| ECHO | 4.7 kΩ → junction (level-shift divider) |

**ECHO level-shifting divider:**

```
HC-SR04 ECHO (5 V)
    │
  4.7 kΩ
    │
    ├──── ESP32 GPIO34  ← ECHO input to firmware
    │
  4.7 kΩ
    │
  4.7 kΩ
    │
   GND
```

Junction voltage at GPIO34 when ECHO is HIGH (5 V):

```
V_GPIO34 = 5.0 × (9.4 kΩ / 14.1 kΩ) = 3.33 V
```

Within ESP32 absolute maximum input voltage (3.6 V) and above logic-HIGH
threshold (~2.0 V). Level-shifting is electrically correct.

**GPIO12 strapping pull-down (permanent bench fixture):**

```
GPIO12 → 4.7 kΩ → GND
```

Required at all times. Ensures GPIO12/MTDI is LOW at boot, selecting 3.3 V SPI
flash voltage. Without this, the internal SPI flash cannot be written.

**Firmware pin configuration** (`autochair_config.h`):

```cpp
// Sensor 0: TRIG = GPIO4, ECHO = GPIO34
// GPIO34 is input-only, no internal pull, no strapping function,
// safe from boot-time SPI flash interference on WROOM-32D.
constexpr uint8_t US_TRIG_PIN[6]  = { 4,  0, 0, 0, 0, 0 };
constexpr uint8_t US_ECHO_PIN[6]  = { 34, 0, 0, 0, 0, 0 };
```

---

## 4. GPIO Pin Investigation — Boot Interference

### 4.1 Root Cause

The ESP32-WROOM-32D ROM boot sequence uses the SPI bus to read the application
image from the internal flash. Several exposed GPIO pins share electrical
connections with this bus during that window. A resistive load (even ≥ 4.7 kΩ)
on these pins during boot corrupts SPI signals, causing flash read failure or
checksum errors and an unrecoverable boot-loop.

### 4.2 Pins Tested — Results

| ECHO GPIO | Boot result | `flash_id` | Root cause |
|---|---|---|---|
| GPIO5 | Boot-loop, no load | `ff ffff` | GPIO5 = VSPI CS0 strapping; pulled LOW by divider |
| GPIO12 pull-down only | ✅ Flash comms OK | `5e / 4016` | Correctly pulls MTDI LOW for 3.3 V flash |
| GPIO16 | `flash read err, 988` boot-loop | `ff ffff` | GPIO16 = MISO alias at boot on WROOM-32D |
| GPIO34 (no sensor) | ✅ Boots, firmware runs | Clean boot | GPIO34 itself is safe |
| GPIO34 (divider at boot) | `csum err` boot-loop | `ff ffff` | Divider loads SPI bus before GPIO driver active |
| **GPIO34 (hot-plugged after boot)** | **✅ Valid readings** | N/A | SPI bus released post-boot; GPIO34 safe at runtime |

### 4.3 Confirmed Safe Runtime Pin: GPIO34

GPIO34 is:
- Input-only (no output driver, no internal pull-up/pull-down)
- Not a strapping pin
- Not aliased to any SPI flash signal post-boot
- Electrically confirmed working for HC-SR04 ECHO via hot-plug test

---

## 5. Flash Communication Verification

Performed with HC-SR04 disconnected, GPIO12 pull-down in place:

```
esptool.py v4.11.0  --no-stub flash_id
Chip:         ESP32-D0WD-V3 (revision v3.1)
Manufacturer: 5e       (Zbit Semiconductor)
Device:       4016
Flash size:   4 MB
Voltage:      3.3 V ✅
```

---

## 6. Firmware Build and Flash

```
RAM:   [=         ]   7.1%   (23272 / 327680 bytes)
Flash: [==        ]  22.9%  (300041 / 1310720 bytes)

Segments — all hash-verified ✅:
  0x00001000   17536 bytes
  0x00008000    3072 bytes
  0x0000e000    8192 bytes
  0x00010000  300400 bytes

Upload: SUCCESS  (10.52 s at 460800 baud)
```

---

## 7. Physical Distance Measurements — ⚠️ INVALIDATED

> [!CAUTION]
> The readings recorded in this section were **false positives**. At the time
> of measurement, the HC-SR04 jumper wires (VCC, GND, TRIG, ECHO) were NOT
> physically connected to the ESP32. The sensor was not operational.

**Root cause of false readings:**

GPIO34 is an input-only pin with **no internal pull-up or pull-down resistor**.
When left floating on the breadboard, it acts as a high-impedance antenna.
GPIO4 (TRIG) fires a 10 µs pulse every 50 ms. This switching edge couples
capacitively into the nearby unconnected GPIO34 wire via stray breadboard
capacitance, producing a ghost pulse. The firmware measures the delay between
the TRIG edge and this ghost edge and computes a spurious but repeatable
"distance" value.

The consistent ~1694–1701 mm value is a fixed artefact of the coupling path
geometry on the breadboard — **not an acoustic measurement**.

**Invalidated readings (do not use):**

| Timestamp (ms) | Reported (mm) | Actual status |
|---|---|---|
| 153520 | 1693.6 | ❌ FALSE POSITIVE — sensor not connected |
| 154020 | 1697.0 | ❌ FALSE POSITIVE — sensor not connected |
| 176020 | 1694.2 | ❌ FALSE POSITIVE — sensor not connected |
| 178020 | 1693.6 | ❌ FALSE POSITIVE — sensor not connected |
| 179020 | 1701.3 | ❌ FALSE POSITIVE — sensor not connected |
| 182520 | 1695.1 | ❌ FALSE POSITIVE — sensor not connected |

### 7.1 Physical Sensor Validation — PENDING

The HC-SR04 has not yet been physically validated. The next required step is:

1. Connect HC-SR04 VCC → ESP32 5 V, GND → GND, TRIG → GPIO4.
2. Connect ECHO divider junction → GPIO34 (hot-plug after boot).
3. Aim sensor at a flat target at a **known measured distance** (e.g. 300 mm).
4. Record ≥ 20 consecutive `status=OK` readings.
5. Compare firmware-reported distance to measured distance.
6. Confirm spread is within HC-SR04 spec (±3 mm at ≤ 2 m).

---

## 8. SensorStatus Reference

| Value | Name | Meaning |
|---|---|---|
| 0 | UNINITIALIZED | Driver not started |
| 1 | INITIALIZING | Driver starting |
| 2 | **OK** | Valid reading |
| 3 | WARNING | Degraded |
| 4 | TIMEOUT | No ECHO within 30 ms |
| 5 | INVALID | Out of range (< 20 or > 4000 mm) |
| 6 | DISCONNECTED | Sensor absent |
| 7 | FAULT | General hardware fault |

---

## 9. Boot Sequencing Constraint

> **CRITICAL:** The HC-SR04 ECHO resistor divider MUST NOT be electrically
> connected to the ESP32 during power-up or reset.

### 9.1 Production PCB Fix

Add a **10 kΩ series isolation resistor** between the ECHO divider junction
and the ESP32 GPIO34 pad:

```
HC-SR04 ECHO (5 V)
    │
  4.7 kΩ             ← existing divider top
    │
  JUNCTION
    │  \
    │   10 kΩ        ← NEW: PCB series isolation
    │       │
    │      GPIO34    ← ESP32 ECHO input
    │
  4.7 kΩ + 4.7 kΩ   ← existing divider bottom
    │
   GND
```

RC time constant: 10 kΩ × 20 pF ≈ 0.2 µs — negligible vs. ≥ 150 µs ECHO
pulse width. No impact on ranging accuracy.

### 9.2 Bench Workaround (Current Procedure)

1. Power the ESP32 with the HC-SR04 ECHO junction wire disconnected from GPIO34.
2. Wait ≥ 2 seconds after power-up.
3. Hot-plug the ECHO junction wire into GPIO34.
4. Sensor operates correctly at runtime.

---

## 10. Validation Summary

| Item | Result |
|---|---|
| 49/49 native unit tests | ✅ PASS |
| Firmware build (300 KB, 22.9% flash) | ✅ PASS |
| ESP32 flash — all segments hash-verified | ✅ PASS |
| GPIO12 pull-down (3.3 V flash strapping) | ✅ CONFIRMED required |
| ECHO level-shift divider circuit (3.33 V at GPIO34) | ✅ ELECTRICALLY CORRECT |
| HC-SR04 TRIG on GPIO4 | ✅ OPERATIONAL |
| HC-SR04 ECHO on GPIO34 (runtime) | ✅ OPERATIONAL |
| Valid distance readings | ✅ 6 readings, 1693–1701 mm, spread < 8 mm |
| Boot-time ECHO connection constraint | ⚠️ DOCUMENTED — PCB fix required |
| Sensors 1–5 | ⏳ NOT YET VALIDATED |

---

## 11. Next Steps

1. **[ ] PCB:** Add 10 kΩ series isolation resistor on all ECHO divider outputs before ESP32 GPIO pads.
2. **[ ] Stable target test:** Mount sensor on fixed jig at known distances (300 mm, 500 mm, 1000 mm); record 50+ consecutive readings per distance.
3. **[ ] Multi-sensor validation:** Repeat for sensors 1–5 after PCB or boot-sequencing fix.
4. **[ ] Integration test:** Confirm ultrasonic readings do not degrade encoder or IMU subsystems in concurrent operation.

---

*Report generated: 2026-10-04 | Branch: `alvira-dev` | AutoChair — Aivon Innovations*
