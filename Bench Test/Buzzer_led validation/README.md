# Active Buzzer and LED Bench Testing Mini-Project

This directory contains the standalone hardware validation suite and test artifacts for bench-testing the **3.5–5.5 V Standard Active Buzzer Module** and an **LED Indicator** using an ESP32.

---

## Directory Structure

```text
Bench Test/Buzzer_led validation/
├── README.md                              # Directory overview & execution guide
├── BUZZER_LED_BENCH_VALIDATION_REPORT.md  # Official engineering validation report (PASS)
├── serial_runner.py                       # Automated Python serial test runner & telemetry collector
└── firmware/                              # Standalone PlatformIO firmware project
    ├── platformio.ini                     # PlatformIO configuration (ESP32-WROOM-32, 115200 baud)
    └── src/
        └── main.cpp                       # Dual-output validation engine (Buzzer, LED, Combined)
```

---

## Hardware Wiring Summary

### 1. Active Buzzer Module (3.5–5.5 V)
- **`−` (GND)** $\rightarrow$ **ESP32 GND**
- **`Middle` (VCC)** $\rightarrow$ **ESP32 5V (USB Rail)**
- **`S` (Signal)** $\rightarrow$ **ESP32 GPIO33**
- **Drive Logic:** Resonant 2.7 kHz square wave pulse drive

### 2. LED Warning Circuit
- **ESP32 GPIO32** $\rightarrow$ **220 Ω Resistor** $\rightarrow$ **LED Anode (+)**
- **LED Cathode (−)** $\rightarrow$ **ESP32 GND**
- **Drive Logic:** Active-HIGH digital output (1.0 s ON / 1.0 s OFF)

---

## How to Run the Bench Tests

### 1. Flash the Firmware
From the `firmware/` directory:
```bash
pio run --target upload --upload-port /dev/cu.usbserial-0001
```

### 2. Run Automated Test Commands via Python
From this directory:
```bash
# Run 20-cycle Buzzer Test
python3 serial_runner.py CMD:BUZZER_20 46

# Run 20-cycle LED Test
python3 serial_runner.py CMD:LED_20 46

# Run 20-cycle Synchronized Combined Buzzer + LED Test
python3 serial_runner.py CMD:COMBINED_20 46

# Query Live System Telemetry (Uptime, Free Heap, Reset Reasons)
python3 serial_runner.py CMD:STATUS 5
```

---

## Test Results
- **Buzzer 20-Cycle Test:** **PASS (20/20)** — Audible (Nice, loud & clear)
- **LED 20-Cycle Test:** **PASS (20/20)** — Visible flashing (1s ON / 1s OFF)
- **Combined 20-Cycle Test:** **PASS (20/20)** — Simultaneous audible & visual emission
- **ESP32 Stability:** **PASS** — 0 watchdog resets, 0 brownouts, stable heap
- **Overall Result:** **OVERALL PASS**
