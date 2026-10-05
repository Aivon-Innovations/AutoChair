# Ultrasonic Sensor Bench Testing & Validation Suite

This directory contains the standalone hardware validation suite, firmware, automated test runner, and official engineering validation reports for bench-testing the **HC-SR04 Ultrasonic Distance Sensor Subsystem** using an ESP32.

---

## Directory Structure

```text
Bench Test/Ultrasonic sensor validation/
├── README.md                                          # Directory overview & execution guide
├── serial_runner.py                                   # Automated Python serial test runner & telemetry logger
├── ULTRASONIC_PHASE_VALIDATION_REPORT.md              # Overall Ultrasonic Subsystem Phase Validation Report
├── ULTRASONIC_SENSOR_TESTING_REPORT.md                # Sensor #1 Bench Testing & Troubleshooting Report
├── ULTRASONIC_SENSOR_2_TESTING_REPORT.md              # Sensor #2 Functional Smoke Test Report (PASS)
├── ULTRASONIC_SENSOR_3_TESTING_REPORT.md              # Sensor #3 Functional Smoke Test Report (PASS)
├── ULTRASONIC_SENSOR_4_TESTING_REPORT.md              # Sensor #4 Functional Smoke Test Report (PASS)
├── ULTRASONIC_SENSOR_5_TESTING_REPORT.md              # Sensor #5 Functional Smoke Test Report (PASS)
├── ULTRASONIC_SENSOR_6_TESTING_REPORT.md              # Sensor #6 Functional Smoke Test Report (PASS)
└── firmware/                                          # Standalone PlatformIO firmware project
    ├── platformio.ini                                 # PlatformIO configuration (ESP32-WROOM-32, 115200 baud)
    └── src/
        └── main.cpp                                   # Standalone single-sensor & round-robin array test engine
```

---

## Hardware Specifications & Pin Mapping

### 1. HC-SR04 Sensor Specifications
- **Operating Voltage:** $5.0\text{ V DC}$ (Mandatory; isolated supply recommended)
- **Acoustic Burst Frequency:** $40\text{ kHz}$
- **Trigger Input:** $10\ \mu\text{s}$ TTL Pulse (`HIGH`)
- **Echo Output:** $5.0\text{ V}$ TTL pulse duration $\propto$ target distance
- **Measurement Range:** $20\text{ mm}$ ($2\text{ cm}$) to $4000\text{ mm}$ ($400\text{ cm}$)
- **Distance Formula:** $\text{Distance (mm)} = \frac{\text{Pulse Duration (}\mu\text{s)} \times 0.343\text{ mm/}\mu\text{s}}{2}$

### 2. ECHO Voltage-Divider Protection Circuit
Because the HC-SR04 `ECHO` output delivers a $5.0\text{ V}$ pulse, direct connection to an ESP32 pin would exceed the $3.6\text{ V}$ absolute maximum rating. A symmetrical 2:1 resistive divider is placed on each echo line:

```text
HC-SR04 ECHO (5.0 V Pulse)
        │
        ▼
 [ 4.7 kΩ Resistor (R1) ]
        │
        ├────────────────────────► ESP32 GPIO (Input-only Pin)
        │                          (Peak level: 2.50 V)
 [ 4.7 kΩ Resistor (R2) ]
        │
        ▼
Common GND Rail (0 V)
```

### 3. Physical Channel Mapping

