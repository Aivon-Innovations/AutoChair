# MPU6050 IMU Bench Testing & Validation Suite

This directory contains the standalone hardware validation suite, firmware, test operator scripts, and official engineering validation reports for bench-testing the **MPU6050 6-DOF Inertial Measurement Unit (Accelerometer + Gyroscope)** using an ESP32.

---

## Directory Structure

```text
Bench Test/IMU testing/
├── README.md                                                   # Directory overview & execution guide
├── ESP32_MPU6050_Physical_Bench_Validation_Final_Report.md     # Official engineering validation report (12/12 PASS)
├── interactive_test.py                                         # Interactive CLI test operator script
├── monitor_validation.py                                       # Real-time telemetry monitoring script
├── run_operator.py                                             # Automated test runner & report validator
├── platformio.ini                                              # PlatformIO configuration (ESP32-WROOM-32, 115200 baud)
└── src/
    └── main.cpp                                                # Standalone MPU6050 I2C validation engine
```

---

## Hardware Specifications & Pin Mapping

### 1. Sensor Specifications
- **Sensor:** MPU6050 6-DOF IMU (Silicon verified via `WHO_AM_I = 0x68`)
- **MCU:** ESP32-D0WD-V3 (rev 3.1), 240 MHz
- **Protocol:** I²C (Fast Mode 400 kHz / Standard 100 kHz)
- **I²C Address:** `0x68` (AD0 pin grounded / low)
- **Supply Voltage:** 3.3 V DC (measured stable from ESP32 3V3 rail)

### 2. Physical Wiring
- **VCC:** $\rightarrow$ **ESP32 3V3**
- **GND:** $\rightarrow$ **ESP32 GND**
- **SDA:** $\rightarrow$ **ESP32 GPIO21**
- **SCL:** $\rightarrow$ **ESP32 GPIO22**
- **AD0:** $\rightarrow$ **GND** (selects address `0x68`)
- **INT:** Left unconnected for bench polling test

---

## How to Run the Bench Tests

### 1. Flash the Firmware
From the `Bench Test/IMU testing/` directory:
```bash
pio run --target upload
```

### 2. Run Interactive Test Operator
```bash
python3 interactive_test.py
```

### 3. Monitor Real-time Telemetry
```bash
python3 monitor_validation.py
```

---

## Test Results Summary

| # | Test Item | Result | Key Measurement / Behavior | Status |
| :--- | :--- | :---: | :--- | :---: |
| **T1** | I²C Bus Detection | ✅ PASS | Device found at `0x68`, 10/10 probes stable | **PASS** |
| **T2** | Sensor Identification | ✅ PASS | `WHO_AM_I = 0x68` $\rightarrow$ Confirmed authentic MPU6050 | **PASS** |
| **T3** | Stationary Accelerometer (5s) | ✅ PASS | $\|A\|_{\text{avg}} \approx 0.984\text{ g}$ (expected $\sim 1.000\text{ g}$) | **PASS** |
| **T4** | Stationary Gyroscope (5s) | ✅ PASS | Bias: $X=+1.92^\circ/\text{s}, Y=+1.16^\circ/\text{s}, Z=+0.33^\circ/\text{s}$ | **PASS** |
| **T5.1** | X-Axis Tilt Response | ✅ PASS | Clear $\Delta A_x \ge 0.15\text{ g}$ response detected | **PASS** |
| **T5.2** | Y-Axis Tilt Response | ✅ PASS | Clear $\Delta A_y \ge 0.15\text{ g}$ response detected | **PASS** |
| **T5.3** | X-Axis Rotation (Gyro) | ✅ PASS | Peak $\|G_x\| \ge 20^\circ/\text{s}$ confirmed | **PASS** |
| **T5.4** | Y-Axis Rotation (Gyro) | ✅ PASS | Peak $\|G_y\| \ge 20^\circ/\text{s}$ confirmed | **PASS** |
| **T5.5** | Z-Axis Rotation (Gyro) | ✅ PASS | Peak $\|G_z\| \ge 20^\circ/\text{s}$ confirmed | **PASS** |
| **T6** | 60s Stationary Stability | ✅ PASS | 300 samples, 0 errors, $\text{StdDev} < 0.05\text{ g}$ | **PASS** |
| **T7A** | Communication Fault Detection | ✅ PASS | NACK detected at $t = 0.00\text{ s}$ (Error Code 2) | **PASS** |
| **T7B** | Communication Bus Recovery | ✅ PASS | Full recovery in $0.27\text{ s}$, $\|A\| = 0.955\text{ g}$ post-recovery | **PASS** |

**Total Score:** **12 / 12 PASS — 0 FAIL**

---

## Key Hardware Findings & Fixes
- **ESP32 I²C Repeated-Start Workaround:** Using `Wire.endTransmission(true)` followed by `Wire.requestFrom(..., true)` resolved the ESP32 hardware repeated-start dropout bug.
- **Bus Fault Recovery:** Recovery from disconnected I²C bus lines requires an explicit `Wire.end()` / `Wire.begin()` hard re-initialization cycle.

---

## Related Engineering Documentation
- [`ESP32_MPU6050_Physical_Bench_Validation_Final_Report.md`](file:///Users/admin/Desktop/AutoChair/Bench%20Test/IMU%20testing/ESP32_MPU6050_Physical_Bench_Validation_Final_Report.md) — Comprehensive technical record of all test logs, noise characterization, dynamic response curves, and bus recovery data.
