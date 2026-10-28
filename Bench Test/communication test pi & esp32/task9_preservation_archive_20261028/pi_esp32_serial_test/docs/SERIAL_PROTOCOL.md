# AutoChair — Standalone Raspberry Pi ↔ ESP32 Serial Protocol Specification

**Document ID:** `SPEC-SERIAL-PI-ESP32-001`  
**Protocol Version:** `1`  
**Baud Rate:** `115200` (8-N-1)  
**Framing:** Newline-delimited JSON (`\n`)  
**Maximum Frame Size:** `512 bytes`  
**Revision:** Stage A.3 Final Verification (Heartbeat Ack Distinction, RFC 1982 Sequence Rollover & Central Firmware Validation)

---

## 1. Safety State Machine Architecture

The ESP32 firmware implements six distinct states:

```text
       +---------------------------------------------+
       |                    BOOT                     |
       |         (Hardware / Serial Init)            |
       +----------------------+----------------------+
                              |
                              v
       +---------------------------------------------+
       |                    IDLE                     | <---+
       |  (Non-active startup; motion prohibited;    |     |
       |   first heartbeat required before arming)   |     |
       +----------------------+----------------------+     |
                              |                            |
                              | CMD: RESET / ARM           |
                              | (Requires first heartbeat  |
                              |  AND heartbeat age <=1.5s) |
                              v                            |
       +---------------------------------------------+     |
       |                   READY                     |     |
       |  (Armed & waiting; heartbeats enforced)     |     |
       +-----------+---------------------+-----------+     |
                   |                     |                 |
     CMD: MOVE     |         CMD: STOP   | Heartbeat       |
     (Motion       |         or complete | Timeout         |
     simulated)    v                     | (>1500 ms)      |
       +---------------------+           |                 |
       |     ACTIVE_TEST     |           |                 |
       | (Movement simulated)|           |                 |
       +-----------+---------+           |                 |
                   |                     |                 |
                   | Heartbeat Timeout   |                 |
                   | (>1500 ms)          |                 |
                   +----------+----------+                 |
                              |                            |
                              v                            |
       +---------------------------------------------+     |
       |                    SAFE                     |     |
       |    (Failsafe latch; motion prohibited;      |     |
       |     resuming heartbeat does NOT clear)      |     |
       +----------------------+----------------------+     |
                              |                            |
                              | CMD: RESET                 |
                              | (Heartbeat healthy <=1.5s) |
                              v                            |
       +---------------------------------------------+     |
       |                    READY                    | ----+
       +---------------------------------------------+
```

---

## 2. Heartbeat Policy & Sequence Validation (Stage A.3)

* **Interval:** Transmitted by Pi every **250 ms**.
* **Timeout Threshold:** **1500 ms** on ESP32.
* **First Heartbeat Requirement:**
  * Initializing the state machine does **NOT** count as receiving a heartbeat (`_lastHeartbeatMillis = 0`).
  * System starts in `IDLE`.
  * `RESET`, `RECOVER`, and `ARM` are strictly **REJECTED** (`CANNOT_RESET_BEFORE_FIRST_HEARTBEAT`) until at least one real heartbeat has been received.
* **Sequence Monotonicity & Rollover Policy (RFC 1982 Serial Number Arithmetic):**
  * Sequence numbers are unsigned 32-bit integers (`uint32_t`).
  * The advance difference is calculated using unsigned modular subtraction:
    $$\Delta = (\text{uint32\_t})(\text{seq} - \text{last\_seq})$$
  * **Valid Forward Advance:** $0 < \Delta < 2^{31}$ (`0x80000000UL`). Updates `last_seq`, refreshes watchdog timer, and returns `ack: true`, `status: "ACCEPTED"`.
  * **Duplicate Sequence:** $\Delta = 0$ (`seq == last_seq`). Increments `_staleHeartbeatCount`, **DOES NOT** refresh watchdog timer, returns `ack: false`, `status: "REJECTED"`, `reason: "STALE_OR_DUPLICATE_HEARTBEAT"`.
  * **Out-of-Order / Stale Sequence:** $\Delta \ge 2^{31}$ (`seq` belongs to past window). Increments `_staleHeartbeatCount`, **DOES NOT** refresh watchdog timer, returns `ack: false`, `status: "REJECTED"`.
  * **Rollover Precision:** Transition from `0xFFFFFFFF` to `0` yields $\Delta = (0 - 0xFFFFFFFF) = 1$, which is recognized as a valid forward advance without arbitrary threshold heuristics.
