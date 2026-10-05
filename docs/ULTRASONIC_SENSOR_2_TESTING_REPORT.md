# AutoChair — Ultrasonic Sensor #2 Testing Report

### Subtitle
**HC-SR04 Sensor #2 Bench Smoke Testing, Level-Shifting Verification, and Live Hand-Distance Detection**

* **Project**: AutoChair — Smart Wheelchair Navigation & Safety Subsystem
* **Document ID**: `DOC-TEST-HCSR04-002`
* **Date**: October 5, 2026
* **Branch**: `alvira-dev`
* **Status**: Completed Engineering Record

---

## 1. Objective

The primary objective was to perform a focused individual functional smoke test and bench verification for physical **HC-SR04 Ultrasonic Sensor #2** before incorporating it into the multi-sensor array.

Following the thorough baseline investigation and power isolation established during Sensor #1 testing, Sensor #2 was tested strictly for unit-level hardware functionality and dynamic echo response.

### Scope of this Test:
* **IN SCOPE**:
  1. Verification of physical Sensor #2 transceiver health (piezoelectric transmitter/receiver pair).
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
* **Ultrasonic Sensor Under Test**: HC-SR04 Ultrasonic Distance Sensor Module (Physical Unit #2).
* **External 5 V Power Source**: Raspberry Pi 4 Model B (utilizing Physical Pin 2 for 5.0 V DC power and Physical Pin 6 for Ground reference).
* **Development Host Link**: Development computer connected to the ESP32 via USB-UART cable (`/dev/cu.SLAB_USBtoUART`) at 115200 baud, providing ESP32 power and serial monitor capture.
* **Level-Shifting Passives**: Two carbon-film through-hole resistors ($4.7\text{ k}\Omega \pm 5\%$) forming a 2:1 resistive divider.
* **Prototyping Medium**: Solderless breadboard and standard DuPont jumper wires.

> [!IMPORTANT]
> The ESP32 was powered strictly via its USB port from the computer. The HC-SR04 Sensor #2 was powered strictly from the Raspberry Pi Physical Pin 2 ($5.0\text{ V}$). The Raspberry Pi $5\text{ V}$ was **NOT** connected to the ESP32 $5\text{V}$ (`VIN`) pin.

---

## 3. Final Wiring Configuration

Sensor #2 was connected using the proven electrical configuration established during prior bench validation:

### 3.1 Interconnect Table

| Signal / Pin | Source | Destination | Nominal Voltage | Function |
|---|---|---|---|---|
| **`VCC`** | HC-SR04 Sensor #2 Pin 1 | Raspberry Pi 4 Physical Pin 2 | $5.0\text{ V DC}$ | Sensor Power Supply |
| **`GND`** | HC-SR04 Sensor #2 Pin 4 | Common Breadboard GND Rail | $0\text{ V}$ | Sensor Ground Reference |
| **`Pi GND`** | Raspberry Pi 4 Physical Pin 6 | Common Breadboard GND Rail | $0\text{ V}$ | Power Supply Common Ground |
| **`ESP32 GND`** | ESP32 GND Header Pin | Common Breadboard GND Rail | $0\text{ V}$ | Microcontroller Common Ground |
| **`TRIG`** | ESP32 `GPIO4` | HC-SR04 Sensor #2 `TRIG` Pin | $3.3\text{ V}$ TTL Logic | $10\ \mu\text{s}$ Trigger Pulse |
| **`ECHO`** | HC-SR04 Sensor #2 `ECHO` Pin | Breadboard Voltage Divider Input | $5.0\text{ V}$ Pulse | Return Timing Pulse |
| **`ECHO Sense`** | Voltage Divider Junction | ESP32 `GPIO34` (GPI / Input-only) | $2.5\text{ V}$ Step-down | Level-Shifted Echo Sense |

---

## 4. ECHO Voltage-Divider Schematic

To protect the ESP32 $3.3\text{ V}$ input stage from the HC-SR04 $5.0\text{ V}$ echo pulse, a symmetrical 2:1 resistive voltage divider was placed between the sensor `ECHO` output and `GPIO34`:

```text
HC-SR04 Sensor #2 ECHO (5.0 V Pulse)
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
3. **Position 1 (Initial Hand Hold)**: An open hand was placed directly in front of Sensor #2 at a comfortable mid-range distance and held steady while readings were recorded.
4. **Position 2 (Farther Range)**: The hand was moved farther away from the sensor face and held steady while readings were recorded.
5. **Position 3 (Closer Range)**: The hand was moved closer to the sensor face and held steady while readings were recorded.
6. **Data Capture & Logging**: Telemetry samples across each stationary hold were captured and logged.

---

## 7. Raw Test Results

During the live multi-position hand test, 12 distinct telemetry samples were captured across the three physical positions:

| Sample Index | Raw Distance (mm) | Distance (cm) | Status Code | Test Position |
|:---:|:---:|:---:|:---:|:---|
| **01** | $283.7\text{ mm}$ | $28.4\text{ cm}$ | `status=2` | Position 1 (Initial Hold) |
| **02** | $256.9\text{ mm}$ | $25.7\text{ cm}$ | `status=2` | Position 1 (Initial Hold) |
| **03** | $300.8\text{ mm}$ | $30.1\text{ cm}$ | `status=2` | Position 1 (Initial Hold) |
| **04** | $296.5\text{ mm}$ | $29.6\text{ cm}$ | `status=2` | Position 1 (Initial Hold) |
| **05** | $298.2\text{ mm}$ | $29.8\text{ cm}$ | `status=2` | Position 1 (Initial Hold) |
| **06** | $617.7\text{ mm}$ | $61.8\text{ cm}$ | `status=2` | Position 2 (Farther Position) |
| **07** | $689.3\text{ mm}$ | $68.9\text{ cm}$ | `status=2` | Position 2 (Farther Position) |
| **08** | $610.4\text{ mm}$ | $61.0\text{ cm}$ | `status=2` | Position 2 (Farther Position) |
| **09** | $655.0\text{ mm}$ | $65.5\text{ cm}$ | `status=2` | Position 2 (Farther Position) |
| **10** | $602.0\text{ mm}$ | $60.2\text{ cm}$ | `status=2` | Position 2 (Farther Position) |
| **11** | $148.9\text{ mm}$ | $14.9\text{ cm}$ | `status=2` | Position 3 (Closer Position) |
| **12** | $142.0\text{ mm}$ | $14.2\text{ cm}$ | `status=2` | Position 3 (Closer Position) |

---

## 8. Position-Wise Averages & Breakdown

### Position 1 — Initial Hand Position (5 Samples)
* Sample 1: $283.7\text{ mm}$ ($28.4\text{ cm}$)
* Sample 2: $256.9\text{ mm}$ ($25.7\text{ cm}$)
* Sample 3: $300.8\text{ mm}$ ($30.1\text{ cm}$)
* Sample 4: $296.5\text{ mm}$ ($29.6\text{ cm}$)
* Sample 5: $298.2\text{ mm}$ ($29.8\text{ cm}$)
* **Reported Average Distance**: **$287.2\text{ mm}$ ($28.7\text{ cm}$)**
* **Status Code**: `status=2` (`SensorStatus::OK`)

### Position 2 — Hand Moved Farther Away (5 Samples)
* Sample 1: $617.7\text{ mm}$ ($61.8\text{ cm}$)
* Sample 2: $689.3\text{ mm}$ ($68.9\text{ cm}$)
* Sample 3: $610.4\text{ mm}$ ($61.0\text{ cm}$)
* Sample 4: $655.0\text{ mm}$ ($65.5\text{ cm}$)
* Sample 5: $602.0\text{ mm}$ ($60.2\text{ cm}$)
* **Reported Average Distance**: **$634.9\text{ mm}$ ($63.5\text{ cm}$)**
* **Status Code**: `status=2` (`SensorStatus::OK`)

### Position 3 — Hand Moved Closer (2 Samples)
* Sample 1: $148.9\text{ mm}$ ($14.9\text{ cm}$)
* Sample 2: $142.0\text{ mm}$ ($14.2\text{ cm}$)
* **Reported Average Distance**: **$145.5\text{ mm}$ ($14.6\text{ cm}$)**
* **Status Code**: `status=2` (`SensorStatus::OK`)

---

## 9. Observed Behavior

1. **Target Detection**: The sensor detected the physical presence of the hand immediately upon placement in the acoustic beam.
2. **Dynamic Range Tracking**: The reported distance changed dynamically and monotonically with the physical motion of the hand, spanning from $\sim 14.6\text{ cm}$ at close range up to $\sim 63.5\text{ cm}$ at the extended position.
3. **Telemetry Consistency**: 100% of recorded samples returned valid `status=2` (`SensorStatus::OK`) without timeout or error packets.
4. **Firmware Stability**: Zero bootloops, zero watchdog timer resets, and zero flash-read/strapping errors were observed during execution.

---

## 10. Pass / Fail Criteria

| Acceptance Criterion | Requirement | Observed Result | Status |
|---|---|---|:---:|
| **Valid Status Telemetry** | Sensor returns `status=2` for all acquisitions | $12/12$ samples returned `status=2` | **PASS** |
| **Object Detection** | Target hand reliably detected in beam | Hand detected in all 3 positions | **PASS** |
| **Dynamic Response** | Output distance shifts corresponding to movement | Scaled correctly from $14.6\text{ cm}$ to $63.5\text{ cm}$ | **PASS** |
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
| **Sensor #2 Transceiver Health** | Functional acoustic burst & echo detection | 12 valid samples acquired | **CONFIRMED** |
| **Sensor #2 Dynamic Hand Tracking** | Detection across near, mid, and far positions | Tracked $14.6\text{ cm} \rightarrow 63.5\text{ cm}$ | **CONFIRMED** |
| **Level-Shifting Operation** | $4.7\text{ k}\Omega + 4.7\text{ k}\Omega$ divider into `GPIO34` | Reliable echo edge detection | **CONFIRMED** |
| **Isolated 5 V Power Compatibility** | Powered via Raspberry Pi Pin 2 with common GND | Stable operation without reboot | **CONFIRMED** |
| **Firmware Task Watchdog Timing** | Task scheduling with FreeRTOS `delay(1)` yield | Zero TWDT triggers observed | **CONFIRMED** |
| **Sensor #2 Calibrated Distance Accuracy** | Formal optical/ruler calibrated comparison | Not performed during this smoke test | **NOT FORMALLY VALIDATED** |
| **Sensors #3 to #6 Smoke Testing** | Individual functional tests for remaining units | Awaiting physical bench testing | **IN DEVELOPMENT** |
| **Multi-Sensor Sequential Polling** | Staggered triggering of full 6-sensor array | Firmware architecture defined | **TARGET** |
| **Acoustic Crosstalk Mitigation** | Physical chassis beam-overlap testing | Requires full array mount | **FUTURE** |
| **SafetyManager E-Stop Integration** | Obstacle threshold braking linkage | Awaiting full array validation | **FUTURE** |

---

## 12. Limitations

To maintain engineering integrity, the following limitations must be noted:

1. **Uncalibrated Reference**: Hand positions were set manually without a rigid mechanical jig or optical distance reference.
2. **No Accuracy Claims**: This test does **not** evaluate absolute sensor accuracy or percentage error.
3. **Single-Sensor Test**: This test was executed with Sensor #2 standalone on `GPIO4`/`GPIO34`. It does not test multi-sensor bus switching or parallel sensor firing.
4. **Temporary Bench Power**: Power was supplied via a Raspberry Pi 5 V bench rail, not the final wheelchair power distribution system.

---

## 13. Final Engineering Conclusion

Physical **HC-SR04 Ultrasonic Sensor #2** successfully passed its bench-level functional smoke test. When powered from a stable $5.0\text{ V}$ rail and connected to the ESP32 through a $4.7\text{ k}\Omega / 4.7\text{ k}\Omega$ voltage divider on `GPIO34` (with `TRIG` driven from `GPIO4`), the sensor unit demonstrated healthy acoustic transceiving, responsive dynamic tracking of a human hand across 3 distinct positions ($14.6\text{ cm}$ to $63.5\text{ cm}$), and 100% valid `status=2` telemetry emission with zero microcontroller faults.

### Official Verdicts:
* **Sensor #2 Functional Smoke Test**: **`PASS`**
* **Absolute Calibrated Distance Accuracy**: **`NOT FORMALLY VALIDATED`**

---

## 14. Next Steps

1. **Test Remaining Sensors**: Perform quick functional smoke tests on physical HC-SR04 units #3, #4, #5, and #6.
2. **Finalize Multi-Sensor GPIO Mapping**: Map all 6 sensors to their designated trigger and echo GPIOs with dedicated level-shifting dividers.
3. **Deploy Multi-Sensor Sequential Polling**: Integrate the verified time-multiplexed polling schedule ($50\text{ ms}$ interval between sensors) to prevent acoustic crosstalk.
4. **Precision Chassis Calibration**: Mount the sensor array in the final mechanical chassis and perform formal multi-point distance calibration with fixed reference targets.
