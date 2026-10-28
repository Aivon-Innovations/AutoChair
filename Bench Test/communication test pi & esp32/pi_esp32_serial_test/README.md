# AutoChair — Standalone Raspberry Pi ↔ ESP32 USB Serial Communication Test

This is an independent, standalone bench-test project designed to validate bidirectional USB serial communication, safety state-machine behavior, heartbeat monitoring, command rejection, and telemetry exchange between a **Raspberry Pi 4 Model B** and an **ESP32** microcontroller.

> [!NOTE]
> **COMPLETELY SEPARATE FROM MAIN REPOSITORY:**
> This project is maintained outside the main AutoChair repository at `/Users/admin/Desktop/pi_esp32_serial_test/` and does not touch or alter the `main`, `alvira-dev`, or any other production firmware branch.

---

## 📁 Project Directory Structure

```text
pi_esp32_serial_test/
├── esp32_firmware/               # Standalone ESP32 PlatformIO firmware project
│   ├── platformio.ini           # Build config: esp32dev, Arduino framework, 115200 baud
│   └── src/
│       ├── main.cpp             # Serial processing loop, rate limiting & telemetry dispatcher
│       ├── protocol.h / .cpp    # Stream framing buffer & JSON serializers
│       └── state_machine.h / .cpp # Safety state machine & 1500 ms heartbeat monitor
├── raspberry_pi/                # Python communication package & CLI monitor
│   ├── __init__.py
│   ├── protocol.py              # Strict type framing buffer, protocol message schemas & enums
│   ├── client.py                # Thread-safe client, disconnect resilience, background 250 ms heartbeats
│   └── cli_monitor.py           # Universal interactive CLI & live telemetry dashboard
├── protocol/
│   └── protocol_spec.json       # Formal JSON schema / protocol definition
├── tests/                       # Automated test suite
│   ├── Arduino.h / arduino_mock.h # Host-side Arduino mocks for native C++ compilation
│   ├── test_cpp_state_machine.cpp # Native C++ state machine & parser test suite (33 Checks PASS)
│   ├── test_protocol.py         # 14 unit tests for framing, strict typing, and malformed JSON
│   ├── test_state_machine_logic.py # 8 unit tests for timeout, latching, and rejection rules
│   ├── test_client_harness.py   # 5 end-to-end integration tests using virtual PTY serial harness
│   └── run_all_tests.py         # Master test runner (Native C++ + 32 Python tests PASS)
├── logs/                        # Staged directory for raw test logs
│   └── stage_a2_automated_test_report.log # Full execution log of automated tests
├── docs/                        # Formal engineering documentation
│   ├── SETUP_AND_WIRING.md      # Physical architecture, cable rules, and SSH guide
│   ├── SERIAL_PROTOCOL.md       # Complete JSON protocol, state machine & framing specification
│   ├── TEST_PLAN.md             # Detailed stage-by-stage testing rubric (Stages A to H)
│   └── VALIDATION_REPORT.md     # Engineering validation report
└── README.md                    # Project overview documentation
```

---

## ⚡ Key Technical Specifications

* **Physical Interface:** USB Serial (CDC / UART) via Raspberry Pi 4 USB-A port to ESP32 micro-USB / Type-C.
* **Baud Rate:** `115200` baud (8 data bits, no parity, 1 stop bit).
* **Framing:** Newline-delimited JSON (`\n`), buffer protection at 512 bytes with recovery after oversized frames.
* **State Lifecycle:** `BOOT` $\rightarrow$ `IDLE` (non-active startup) $\rightarrow$ `READY` (armed via explicit `RESET`/`ARM`) $\rightarrow$ `ACTIVE_TEST` (simulated movement) $\rightarrow$ `SAFE` (heartbeat timeout latch) $\rightarrow$ `READY` (explicit recovery).
* **Heartbeat Stream:** Pi sends periodic heartbeat ping every **250 ms**.
* **Safety Timeout:** ESP32 trips to `SAFE` state if heartbeat is absent for **> 1500 ms**.
* **Failsafe Latching:** Resuming heartbeats does **NOT** auto-reactivate the ESP32. It remains latched in `SAFE` until an explicit `RESET` command is issued.
* **Sensor Transparency:** All ultrasonic, IMU, and encoder telemetry fields are explicitly labeled `"NOT_CONNECTED"`.
* **Disconnection Resilience:** Client reader thread failure immediately updates `is_connected = False` and aborts blocked commands fast.

---

## 🧪 Running Automated Tests (Software Only)

You can run the entire automated verification suite on your Mac without hardware:

```bash
python3 "/Users/admin/Desktop/pi_esp32_serial_test/tests/run_all_tests.py"
```

**Results:**
* Native C++ State Machine: **33 / 33 Assertion Checks PASSED**
* Python Protocol & Integration Tests: **32 / 32 Unit/Integration Tests PASSED**
* Execution Time: **~6.5 seconds**

---

## 🔨 Building ESP32 Firmware

```bash
pio run -d "/Users/admin/Desktop/pi_esp32_serial_test/esp32_firmware"
```

**Result:** **SUCCESS** (RAM: 6.7%, Flash: 21.3%, 0 warnings).

---

## 💻 Running the Raspberry Pi CLI Monitor

When on the Raspberry Pi (or test host with connected ESP32):

```bash
# Direct execution:
python3 "/Users/admin/Desktop/pi_esp32_serial_test/raspberry_pi/cli_monitor.py" --port /dev/ttyUSB0

# Or module execution from project root:
cd "/Users/admin/Desktop/pi_esp32_serial_test"
python3 -m raspberry_pi.cli_monitor --port /dev/ttyUSB0
```

---

## 🛡️ Safety Disclaimer

> [!CAUTION]
> This mini-project is strictly a **communication and safety state machine validation**.
> * No 24V wheelchair batteries, motor controllers, or physical drive motors are connected.
> * All movement commands are simulated in software.