* **Heartbeat Acknowledgement Distinction:**
  * **Accepted Ack:**
    ```json
    {
      "version": 1,
      "msg_type": "HEARTBEAT",
      "msg_id": 1001,
      "timestamp_ms": 12050,
      "payload": {
        "seq": 42,
        "ack": true,
        "status": "ACCEPTED",
        "reason": "HEARTBEAT_OK",
        "state": "READY"
      }
    }
    ```
  * **Rejected Ack (Stale/Duplicate):**
    ```json
    {
      "version": 1,
      "msg_type": "HEARTBEAT",
      "msg_id": 1002,
      "timestamp_ms": 12300,
      "payload": {
        "seq": 42,
        "ack": false,
        "status": "REJECTED",
        "reason": "STALE_OR_DUPLICATE_HEARTBEAT",
        "state": "READY"
      }
    }
    ```
  * **Raspberry Pi Client Safety:** The Pi client checks `ack == true` and `status == "ACCEPTED"`. If rejected, it records `stats["rejected_hb"] += 1` and **does not** refresh its heartbeat acknowledgement timestamp.
* **Failsafe Latching:** Exceeding 1500 ms trips state to `SAFE`.
* **No Auto-Recovery:** Heartbeat resumption alone does **NOT** clear `SAFE`.

---

## 3. Bounded Duplicate Command Handling (Stage A.3)

To prevent duplicate command execution or replayed state transitions without unlimited memory growth:
* **Mechanism:** Bounded FIFO circular cache of size **16** (`RECENT_CMD_CACHE_SIZE = 16`).
* **Cache Contents:** Stores `msg_id`, `ack`, `result`, `reason`, `state`, and `mode`.
* **Behavior:**
  * When a command arrives whose `msg_id` is present in the cache, it is recognized as a duplicate (`DUPLICATE_COMMAND_CACHED`).
  * The ESP32 echoes the cached acknowledgment without repeating simulated motion, mode changes, or safety latch clears.
* **Eviction Policy:** Strictly FIFO. Oldest entry is overwritten after 16 subsequent unique commands.
* **Scope Boundary:** Provides replay protection within a 16-command sliding window; does not claim indefinite replay defense.

---

## 4. Frame Structure & Strict Validation Rules

Every transmission is a single JSON object terminated by a single newline character `\n` (`0x0A`).

```json
{
  "version": 1,
  "msg_type": "COMMAND | RESPONSE | TELEMETRY | HEARTBEAT | EVENT",
  "msg_id": 1001,
  "timestamp_ms": 1234567,
  "payload": {}
}
```

### Strict Type & Range Rules
* `version`: Integer strictly equal to `1`. Booleans (`true`/`false`) and strings are rejected.
* `msg_id`: Unsigned integer &ge; 0. Booleans or strings are rejected.
* `timestamp_ms`: Unsigned integer &ge; 0. Booleans or strings are rejected.
* `msg_type`: Must be one of `COMMAND`, `RESPONSE`, `TELEMETRY`, `HEARTBEAT`, or `EVENT`.
* `payload`: Must be a JSON object dictionary.

---

## 5. Telemetry & Sensor Transparency

Transmitted automatically every **500 ms**:

```json
{
  "version": 1,
  "msg_type": "TELEMETRY",
  "msg_id": 120,
  "timestamp_ms": 60000,
  "payload": {
    "state": "READY",
    "mode": "MANUAL",
    "uptime_ms": 60000,
    "telemetry_seq": 120,
    "heartbeat": {
      "ok": true,
      "last_received_age_ms": 45,
      "missed_count": 0
    },
    "sensors": {
      "ultrasonic": "NOT_CONNECTED",
      "imu": "NOT_CONNECTED",
      "encoders": "NOT_CONNECTED"
    },
    "diagnostics": {
      "malformed_frames": 0
    },
    "simulated_motion": "STOP"
  }
}
```

> [!NOTE]
> All sensor fields (`ultrasonic`, `imu`, `encoders`) are explicitly labeled `"NOT_CONNECTED"`.

---

## 6. Test & Verification Boundaries

* **Exercised by Host-Side C++ Tests:**
  - `esp32_firmware/src/protocol.cpp` (`processIncomingLine`, `sendHeartbeatAck`, `sendJsonResponse`, `SerialFrameParser`).
  - `esp32_firmware/src/state_machine.cpp` (all state transitions, first-heartbeat gating, RFC 1982 sequence window, bounded 16-entry command cache).
* **Pending Physical Hardware Tests (Stage B & beyond):**
  - ESP32 hardware UART peripheral register configuration and hardware FIFO buffering.
  - Baud rate clock drift, physical cable bounce, and electrical line noise at 115200 baud.
  - USB-to-UART bridge chip (CP2102/CH340) behavior under Linux tty driver disconnect/reconnect.
