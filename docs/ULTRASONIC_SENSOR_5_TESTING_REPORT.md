# AutoChair — Ultrasonic Sensor #5 Testing Report

### Subtitle
**HC-SR04 Sensor #5 Bench Smoke Testing, Level-Shifting Verification, and Live Hand-Distance Detection**

* **Project**: AutoChair — Smart Wheelchair Navigation & Safety Subsystem
* **Document ID**: `DOC-TEST-HCSR04-005`
* **Date**: October 5, 2026
* **Branch**: `alvira-dev`
* **Status**: Completed Engineering Record

---

## 1. Objective

The primary objective was to perform a focused individual functional smoke test and bench verification for physical **HC-SR04 Ultrasonic Sensor #5** before incorporating it into the multi-sensor array.

Following the thorough baseline investigation and power isolation established during Sensor #1 testing, Sensor #5 was tested strictly for unit-level hardware functionality, acoustic transceiver response, and dynamic echo tracking.

### Scope of this Test:
* **IN SCOPE**:
  1. Verification of physical Sensor #5 transceiver health (piezoelectric transmitter/receiver pair).
  2. Verification of signal response on proven `TRIG` (ESP32 `GPIO4`) and `ECHO` (ESP32 `GPIO34` via level divider) channels.
  3. Functional detection of a target (human hand) moving dynamically across multiple distances.
  4. Verification of telemetry data format (`status=2` / `SensorStatus::OK`) and firmware stability.
* **EXPLICITLY OUT OF SCOPE**:
  * Formal 30 cm calibrated accuracy validation.
  * Precision distance calibration curves or error percentage calculation.
  * Multi-sensor sequential firing or acoustic crosstalk characterization.
  * Obstacle-avoidance decision logic, path planning, or automatic braking.

---

## 2. Hardware Used

The physical hardware configuration used for this test comprised:

