# AutoChair — Bench Test: Communication Test Pi & ESP32

**Location:** `Bench Test/communication test pi & esp32/`  
**Module:** Raspberry Pi 4 ↔ ESP32 USB Serial Communication (Task 9)  
**Status:** COMPLETE BENCH VALIDATION & PRESERVATION  
**Date:** October 2026  

---

## 1. Overview & Objective

This bench test module validates bidirectional serial communication, command framing, telemetry streaming, heartbeat watchdog monitoring, fail-safe state latching, and recovery between the Raspberry Pi 4 Model B and the ESP32 microcontroller over USB serial (`115200` baud, 8-N-1).

### Strict Bench Safety Constraints
* **Isolated Communication Test:** All wheelchair movement actions were purely virtual (`_simulatedMotion`).
* **Zero High-Voltage Connection:** The ESP32 was **never** connected to the wheelchair's 24V battery, motor controller, motors, or brakes. Power was supplied solely via USB VBUS.

---

## 2. Directory Contents

```text
communication test pi & esp32/
├── README.md                            # This overview and navigation guide
│
├── pi_esp32_serial_test/                # Active standalone serial test workspace
│   ├── README.md                        # Project guide & commands
│   ├── platformio.ini                   # PlatformIO build configuration (esp32dev, 115200 baud)
│   ├── .venv/                           # Python virtual environment
│   ├── esp32_firmware/                  # ESP32 C++ firmware source, headers & build binaries
│   ├── raspberry_pi/                    # Python serial client, protocol framing & CLI monitor
│   ├── protocol/                        # Formal JSON protocol specification
│   ├── tests/                           # Native C++ & Python automated test suite
│   ├── docs/                            # Protocol spec, wiring guide, test plan & validation report
│   └── logs/                            # Host-side execution logs
│
├── task9_preservation_archive_20261028/ # Complete, dated preservation directory
│   ├── TASK9_ARCHIVE_README.md          # Comprehensive archive documentation & restoration guide
│   ├── FILE_INVENTORY.txt               # Itemized inventory of all 1,263 files
│   ├── SHA256SUMS.txt                   # Cryptographic SHA-256 manifest (100% verified)
│   ├── raspberry_pi_artifacts/          # Artifacts retrieved from the physical Raspberry Pi
│   │   ├── task9_serial_communication_report.md # 13 physical hardware test verdicts (all PASS)
│   │   └── pi_workspace_remote_copy/    # Full copy of remote Pi test workspace
│   └── backups/                         # Hardware flash backups
│
├── task9_preservation_archive_20261028.tar.gz # Verified 10.2 MB compressed archive tarball
│
└── AutoChair_ESP32_Backup/              # Original physical ESP32 flash read-back slices
    ├── esp32_flash_full_4MB.bin         # Assembled 4 MB binary dump
    ├── esp32_chunk_0_512KB.bin to _7    # Eight contiguous 512 KB flash slices
    └── SHA256SUMS.txt                   # Checksums for flash slices
```

---

## 3. Verified Test Evidence Summary

### A. Host-Side Automated Verification
* **Native C++ Assertion Suite (`tests/test_cpp_state_machine`):**
  - **92 assertion checks PASSED** (0 failures, exit code: `0`).
  - Enforced startup in `IDLE`, first-heartbeat requirement (arming and recovery prohibited before first heartbeat), RFC 1982 sequence monotonicity & rollover ($2^{32}-1 \rightarrow 0$), bounded 16-entry command cache deduplication, and frame parser recovery.
* **Python Automated Test Suite (`tests/run_all_tests.py`):**
  - **37 automated tests PASSED** (0 failures, 0 errors, exit code: `0`):
    - `test_protocol.py`: 20 tests.
    - `test_state_machine_logic.py`: 11 tests.
    - `test_client_harness.py`: 6 tests (full-duplex virtual serial PTY loopback).
* **PlatformIO ESP32 Firmware Build:**
  - Build command: `pio run -d esp32_firmware` succeeded (Flash: 21.3%, RAM: 7.1%, 0 errors, 0 warnings).

### B. Physical Hardware Verification (Recorded on Raspberry Pi 4)
From `task9_serial_communication_report.md` (tested on Raspberry Pi 4 connected to ESP32 Dev Module via `/dev/ttyUSB0` at 115200 baud):

| # | Physical Test | Observed Result | Verdict |
|---|---|---|---|
| 1 | Serial connection | Pi connected to ESP32 | **PASS** |
| 2 | PING | ACCEPTED; PONG | **PASS** |
| 3 | GET_STATUS | ACCEPTED; STATUS_OK | **PASS** |
| 4 | SET_MODE MANUAL | ACCEPTED; MODE_UPDATED | **PASS** |
| 5 | Motion command in IDLE | REJECTED; SYSTEM_NOT_ARMED_IDLE_STATE | **PASS** (expected rejection) |
| 6 | RESET without healthy heartbeat | REJECTED; CANNOT_RESET_WITHOUT_HEALTHY_HEARTBEAT | **PASS** (expected rejection) |
| 7 | RESET with heartbeat restored | ACCEPTED; SAFETY_LATCH_CLEARED_READY | **PASS** |
| 8 | STOP in READY | ACCEPTED; MOTION_STOPPED | **PASS** |
| 9 | Unknown command | REJECTED; UNKNOWN_COMMAND | **PASS** |
| 10 | Malformed JSON | MALFORMED_FRAME event received; state remained READY | **PASS** |
| 11 | Heartbeat timeout | HEARTBEAT_TIMEOUT event; state changed to latched SAFE | **PASS** |
| 12 | Status while SAFE | ACCEPTED; reported state SAFE | **PASS** |
| 13 | Recovery after heartbeat restored | ACCEPTED; state returned to READY | **PASS** |

* **Sensor Status:** Telemetry explicitly reported ultrasonic, IMU, and encoder sensors as `NOT_CONNECTED`.
* **Motion Status:** Simulated only (`_simulatedMotion: STOP`).

---

## 4. Flash Backup Safety Notice

> [!CAUTION]
> **DO NOT USE THE PHYSICAL FLASH BACKUP FOR RESTORATION.**  
> The completeness and integrity of the physical flash dump files in `AutoChair_ESP32_Backup/` have **not been established**. They are preserved strictly for historical reference and must NOT be written back to any physical board without independent verification.
