# AutoChair Task 9 — Standalone Pi ↔ ESP32 Serial Communication Archive
**Archive Creation Date:** October 28, 2026 (Updated with Raspberry Pi Artifacts)  
**Archive Directory:** `/Users/admin/Desktop/task9_preservation_archive_20261028/`  
**Compressed Archive:** `/Users/admin/Desktop/task9_preservation_archive_20261028.tar.gz`  
**Original Mac Project:** `/Users/admin/Desktop/pi_esp32_serial_test/`  
**Original Raspberry Pi Project:** `~/pi_esp32_serial_test/` (on `autochair-pi.local`)  
**Flash Backup Directory:** `/Users/admin/Desktop/AutoChair_ESP32_Backup/`  

---

## 1. Executive Summary & Objective

This preservation archive captures the complete, self-contained implementation, build outputs, automated test suites, documentation, physical hardware test reports, and flash backups for **Task 9: Standalone Raspberry Pi 4 ↔ ESP32 USB Serial Communication Test**.

### Safety & Scope Boundaries
* **Pure Communication Bench Validation:** Tests and validates bidirectional USB serial framing, command dispatch, telemetry exchange, heartbeat watchdog monitoring, fail-safe state latching, and recovery.
* **No Motor or High-Voltage Actuation:** All wheelchair motion was purely simulated via software buffers (`_simulatedMotion`). No physical motor controllers, motors, brakes, or 24V wheelchair batteries were connected.
* **Isolated Project:** This project was developed, tested, and archived strictly outside the main AutoChair repository. The main repository (`/Users/admin/Desktop/AutoChair`) and its branches remain completely untouched.

---

## 2. Archive Structure & Inventory

```text
task9_preservation_archive_20261028/
├── TASK9_ARCHIVE_README.md          # Comprehensive archive documentation and restoration guide
├── FILE_INVENTORY.txt               # Detailed file-by-file inventory (paths, sizes, types)
├── SHA256SUMS.txt                   # Cryptographic SHA-256 manifest of all preserved files
│
├── pi_esp32_serial_test/            # Complete recursive snapshot of the Mac project workspace
│   ├── README.md                    # Standalone serial test overview & command guide
│   ├── platformio.ini               # PlatformIO build configuration (esp32dev, 115200 baud)
│   ├── .venv/                       # Python virtual environment containing dependencies
│   ├── docs/                        # Formal architecture, protocol, and verification documents
│   │   ├── SERIAL_PROTOCOL.md       # Newline-delimited JSON serial protocol specification
│   │   ├── SETUP_AND_WIRING.md      # Physical USB connection, electrical isolation & safety
│   │   ├── TEST_PLAN.md             # Multi-stage test plan (Stage A through Stage H)
│   │   └── VALIDATION_REPORT.md     # Test verification report & audit results
│   ├── esp32_firmware/              # ESP32 C++ firmware source, headers & build artifacts
│   │   ├── platformio.ini           # Firmware build settings & dependencies (ArduinoJson 6)
│   │   ├── src/                     # C++ source files (main.cpp, protocol.cpp, state_machine.cpp)
│   │   └── .pio/                    # Build outputs (firmware.bin, firmware.elf, map, and libdeps)
│   ├── protocol/                    # Protocol definitions
│   │   └── protocol_spec.json       # Formal machine-readable JSON protocol schema
│   ├── raspberry_pi/                # Python client & monitoring tools
│   │   ├── __init__.py
│   │   ├── protocol.py              # Strict JSON framing, type checking, & enum definitions
│   │   ├── client.py                # Thread-safe client, 250ms heartbeat worker, recovery
│   │   └── cli_monitor.py           # Interactive CLI monitor & live telemetry dashboard
│   ├── tests/                       # Automated test suites & test runners
│   │   ├── run_all_tests.py         # Master test runner (executes C++ and Python suites)
│   │   ├── test_cpp_state_machine   # Compiled native C++ host test executable
│   │   ├── test_cpp_state_machine.cpp # 92 native C++ assertion checks
│   │   ├── test_protocol.py         # 20 protocol & frame buffer unit tests
│   │   ├── test_state_machine_logic.py # 11 state machine & sequence logic unit tests
│   │   ├── test_client_harness.py   # 6 full-duplex PTY loopback integration tests
│   │   ├── arduino_mock.h           # Host-side Arduino & Serial mocks for native testing
│   │   └── Arduino.h                # Mock redirection header
│   └── logs/                        # Historical and final execution logs
│       ├── stage_a2_automated_test_report.log
│       ├── stage_a3_automated_test_report.log
│       └── stage_a3_final_verification.log
│
├── backups/                         # Physical ESP32 flash backups
│   └── AutoChair_ESP32_Backup/
│       ├── esp32_flash_full_4MB.bin # Full 4 MB assembled binary dump of physical ESP32 flash
│       ├── esp32_chunk_0_512KB.bin  # Segment 0 (0x000000 - 0x080000) containing bootloader & partitions
│       ├── esp32_chunk_1_512KB.bin  # Segment 1 (0x080000 - 0x100000)
│       ├── esp32_chunk_2_512KB.bin  # Segment 2 (0x100000 - 0x180000)
│       ├── esp32_chunk_3_512KB.bin  # Segment 3 (0x180000 - 0x200000)
│       ├── esp32_chunk_4_512KB.bin  # Segment 4 (0x200000 - 0x280000)
│       ├── esp32_chunk_5_512KB.bin  # Segment 5 (0x280000 - 0x300000)
│       ├── esp32_chunk_6_512KB.bin  # Segment 6 (0x300000 - 0x380000)
│       ├── esp32_chunk_7_512KB.bin  # Segment 7 (0x380000 - 0x400000)
│       └── SHA256SUMS.txt           # SHA-256 verification hashes for all flash chunks
│
└── raspberry_pi_artifacts/          # Preserved artifacts retrieved from the Raspberry Pi
    ├── README_PI_ARTIFACTS.md       # Manifest & description of retrieved Pi artifacts
    ├── task9_serial_communication_report.md # Bench test report recorded on the Raspberry Pi
    └── pi_workspace_remote_copy/    # Full remote test workspace copied from Raspberry Pi
        ├── __init__.py
        ├── client.py
        ├── protocol.py
        ├── cli_monitor.py
        ├── cli_monitor.py.bak
        ├── logs/
        └── __pycache__/
```