| Sensor Index | Channel Role | TRIG Pin (Output) | ECHO Pin (Input via Divider) | Bench Status |
|:---:|:---:|:---:|:---:|:---:|
| **Sensor 0** (Phys #1) | Front-Center Left | **`GPIO4`** | **`GPIO34`** | **CONFIRMED** |
| **Sensor 1** (Phys #2) | Front-Center Right | **`GPIO5`** | **`GPIO35`** | **CONFIRMED (Single) / TARGET (Array)** |
| **Sensor 2** (Phys #3) | Front Left Corner | **`GPIO18`** | **`GPIO36` (VP)** | **CONFIRMED (Single) / TARGET (Array)** |
| **Sensor 3** (Phys #4) | Front Right Corner | **`GPIO19`** | **`GPIO39` (VN)** | **CONFIRMED (Single) / TARGET (Array)** |
| **Sensor 4** (Phys #5) | Side Left / Rear | **`GPIO23`** | **`GPIO32`** | **CONFIRMED (Single) / TARGET (Array)** |
| **Sensor 5** (Phys #6) | Side Right / Rear | **`GPIO13`** | **`GPIO33`** | **CONFIRMED (Single) / TARGET (Array)** |

---

## Bench Test Results Summary

All six physical HC-SR04 units have completed bench smoke and dynamic hand-tracking testing:

| Sensor Module | Reported Distance Range | Telemetry Status | Bench Verdict | Full Report |
|:---:|:---:|:---:|:---:|:---|
| **Sensor #1** | $9.0\text{ cm} \rightarrow 12.9\text{ cm}$ | $100\%$ `status=2` | **PASS** | [ULTRASONIC_SENSOR_TESTING_REPORT.md](file:///Users/admin/Desktop/AutoChair/Bench%20Test/Ultrasonic%20sensor%20validation/ULTRASONIC_SENSOR_TESTING_REPORT.md) |
| **Sensor #2** | $14.6\text{ cm} \rightarrow 63.5\text{ cm}$ | $100\%$ `status=2` | **PASS** | [ULTRASONIC_SENSOR_2_TESTING_REPORT.md](file:///Users/admin/Desktop/AutoChair/Bench%20Test/Ultrasonic%20sensor%20validation/ULTRASONIC_SENSOR_2_TESTING_REPORT.md) |
| **Sensor #3** | $53.4\text{ cm} \rightarrow 74.1\text{ cm}$ | $100\%$ `status=2` | **PASS** | [ULTRASONIC_SENSOR_3_TESTING_REPORT.md](file:///Users/admin/Desktop/AutoChair/Bench%20Test/Ultrasonic%20sensor%20validation/ULTRASONIC_SENSOR_3_TESTING_REPORT.md) |
| **Sensor #4** | $16.7\text{ cm} \rightarrow 19.9\text{ cm}$ | $100\%$ `status=2` | **PASS** | [ULTRASONIC_SENSOR_4_TESTING_REPORT.md](file:///Users/admin/Desktop/AutoChair/Bench%20Test/Ultrasonic%20sensor%20validation/ULTRASONIC_SENSOR_4_TESTING_REPORT.md) |
| **Sensor #5** | $17.3\text{ cm} \rightarrow 18.8\text{ cm}$ | $100\%$ `status=2` | **PASS** | [ULTRASONIC_SENSOR_5_TESTING_REPORT.md](file:///Users/admin/Desktop/AutoChair/Bench%20Test/Ultrasonic%20sensor%20validation/ULTRASONIC_SENSOR_5_TESTING_REPORT.md) |
| **Sensor #6** | $19.1\text{ cm} \rightarrow 20.6\text{ cm}$ | $100\%$ `status=2` | **PASS** | [ULTRASONIC_SENSOR_6_TESTING_REPORT.md](file:///Users/admin/Desktop/AutoChair/Bench%20Test/Ultrasonic%20sensor%20validation/ULTRASONIC_SENSOR_6_TESTING_REPORT.md) |

---

## How to Run the Bench Tests

### 1. Flash Standalone Firmware
From `Bench Test/Ultrasonic sensor validation/firmware/`:
```bash
pio run --target upload --upload-port /dev/cu.SLAB_USBtoUART
```

### 2. Stream Live Telemetry
Run the automated serial capture tool:
```bash
# Stream single-sensor telemetry for 15 seconds
python3 serial_runner.py 15

# Stream 6-sensor round-robin array telemetry for 30 seconds
python3 serial_runner.py ARRAY 30
```
