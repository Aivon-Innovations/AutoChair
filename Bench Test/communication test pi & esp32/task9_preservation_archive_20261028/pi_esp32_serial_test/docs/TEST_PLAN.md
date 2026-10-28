# AutoChair — Standalone Raspberry Pi ↔ ESP32 Test Plan

**Document ID:** `PLAN-TEST-PI-ESP32-001`  
**Execution Strategy:** Multi-stage validation progressing from automated host-level software tests to live physical hardware bench testing.  
**Revision:** Stage A.3 (Native C++ Verification, Strict Heartbeat & Bounded Cache Policies)

---

## 1. Test Stage Summary

| Stage | Title | Environment | Objectives | Status |
| :---: | :--- | :--- | :--- | :---: |
| **Stage A.1** | **Initial Software Build & Tests** | Mac / Host | Compile firmware with PlatformIO, initial protocol and harness tests. | **PASSED (20/20)** |
| **Stage A.2** | **Safety Code Audit & Native Tests** | Mac / Host | Strict type validation, rollover safety, rate limiting, disconnect resilience. | **PASSED (33 C++ checks + 32 Python tests)** |
| **Stage A.3** | **Close Safety & Protocol Gaps** | Mac / Host | First heartbeat requirement, sequence monotonicity, bounded 16-command cache, top-level field validation. | **PASSED (51 C++ checks + 34 Python tests)** |
| **Stage B** | **SSH & Pi Environment Inspection** | Raspberry Pi 4 | Minimal read-only inspection of OS, Python, `pyserial`, and dev directories via SSH. | *Pending User SSH* |
| **Stage C** | **USB Connection & Isolation Check** | Physical Bench | Connect ESP32 to Pi 4 USB-A; verify 24V wheelchair battery/motor isolation. | *Pending Hardware* |
| **Stage D** | **Serial Device Port Identification** | Raspberry Pi 4 | Discover actual ESP32 `/dev/tty*` path using `dmesg` / `lsusb` / `udev` without guessing. | *Pending Hardware* |
| **Stage E** | **Bidirectional Communication** | Pi ↔ ESP32 | Validate startup banners, `PING`, `GET_STATUS`, `SET_MODE`, and telemetry streaming. | *Pending Hardware* |
| **Stage F** | **Invalid Command & Malformed Input** | Pi ↔ ESP32 | Inject corrupt JSON and invalid commands; verify clean rejection without firmware lockup. | *Pending Hardware* |
| **Stage G** | **Heartbeat Timeout, Latch & Recovery** | Pi ↔ ESP32 | Cut heartbeat stream; verify transition to `SAFE`, latch maintenance, and recovery via `RESET`. | *Pending Hardware* |
| **Stage H** | **Reconnection & Endurance Link** | Pi ↔ ESP32 | Physical USB disconnect/reconnect and 10-minute sustained telemetry link without packet loss. | *Pending Hardware* |

---

## 2. Detailed Pass/Fail Criteria (Stage A.3)

### Stage A.3 — Host-Side Automated Tests (Completed)
- **A.3.1 Native C++ Assertion Checks:** 51/51 checks pass testing real C++ `SafetyStateMachine` and `SerialFrameParser` code on host:
  1. Startup in `IDLE` with `_firstHeartbeatReceived = false`.
  2. Rejection of `RESET`/`ARM` before first heartbeat (`CANNOT_RESET_BEFORE_FIRST_HEARTBEAT`).
  3. Rejection of `MOVE` in `IDLE`.
  4. Acceptance of first valid heartbeat and arming to `READY`.
  5. Movement into `ACTIVE_TEST` and stopping into `READY`.
  6. Rejection of duplicate or out-of-order sequence numbers (`_staleHeartbeatCount++`).
  7. Proof that duplicate sequences do NOT refresh watchdog, causing transition to `SAFE`.
  8. Latch maintenance in `SAFE` when heartbeats resume.
  9. Rejection of `RESET` when heartbeat becomes stale.
  10. Successful recovery to `READY` after fresh heartbeat.
  11. Bounded command cache: duplicate `msg_id` recognized, suppression within 16 commands, eviction after 16 subsequent commands.
  12. Rollover math across `0xFFFFFFFF` timestamps and sequence numbers.
  13. Oversized frame discard followed by valid frame recovery.
- **A.3.2 Python Test Breakdown (34 Tests Total):**
  * `test_protocol.py`: **19 Tests** (14 `TestProtocolMessage` strict type tests + 5 `TestFrameBuffer` tests)
  * `test_state_machine_logic.py`: **10 Tests** (Python reference model mirroring C++ rules)
  * `test_client_harness.py`: **5 Tests** (PTY virtual loopback, disconnect detection, CLI imports)

### Stages B to H — Physical Hardware Validation (Pending)
*All physical tests remain pending and will be performed sequentially one stage at a time after user confirmation.*