---

## 3. Preserved Test Evidence

Test evidence is strictly segregated into **host-side automated software tests** and **physical-device hardware communication tests**.

### A. Host-Side Automated Test Evidence (Mac Host)
* **Native C++ Assertion Suite (`tests/test_cpp_state_machine`):**
  - **92 assertion checks PASSED** (0 failures, exit code 0).
  - Validates non-active startup in `IDLE`.
  - Enforces first heartbeat requirement: `RESET`, `RECOVER`, and `ARM` are rejected prior to receiving a valid heartbeat.
  - Validates RFC 1982 Serial Number Arithmetic sequence monotonicity:
    - Normal sequence advancement accepted.
    - Duplicate and out-of-order sequences rejected without watchdog refresh.
    - Proof that repeated heartbeats cannot keep the system healthy indefinitely (failsafe trips to `SAFE`).
    - Unsigned 32-bit sequence rollover ($2^{32}-1 \rightarrow 0$) accepted seamlessly.
  - Validates bounded 16-entry circular command cache deduplication.
  - Validates central frame processor (`processIncomingLine`): strict type checking (boolean values rejected for integer fields), malformed JSON recovery, and `SerialFrameParser` recovery after oversized frames (>512 bytes).
* **Python Automated Test Suite (`tests/run_all_tests.py`):**
  - **37 automated unit & integration tests PASSED** (0 failures, 0 errors, exit code 0):
    - `test_protocol.py`: 20 tests (15 `TestProtocolMessage` + 5 `TestFrameBuffer`).
    - `test_state_machine_logic.py`: 11 tests (Python reference model).
    - `test_client_harness.py`: 6 tests (full-duplex virtual serial PTY loopback).
* **PlatformIO ESP32 Firmware Build:**
  - Build command: `pio run -d esp32_firmware`
  - Result: SUCCESS (0 errors, 0 warnings).
  - RAM usage: 23,360 bytes (7.1%).
  - Flash usage: 279,237 bytes (21.3%).
  - Firmware binary artifact: `esp32_firmware/.pio/build/esp32dev/firmware.bin`.

### B. Physical-Device Test Evidence (Recorded on Raspberry Pi)
From `raspberry_pi_artifacts/task9_serial_communication_report.md` (tested on Raspberry Pi 4 connected to ESP32 Dev Module via `/dev/ttyUSB0` at 115200 baud):

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

* **Sensor Transparency:** ESP32 telemetry explicitly reported ultrasonic, IMU, and encoder sensors as `NOT_CONNECTED`.
* **Motion Simulation:** Motion was virtual (`_simulatedMotion: STOP`); no physical wheelchair motors or brakes were connected.

---

## 4. Preservation & Restoration Notes

### 1. Extract the Archive
```bash
tar -xzf /Users/admin/Desktop/task9_preservation_archive_20261028.tar.gz -C /Users/admin/Desktop/
```

### 2. Verify File Integrity
```bash
cd /Users/admin/Desktop/task9_preservation_archive_20261028
shasum -a 256 -c SHA256SUMS.txt
```

### 3. Re-run Automated Test Suite
```bash
cd /Users/admin/Desktop/task9_preservation_archive_20261028/pi_esp32_serial_test
python3 tests/run_all_tests.py
```

### 4. Rebuild ESP32 Firmware
```bash
cd /Users/admin/Desktop/task9_preservation_archive_20261028/pi_esp32_serial_test/esp32_firmware
pio run
```

### 5. Critical Flash Backup Warning & Limitation
> [!CAUTION]
> **DO NOT USE THE EXISTING ESP32 FLASH BACKUP FOR RESTORATION.**
> The completeness and valid integrity of the physical flash dump files in `backups/AutoChair_ESP32_Backup/` have **not been established**. They are preserved strictly for historical reference and must NOT be written to any physical board without independent verification.

---

## 5. Excluded Files & Integrity Policy

* **Self-Exclusion from Manifest:** `SHA256SUMS.txt` is intentionally excluded from its own checksum list to maintain mathematical consistency.
* **Preservation Scope:** No project files, virtual environments, binaries, or logs were excluded.
* **Security:** No credentials, tokens, or private keys are contained in this archive.