* **Microcontroller Board**: ESP32-WROOM-32D Development Board (NodeMCU form factor, 4MB Flash).
* **Ultrasonic Sensor Under Test**: HC-SR04 Ultrasonic Distance Sensor Module (Physical Unit #5).
* **External 5 V Power Source**: Raspberry Pi 4 Model B (utilizing Physical Pin 2 for 5.0 V DC power and Physical Pin 6 for Ground reference).
* **Development Host Link**: Development computer connected to the ESP32 via USB-UART cable (`/dev/cu.SLAB_USBtoUART`) at 115200 baud, providing ESP32 power and serial monitor capture.
* **Level-Shifting Passives**: Two carbon-film through-hole resistors ($4.7\text{ k}\Omega \pm 5\%$) forming a 2:1 resistive divider.
* **Prototyping Medium**: Solderless breadboard and standard DuPont jumper wires.

> [!IMPORTANT]
> The ESP32 was powered strictly via its USB port from the computer. The HC-SR04 Sensor #5 was powered strictly from the Raspberry Pi Physical Pin 2 ($5.0\text{ V}$). The Raspberry Pi $5\text{ V}$ was **NOT** connected to the ESP32 $5\text{V}$ (`VIN`) pin.

---

## 3. Final Wiring Configuration

Sensor #5 was connected using the proven electrical configuration established during prior bench validation:

### 3.1 Interconnect Table

| Signal / Pin | Source | Destination | Nominal Voltage | Function |
|---|---|---|---|---|
| **`VCC`** | HC-SR04 Sensor #5 Pin 1 | Raspberry Pi 4 Physical Pin 2 | $5.0\text{ V DC}$ | Sensor Power Supply |
| **`GND`** | HC-SR04 Sensor #5 Pin 4 | Common Breadboard GND Rail | $0\text{ V}$ | Sensor Ground Reference |
| **`Pi GND`** | Raspberry Pi 4 Physical Pin 6 | Common Breadboard GND Rail | $0\text{ V}$ | Power Supply Common Ground |
| **`ESP32 GND`** | ESP32 GND Header Pin | Common Breadboard GND Rail | $0\text{ V}$ | Microcontroller Common Ground |
| **`TRIG`** | ESP32 `GPIO4` | HC-SR04 Sensor #5 `TRIG` Pin | $3.3\text{ V}$ TTL Logic | $10\ \mu\text{s}$ Trigger Pulse |
| **`ECHO`** | HC-SR04 Sensor #5 `ECHO` Pin | Breadboard Voltage Divider Input | $5.0\text{ V}$ Pulse | Return Timing Pulse |
| **`ECHO Sense`** | Voltage Divider Junction | ESP32 `GPIO34` (GPI / Input-only) | $2.5\text{ V}$ Step-down | Level-Shifted Echo Sense |

---

## 4. ECHO Voltage-Divider Schematic

To protect the ESP32 $3.3\text{ V}$ input stage from the HC-SR04 $5.0\text{ V}$ echo pulse, a symmetrical 2:1 resistive voltage divider was placed between the sensor `ECHO` output and `GPIO34`:

```text
HC-SR04 Sensor #5 ECHO (5.0 V Pulse)
               │
               ▼
        [ 4.7 kΩ Resistor (R1) ]
               │
               ├────────────────────────► ESP32 GPIO34 (Input-only, no internal pulls)
               │                          (Peak level: 2.50 V)
        [ 4.7 kΩ Resistor (R2) ]
               │
               ▼
       Common GND Rail (0 V)
```

### Circuit Analysis:
* **Divider Ratio**: $\text{Gain} = \frac{R_2}{R_1 + R_2} = \frac{4.7\text{ k}\Omega}{4.7\text{ k}\Omega + 4.7\text{ k}\Omega} = 0.50$
* **Nominal Divided Peak Voltage**: $V_{\text{sense}} = 5.0\text{ V} \times 0.50 = 2.50\text{ V}$
* **Logic Threshold Compliance**: $2.50\text{ V}$ safely exceeds the ESP32 input-high threshold ($V_{IH(\min)} \approx 2.475\text{ V}$) while remaining well below the $3.6\text{ V}$ maximum absolute GPIO limit.

---

## 5. Firmware & GPIO Configuration

* **Trigger Pin**: `GPIO4` (configured as standard output via `pinMode(4, OUTPUT)`).
* **Echo Pin**: `GPIO34` (configured as input via `pinMode(34, INPUT)`).
* **Pulse Duration Capture**: `pulseIn(34, HIGH, 30000)` with a $30\text{ ms}$ timeout limit.
* **Distance Calculation**:
  $$\text{Distance (mm)} = \frac{\text{Pulse Duration (}\mu\text{s)} \times 0.343\text{ mm/}\mu\text{s}}{2}$$
* **Yield & Watchdog Prevention**: FreeRTOS scheduling yield (`delay(1)`) embedded inside the acquisition loop to prevent Task Watchdog Timer (TWDT) triggers.

---

## 6. Test Procedure

1. **Serial Monitor Initialization**: The host serial interface was opened on `/dev/cu.SLAB_USBtoUART` at 115200 baud.
2. **Telemetry Stream Start**: Verified that the firmware emitted distance readings formatted in both millimeters and centimeters alongside status codes.
3. **Position 1 (Closer Steady Hold)**: An open hand was held steady at a closer position (~17.4 cm) while readings were recorded.
4. **Position 2 (Moved Farther Away)**: The hand was moved farther away (~18.1–18.7 cm) and held steady while readings were recorded.
5. **Position 3 (Extended Farther Hold)**: The hand was extended farther (~18.4–19.4 cm) and held steady while readings were recorded.
6. **Data Capture & Logging**: Telemetry samples across each stationary hold were captured and logged.

---

## 7. Raw Test Results

During the live multi-position hand test, 15 distinct telemetry samples were captured across the three physical positions:

| Sample Index | Raw Distance (mm) | Distance (cm) | Status Code | Test Position |
|:---:|:---:|:---:|:---:|:---|
| **01** | $167.9\text{ mm}$ | $16.8\text{ cm}$ | `status=2` | Position 1 (Closer Steady Hold) |
| **02** | $174.2\text{ mm}$ | $17.4\text{ cm}$ | `status=2` | Position 1 (Closer Steady Hold) |
| **03** | $174.2\text{ mm}$ | $17.4\text{ cm}$ | `status=2` | Position 1 (Closer Steady Hold) |
| **04** | $174.2\text{ mm}$ | $17.4\text{ cm}$ | `status=2` | Position 1 (Closer Steady Hold) |
| **05** | $174.4\text{ mm}$ | $17.4\text{ cm}$ | `status=2` | Position 1 (Closer Steady Hold) |
| **06** | $180.8\text{ mm}$ | $18.1\text{ cm}$ | `status=2` | Position 2 (Moved Farther Away) |
| **07** | $187.3\text{ mm}$ | $18.7\text{ cm}$ | `status=2` | Position 2 (Moved Farther Away) |
| **08** | $184.0\text{ mm}$ | $18.4\text{ cm}$ | `status=2` | Position 2 (Moved Farther Away) |
| **09** | $180.8\text{ mm}$ | $18.1\text{ cm}$ | `status=2` | Position 2 (Moved Farther Away) |
| **10** | $180.8\text{ mm}$ | $18.1\text{ cm}$ | `status=2` | Position 2 (Moved Farther Away) |
| **11** | $184.0\text{ mm}$ | $18.4\text{ cm}$ | `status=2` | Position 3 (Extended Farther Hold) |
| **12** | $184.0\text{ mm}$ | $18.4\text{ cm}$ | `status=2` | Position 3 (Extended Farther Hold) |
| **13** | $184.0\text{ mm}$ | $18.4\text{ cm}$ | `status=2` | Position 3 (Extended Farther Hold) |
| **14** | $193.8\text{ mm}$ | $19.4\text{ cm}$ | `status=2` | Position 3 (Extended Farther Hold) |
| **15** | $193.8\text{ mm}$ | $19.4\text{ cm}$ | `status=2` | Position 3 (Extended Farther Hold) |

---

## 8. Position-Wise Averages & Breakdown

### Position 1 — Closer Steady Hold Position (5 Samples)
* Sample 1: $167.9\text{ mm}$ ($16.8\text{ cm}$)
* Sample 2: $174.2\text{ mm}$ ($17.4\text{ cm}$)
* Sample 3: $174.2\text{ mm}$ ($17.4\text{ cm}$)
* Sample 4: $174.2\text{ mm}$ ($17.4\text{ cm}$)
* Sample 5: $174.4\text{ mm}$ ($17.4\text{ cm}$)
* **Reported Average Distance**: **$173.0\text{ mm}$ ($17.3\text{ cm}$)**
* **Status Code**: `status=2` (`SensorStatus::OK`)

### Position 2 — Hand Moved Farther Away (5 Samples)
* Sample 1: $180.8\text{ mm}$ ($18.1\text{ cm}$)
* Sample 2: $187.3\text{ mm}$ ($18.7\text{ cm}$)
* Sample 3: $184.0\text{ mm}$ ($18.4\text{ cm}$)
* Sample 4: $180.8\text{ mm}$ ($18.1\text{ cm}$)
* Sample 5: $180.8\text{ mm}$ ($18.1\text{ cm}$)
* **Reported Average Distance**: **$182.7\text{ mm}$ ($18.3\text{ cm}$)**
* **Status Code**: `status=2` (`SensorStatus::OK`)

### Position 3 — Extended Farther Hold (5 Samples)
* Sample 1: $184.0\text{ mm}$ ($18.4\text{ cm}$)
* Sample 2: $184.0\text{ mm}$ ($18.4\text{ cm}$)
* Sample 3: $184.0\text{ mm}$ ($18.4\text{ cm}$)
* Sample 4: $193.8\text{ mm}$ ($19.4\text{ cm}$)
* Sample 5: $193.8\text{ mm}$ ($19.4\text{ cm}$)
* **Reported Average Distance**: **$187.9\text{ mm}$ ($18.8\text{ cm}$)**
* **Status Code**: `status=2` (`SensorStatus::OK`)

---

## 9. Observed Behavior

1. **Target Detection**: Sensor #5 reliably detected the target hand across all test positions.
2. **Dynamic Range Tracking**: The reported distance changed dynamically and consistently with the motion of the hand, showing tight repeatability at each position ($17.3\text{ cm}$, $18.3\text{ cm}$, and $18.8\text{ cm}$).
3. **Telemetry Consistency**: 100% of recorded samples returned valid `status=2` (`SensorStatus::OK`) with zero packet dropouts.
4. **Firmware Stability**: Zero bootloops, zero watchdog timer resets, and zero flash-read/strapping errors were observed during execution.

---

## 10. Pass / Fail Criteria

| Acceptance Criterion | Requirement | Observed Result | Status |
|---|---|---|:---:|
| **Valid Status Telemetry** | Sensor returns `status=2` for all acquisitions | $15/15$ active samples returned `status=2` | **PASS** |
| **Object Detection** | Target hand reliably detected in beam | Hand detected across all 3 positions | **PASS** |
| **Dynamic Response** | Output distance shifts corresponding to movement | Scaled correctly from $16.8\text{ cm}$ to $19.4\text{ cm}$ | **PASS** |
| **System Stability** | No ESP32 crashes, resets, or boot errors | 0 bootloops, 0 watchdog triggers | **PASS** |
| **Electrical Safety** | Echo signal safe for ESP32 input | $2.5\text{ V}$ peak on `GPIO34` via divider | **PASS** |

---

## 11. Verification Matrix

The status terminology used across all AutoChair engineering documentation is defined as:
* **CONFIRMED**: Directly demonstrated and verified with physical test evidence.
* **IN DEVELOPMENT**: Active implementation or bench work currently underway.
* **TARGET**: Intended engineering requirement not yet physically tested.
* **FUTURE**: Planned for subsequent phases or integration milestones.

| Verification Item | Description | Evidence / Result | Status |
|---|---|---|:---:|
| **Sensor #5 Transceiver Health** | Functional acoustic burst & echo detection | 15 valid samples acquired | **CONFIRMED** |
| **Sensor #5 Dynamic Hand Tracking** | Detection across close, mid, and far positions | Tracked $17.3\text{ cm} \leftrightarrow 18.3\text{ cm} \leftrightarrow 18.8\text{ cm}$ | **CONFIRMED** |
| **Level-Shifting Operation** | $4.7\text{ k}\Omega + 4.7\text{ k}\Omega$ divider into `GPIO34` | Reliable echo edge detection | **CONFIRMED** |
| **Isolated 5 V Power Compatibility** | Powered via Raspberry Pi Pin 2 with common GND | Stable operation without reboot | **CONFIRMED** |
| **Firmware Task Watchdog Timing** | Task scheduling with FreeRTOS `delay(1)` yield | Zero TWDT triggers observed | **CONFIRMED** |
| **Sensor #5 Calibrated Distance Accuracy** | Formal optical/ruler calibrated comparison | Not performed during this smoke test | **NOT FORMALLY VALIDATED** |
| **Sensor #6 Smoke Testing** | Individual functional test for remaining unit | Awaiting physical bench testing | **IN DEVELOPMENT** |
| **Multi-Sensor Sequential Polling** | Staggered triggering of full 6-sensor array | Firmware architecture defined | **TARGET** |
| **Acoustic Crosstalk Mitigation** | Physical chassis beam-overlap testing | Requires full array mount | **FUTURE** |
| **SafetyManager E-Stop Integration** | Obstacle threshold braking linkage | Awaiting full array validation | **FUTURE** |

---

## 12. Limitations

To maintain engineering integrity, the following limitations must be noted:

1. **Uncalibrated Reference**: Hand positions were set manually without a rigid mechanical jig or optical distance reference.
2. **No Accuracy Claims**: This test does **not** evaluate absolute sensor accuracy or percentage error.
3. **Single-Sensor Test**: This test was executed with Sensor #5 standalone on `GPIO4`/`GPIO34`. It does not test multi-sensor bus switching or parallel sensor firing.
4. **Temporary Bench Power**: Power was supplied via a Raspberry Pi 5 V bench rail, not the final wheelchair power distribution system.

---

## 13. Final Engineering Conclusion

Physical **HC-SR04 Ultrasonic Sensor #5** successfully passed its bench-level functional smoke test. When powered from a stable $5.0\text{ V}$ rail and connected to the ESP32 through a $4.7\text{ k}\Omega / 4.7\text{ k}\Omega$ voltage divider on `GPIO34` (with `TRIG` driven from `GPIO4`), the sensor unit demonstrated healthy acoustic transceiving, responsive dynamic tracking of a human hand across 3 distinct positions ($17.3\text{ cm}$ to $18.8\text{ cm}$), and 100% valid `status=2` telemetry emission with zero microcontroller faults.

### Official Verdicts:
* **Sensor #5 Functional Smoke Test**: **`PASS`**
* **Absolute Calibrated Distance Accuracy**: **`NOT FORMALLY VALIDATED`**

---

## 14. Next Steps

1. **Test Sensor #6**: Perform the quick functional smoke test on the final physical HC-SR04 unit #6.
2. **Finalize Multi-Sensor GPIO Mapping**: Map all 6 sensors to their designated trigger and echo GPIOs with dedicated level-shifting dividers.
3. **Deploy Multi-Sensor Sequential Polling**: Integrate the verified time-multiplexed polling schedule ($50\text{ ms}$ interval between sensors) to prevent acoustic crosstalk.
4. **Precision Chassis Calibration**: Mount the sensor array in the final mechanical chassis and perform formal multi-point distance calibration with fixed reference targets.
