# Wheel Encoder Bench Testing & Validation Suite

This directory contains the standalone hardware validation suite, firmware, automated test runner, and official engineering validation reports for bench-testing the **Incremental Optical Rotary Encoder (600 PPR / 2400 CPR X4)** using an ESP32.

---

## Directory Structure

```text
Bench Test/Wheel encoder_validation/
├── README.md                                          # Directory overview & execution guide
├── ENCODER_PHASE_VALIDATION_REPORT.md                 # Official engineering validation report (PASS)
├── ENCODER_MECHANICAL_MOUNTING_CALIBRATION_REVIEW.md  # Mechanical mounting, calibration & odometry review
├── serial_runner.py                                   # Automated Python serial test runner & telemetry logger
└── firmware/                                          # Standalone PlatformIO firmware project
    ├── platformio.ini                                 # PlatformIO configuration (ESP32-WROOM-32, 115200 baud)
    └── src/
        └── main.cpp                                   # Standalone Quadrature & RPM validation engine
```

---

## Hardware Specifications & Pin Mapping

### 1. Encoder Specifications
- **Type:** Incremental Optical Rotary Encoder (A, B, Z channels)
- **Resolution:** 600 waveform cycles / revolution (PPR)
- **Decoding Mode:** X4 Quadrature Decoding $\rightarrow$ **2,400 counts / revolution (CPR)**
- **Measured CPR:** $2,432.7\text{ counts/rev}$ (over 3 consecutive physical revolutions)
- **Supply Voltage:** 5.0 V DC (measured $5.0066\text{ V}$)
- **Signal Logic Level:** 3.3 V DC (measured $3.307\text{ V}$)

### 2. Physical Wiring
- **Red (VCC):** $\rightarrow$ **ESP32 5V (USB Rail)**
- **Black (GND):** $\rightarrow$ **ESP32 GND**
- **Green (Channel A):** $\rightarrow$ **ESP32 GPIO25** with **$4.7\text{ k}\Omega$ pull-up to 3.3V**
- **White (Channel B):** $\rightarrow$ **ESP32 GPIO26** with **$4.7\text{ k}\Omega$ pull-up to 3.3V**
- **Yellow (Channel Z / Index):** $\rightarrow$ **ESP32 GPIO27** with **$4.7\text{ k}\Omega$ pull-up to 3.3V**
- **Shield / Braid:** Left unconnected (isolated bench test)

---

## How to Run the Bench Tests

### 1. Flash the Firmware
From the `firmware/` directory:
```bash
pio run --target upload --upload-port /dev/cu.SLAB_USBtoUART
```
*(Alternative port if using another USB-UART bridge: `/dev/cu.usbserial-0001`)*

### 2. Live Serial Telemetry Monitoring
Open serial monitor at **115200 baud**:
```bash
pio device monitor -b 115200
```
Or run the Python telemetry capture script:
```bash
# Capture live telemetry for 30 seconds
python3 serial_runner.py 30

# Reset encoder counts and stream telemetry
python3 serial_runner.py 30 CMD:RESET
```

### 3. Interactive Serial Commands
Send the following serial commands over 115200 baud UART:
- `CMD:RESET` $\rightarrow$ Resets pulse count, Z-index count, and RPM to zero.
- `CMD:STATUS` $\rightarrow$ Dumps system uptime, free heap, and encoder health statistics.
- `CMD:STREAM` $\rightarrow$ Toggles continuous high-frequency diagnostic stream ($100\text{ ms}$ cadence).

---

## Test Results Summary

| Validation Step | Tested Behavior | Result | Status |
| :--- | :--- | :---: | :---: |
| **1. Electrical Signal Verification** | 600 cycles/rev, clean quadrature, 1 Z/rev on logic analyzer | Verified | **PASS** |
| **2. Stationary Stability** | Count stable ($\Delta = 0$), `dir=0`, `rpm=0.00`, zero glitch transitions (`inv=0`) | Verified | **PASS** |
| **3. Clockwise (CW) Rotation** | Monotonic count increase, `dir=1` (`FORWARD`), positive RPM | Verified | **PASS** |
| **4. Counter-Clockwise (CCW)** | Monotonic count decrease ($\Delta = -22,383$), `dir=-1` (`REVERSE`), negative RPM | Verified | **PASS** |
| **5. Z-Index Multi-Rev Tracking** | $\Delta = 7,298\text{ counts}$ across 3 Z pulses ($2,432.7\text{ counts/rev}$) | Verified | **PASS** |
| **6. Real-Time Signed RPM** | Dynamic elapsed time calculation ($+5\text{ to }+125\text{ RPM}$ CW, $-5\text{ to }-195\text{ RPM}$ CCW) | Verified | **PASS** |
| **7. Glitch Rejection** | Invalid state transitions strictly zero (`inv=0`) across all trials | Verified | **PASS** |
| **Overall Bench Validation** | Full electrical-to-firmware pipeline operational | Verified | **OVERALL PASS** |

---

## Related Engineering Documentation
- [`ENCODER_PHASE_VALIDATION_REPORT.md`](file:///Users/admin/Desktop/AutoChair/Bench%20Test/Wheel%20encoder_validation/ENCODER_PHASE_VALIDATION_REPORT.md) — Comprehensive technical record of all bench test measurements and raw logs.
- [`ENCODER_MECHANICAL_MOUNTING_CALIBRATION_REVIEW.md`](file:///Users/admin/Desktop/AutoChair/Bench%20Test/Wheel%20encoder_validation/ENCODER_MECHANICAL_MOUNTING_CALIBRATION_REVIEW.md) — Requirements and analysis for physical mounting, wheel roll calibration, and differential odometry.
