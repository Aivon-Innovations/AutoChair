# `ESP32_PI_PROTOCOL.md`

**Project:** AutoChair – AI-Powered Smart Wheelchair  
**Company:** Aivon Innovations Pvt. Ltd.  
**Platform:** Raspberry Pi 4 ↔ ESP32-WROOM-32D  
**Document:** ESP32 ↔ Raspberry Pi Communication Protocol  
**Version:** V1.0  
**Status:** Development Specification

---

# 1. Purpose

This document defines the communication protocol between the Raspberry Pi and ESP32 in AutoChair.

The protocol provides a structured interface for:

- Commands from Raspberry Pi to ESP32
- Responses from ESP32 to Raspberry Pi
- Sensor telemetry
- System status
- Safety status
- Fault reporting
- Heartbeat monitoring
- Diagnostics
- Future wheelchair-interface commands

The protocol must keep **high-level application logic** separate from **real-time embedded hardware logic**.

---

# 2. Architectural Position

```text
                 RASPBERRY PI 4
        ┌────────────────────────────┐
        │                            │
        │ Touchscreen                │
        │ Voice                      │
        │ Camera                     │
        │ Navigation                 │
        │ Route Planning             │
        │ Application Logic          │
        │                            │
        └─────────────┬──────────────┘
                      │
              ESP32 ↔ Pi Protocol
                      │
                      ▼
              ┌───────────────┐
              │     ESP32     │
              │               │
              │ State Machine │
              │ Safety        │
              │ Sensors       │
              │ Encoders      │
              │ IMU           │
              │ Diagnostics   │
              └───────────────┘
```

The Raspberry Pi should not directly manipulate ESP32 hardware registers or sensor drivers.

The ESP32 should not implement Raspberry Pi application functions such as touchscreen rendering, voice processing, route planning, or high-level navigation.

---

# 3. Protocol Design Principles

The protocol must be:

1. Explicit
2. Versioned
3. Deterministic
4. Validatable
5. Extensible
6. Observable
7. Fault-aware
8. Safety-aware

Every message must be independently parseable and validated.

No command should be accepted merely because its command name is recognized.

---

# 4. Communication Transport

The protocol is transport-independent at the application layer.

The initial implementation should provide a transport abstraction:

```text
Application Protocol
        │
        ▼
Transport Interface
        │
   ┌────┴────┐
   │         │
 Serial    Future
Transport  Transport
```

Possible initial transport:

```text
Raspberry Pi
      │
      │ Serial / USB / UART
      │
      ▼
    ESP32
```

The exact physical Pi↔ESP32 connection should be selected and verified during implementation.

The protocol itself must not depend on a specific physical transport.

---

# 5. Protocol Version

Every protocol implementation must expose a protocol version.

Initial version:

```text
Protocol Version: 1
```

The version must be included in status information and should be available during handshake.

Example:

```json
{
  "protocol_version": 1
}
```

Future incompatible protocol changes must increment the major protocol version.

---

# 6. Message Categories

Messages are divided into six primary categories.

```text
Messages
│
├── Command
├── Response
├── Telemetry
├── Event
├── Heartbeat
└── Diagnostic
```

---

# 7. Message Direction

```text
Raspberry Pi → ESP32
│
├── Commands
├── Heartbeat
└── Configuration requests

ESP32 → Raspberry Pi
│
├── Responses
├── Telemetry
├── Events
├── Heartbeat acknowledgement
└── Diagnostics
```

---

# 8. Common Message Structure

The logical representation of every protocol message should contain:

```json
{
  "version": 1,
  "message_type": "COMMAND",
  "message_id": 1001,
  "timestamp": 123456,
  "payload": {}
}
```

### Fields

| Field | Purpose |
|---|---|
| `version` | Protocol version |
| `message_type` | Message category |
| `message_id` | Message identifier |
| `timestamp` | Sender-side timestamp |
| `payload` | Message-specific data |

The final binary/framed representation will be defined by the transport implementation.

---

# 9. Message ID

Each command/request should have a unique `message_id`.

Example:

```text
1001
1002
1003
...
```

The Raspberry Pi should use the identifier to correlate responses with requests.

Example:

```text
Pi → ESP32
message_id = 1042
GET_STATUS

ESP32 → Pi
message_id = 1042
RESPONSE
```

The ESP32 should not rely on message ordering alone to correlate requests.

---

# 10. Timestamp

Messages should contain a sender-side timestamp.

For the ESP32 this may be based on:

```text
millis()
```

or an equivalent monotonic system clock.

The timestamp is intended for:

- Telemetry correlation
- Debugging
- Event ordering
- Diagnostics

It must not be interpreted as synchronized wall-clock time unless time synchronization is explicitly implemented.

---

# 11. Command Architecture

Commands follow:

```text
Pi
 │
 ▼
Transport
 │
 ▼
Parser
 │
 ▼
Message Validation
 │
 ▼
Command Handler
 │
 ▼
State Validation
 │
 ▼
Safety Validation
 │
 ▼
Subsystem
```

An invalid command must never reach the hardware subsystem.

---

# 12. Command Set — V1

The initial command set is:

```text
PING
GET_STATUS
GET_SENSOR_DATA
GET_ENCODER_DATA
GET_IMU_DATA
SET_MODE
ENABLE
DISABLE
RESET_FAULT
STOP
```

These commands provide the minimum communication interface needed for the first ESP32 implementation.

---

# 13. `PING`

### Purpose

Check communication availability.

### Request

```json
{
  "version": 1,
  "message_type": "COMMAND",
  "message_id": 1001,
  "timestamp": 10000,
  "payload": {
    "command": "PING"
  }
}
```

### Response

```json
{
  "version": 1,
  "message_type": "RESPONSE",
  "message_id": 1001,
  "timestamp": 10001,
  "payload": {
    "command": "PING",
    "status": "OK"
  }
}
```

`PING` must not modify operational state.

---

# 14. `GET_STATUS`

Returns the current embedded-system status.

Example:

```json
{
  "command": "GET_STATUS"
}
```

Response:

```json
{
  "command": "GET_STATUS",
  "status": "OK",
  "system_state": "IDLE",
  "operating_mode": "NONE",
  "safety_state": "CLEAR",
  "enabled": false,
  "active_faults": []
}
```

Status should represent the ESP32's actual current state rather than an assumed application state from the Raspberry Pi.

---

# 15. `GET_SENSOR_DATA`

Requests the latest sensor measurements.

Example response:

```json
{
  "command": "GET_SENSOR_DATA",
  "status": "OK",
  "ultrasonic": [
    {
      "id": 1,
      "distance_mm": 850,
      "status": "OK"
    },
    {
      "id": 2,
      "distance_mm": 910,
      "status": "OK"
    }
  ]
}
```

Sensor-specific data structures should remain extensible.

---

# 16. `GET_ENCODER_DATA`

Returns encoder information.

Example:

```json
{
  "command": "GET_ENCODER_DATA",
  "status": "OK",
  "left": {
    "count": 10240,
    "direction": "FORWARD"
  },
  "right": {
    "count": 10238,
    "direction": "FORWARD"
  }
}
```

The protocol must not assume that encoder counts already represent physical distance.

Wheel circumference, gearbox behavior, encoder mounting, and calibration must be experimentally verified.

---

# 17. `GET_IMU_DATA`

Example response:

```json
{
  "command": "GET_IMU_DATA",
  "status": "OK",
  "imu": {
    "accel": {
      "x": 0.01,
      "y": 0.02,
      "z": 0.98
    },
    "gyro": {
      "x": 0.10,
      "y": 0.03,
      "z": 0.01
    }
  }
}
```

Units must be explicitly documented by the implemented driver.

---

# 18. `SET_MODE`

Sets the requested operating mode.

Supported V1 modes:

```text
NONE
MANUAL
ASSISTED
```

Example:

```json
{
  "command": "SET_MODE",
  "mode": "MANUAL"
}
```

The ESP32 must verify:

1. Mode is recognized.
2. Current state permits mode change.
3. No blocking fault exists.
4. Safety conditions permit the requested transition.

A successful mode selection does not authorize physical movement.

---

# 19. `ENABLE`

Requests system enablement.

Example:

```json
{
  "command": "ENABLE"
}
```

The ESP32 must verify:

```text
Current state = IDLE
        │
        ▼
Valid operating mode?
        │
        ▼
Safety = CLEAR?
        │
        ▼
Required subsystems ready?
        │
        ▼
No blocking fault?
        │
        ▼
Enable accepted
        │
        ▼
READY
```

If any required condition fails:

```text
ENABLE
  │
  ▼
NACK
```

The exact physical meaning of "enable" remains limited to the ESP32/software layer until the wheelchair controller interface is verified.

---

# 20. `DISABLE`

Disables the ESP32 operational authorization.

Example:

```json
{
  "command": "DISABLE"
}
```

Expected behavior:

```text
READY / ACTIVE
       │
       ▼
Disable
       │
       ▼
Cancel operation
       │
       ▼
Revoke enable authorization
       │
       ▼
IDLE
```

If a safety condition is active, the system remains under the corresponding safety restriction rather than being treated as normally idle.

---

# 21. `STOP`

`STOP` is a high-priority operational command.

Example:

```json
{
  "command": "STOP"
}
```

Expected behavior:

```text
STOP
 │
 ▼
Cancel active operation
 │
 ▼
Revoke operational authorization
 │
 ▼
Safety evaluation
 │
 ├── CLEAR → IDLE
 │
 └── SAFE  → SAFE_STOP
```

`STOP` must not be interpreted as proof of physical wheelchair braking.

If a verified wheelchair-control interface is eventually integrated, its stop behavior will be separately validated.

---

# 22. `RESET_FAULT`

Requests fault recovery.

Example:

```json
{
  "command": "RESET_FAULT"
}
```

The ESP32 must not blindly clear fault records.

Recovery requires:

```text
Fault present
    │
    ▼
Check underlying condition
    │
    ├── Still active → NACK
    │
    ▼
Run required checks
    │
    ▼
Clear recoverable fault
    │
    ▼
Return to IDLE
```

Some faults may require physical intervention and therefore remain latched.

---

# 23. Response Structure

Every command should receive a response unless the transport explicitly defines an asynchronous command.

Response:

```json
{
  "version": 1,
  "message_type": "RESPONSE",
  "message_id": 1001,
  "timestamp": 10002,
  "payload": {
    "status": "OK",
    "command": "GET_STATUS"
  }
}
```

Possible response statuses:

```text
OK
NACK
ERROR
BUSY
```

---

# 24. NACK Structure

A rejected command should provide a reason.

Example:

```json
{
  "status": "NACK",
  "reason": "INVALID_STATE"
}
```

Recommended reasons:

```text
INVALID_COMMAND
INVALID_PAYLOAD
INVALID_STATE
SAFETY_RESTRICTED
FAULT_ACTIVE
NOT_READY
NOT_SUPPORTED
INVALID_MODE
INVALID_PARAMETER
BUSY
```

This makes failures diagnosable instead of simply returning `ERROR`.

---

# 25. Telemetry

The ESP32 should periodically publish telemetry without requiring the Raspberry Pi to request every sample.

Architecture:

```text
Sensors
   │
   ▼
Sensor Manager
   │
   ▼
Telemetry Manager
   │
   ▼
ESP32 → Raspberry Pi
```

Telemetry categories:

```text
SYSTEM
SAFETY
ULTRASONIC
IMU
ENCODER
FAULT
HEALTH
```

Telemetry frequency must be configurable and determined during implementation/testing.

---

# 26. System Telemetry

Example:

```json
{
  "message_type": "TELEMETRY",
  "telemetry_type": "SYSTEM",
  "system_state": "READY",
  "operating_mode": "MANUAL",
  "safety_state": "CLEAR",
  "enabled": true
}
```

---

# 27. Safety Telemetry

Example:

```json
{
  "message_type": "TELEMETRY",
  "telemetry_type": "SAFETY",
  "safety_state": "CLEAR",
  "estop": false,
  "heartbeat_ok": true
}
```

If E-stop becomes active:

```json
{
  "message_type": "EVENT",
  "event_type": "SAFETY_TRIGGERED",
  "reason": "ESTOP_ACTIVE"
}
```

Safety events should be generated immediately rather than waiting for the next ordinary telemetry cycle.

---

# 28. Ultrasonic Telemetry

Example:

```json
{
  "message_type": "TELEMETRY",
  "telemetry_type": "ULTRASONIC",
  "sensors": [
    {
      "id": 1,
      "distance_mm": 850,
      "status": "OK"
    }
  ]
}
```

Sensor failures must be represented explicitly.

Example:

```json
{
  "id": 4,
  "distance_mm": null,
  "status": "TIMEOUT"
}
```

---

# 29. Encoder Telemetry

Example:

```json
{
  "message_type": "TELEMETRY",
  "telemetry_type": "ENCODER",
  "left": {
    "count": 10500,
    "direction": "FORWARD"
  },
  "right": {
    "count": 10498,
    "direction": "FORWARD"
  }
}
```

The protocol should allow future addition of:

```text
pulse_rate
wheel_speed
distance
```

without breaking the V1 message interpretation.

---

# 30. IMU Telemetry

Example:

```json
{
  "message_type": "TELEMETRY",
  "telemetry_type": "IMU",
  "accel": {
    "x": 0.01,
    "y": 0.02,
    "z": 0.98
  },
  "gyro": {
    "x": 0.10,
    "y": 0.03,
    "z": 0.01
  }
}
```

Orientation fields should only be added when an orientation-estimation implementation exists.

---

# 31. Event Messages

Events represent state changes or important asynchronous conditions.

Examples:

```text
STATE_CHANGED
SAFETY_TRIGGERED
SAFETY_CLEARED
FAULT_DETECTED
FAULT_CLEARED
ESTOP_ASSERTED
ESTOP_RELEASED
HEARTBEAT_TIMEOUT
SENSOR_FAILURE
SELF_TEST_COMPLETE
```

Example:

```json
{
  "message_type": "EVENT",
  "event_type": "STATE_CHANGED",
  "previous_state": "READY",
  "new_state": "SAFE_STOP",
  "reason": "HEARTBEAT_TIMEOUT"
}
```

---

# 32. Heartbeat

The Raspberry Pi should periodically transmit heartbeat messages.

```text
Raspberry Pi
      │
      │ HEARTBEAT
      ▼
    ESP32
      │
      │ HEARTBEAT_ACK
      ▼
Raspberry Pi
```

Example:

```json
{
  "version": 1,
  "message_type": "HEARTBEAT",
  "message_id": 2001,
  "timestamp": 50000,
  "payload": {
    "sequence": 182
  }
}
```

ESP32 response:

```json
{
  "version": 1,
  "message_type": "HEARTBEAT_ACK",
  "message_id": 2001,
  "timestamp": 50001,
  "payload": {
    "sequence": 182,
    "system_state": "READY",
    "safety_state": "CLEAR"
  }
}
```

The ESP32 must monitor heartbeat freshness independently of normal telemetry.

---

# 33. Heartbeat Timeout

If the configured timeout expires:

```text
No valid heartbeat
        │
        ▼
Heartbeat Monitor
        │
        ▼
Safety Manager
        │
        ▼
SAFETY = SAFE
        │
        ▼
SAFE_STOP
```

Recovery:

```text
Heartbeat restored
        │
        ▼
Safety condition evaluated
        │
        ▼
Do NOT resume previous operation
        │
        ▼
Require explicit recovery / ENABLE
```

The actual heartbeat interval and timeout are configuration parameters and must be experimentally validated.

---

# 34. Diagnostics

The protocol should support diagnostic messages.

Example:

```json
{
  "message_type": "DIAGNOSTIC",
  "level": "INFO",
  "module": "ULTRASONIC",
  "message": "Sensor initialized",
  "timestamp": 1200
}
```

Diagnostic levels:

```text
DEBUG
INFO
WARNING
ERROR
CRITICAL
```

Production logging frequency should be configurable.

---

# 35. Fault Reporting

Fault events should include:

```text
fault_code
severity
source
timestamp
active
```

Example:

```json
{
  "message_type": "EVENT",
  "event_type": "FAULT_DETECTED",
  "fault": {
    "code": "IMU_FAILURE",
    "severity": "CRITICAL",
    "source": "IMU",
    "active": true
  }
}
```

---

# 36. Command Priority

Commands should not all have identical priority.

Conceptual priority:

```text
Highest
  │
  ├── Safety events
  ├── STOP
  ├── DISABLE
  ├── Fault handling
  ├── Normal commands
  └── Diagnostics
Lowest
```

Safety events must not be blocked indefinitely by telemetry or diagnostic processing.

---

# 37. Command Queue

A command queue may be used for normal commands.

```text
Transport
    │
    ▼
Parser
    │
    ▼
Command Queue
    │
    ▼
Command Handler
```

However:

- Safety inputs must not depend on the normal command queue.
- E-stop monitoring must operate independently.
- Watchdog monitoring must operate independently.
- Heartbeat monitoring must have bounded processing latency.
- `STOP` must receive appropriate priority.

---

# 38. Invalid Message Handling

The ESP32 must reject:

- Unsupported protocol versions
- Invalid message types
- Malformed payloads
- Missing required fields
- Invalid enum values
- Invalid command parameters
- Oversized messages
- Invalid framing/checksum data when the transport uses integrity checks

The parser must fail safely.

Malformed input must not crash the firmware.

---

# 39. Message Size

The protocol implementation must define a maximum accepted message size.

Example configuration:

```cpp
constexpr size_t MAX_MESSAGE_SIZE = ...;
```

The actual value should be chosen based on the final transport and implementation.

Messages exceeding the configured limit must be rejected before payload processing.

---

# 40. Transport Framing

The application protocol should not assume that one transport read equals one message.

For serial communication:

```text
Raw byte stream
      │
      ▼
Framing layer
      │
      ▼
Complete message
      │
      ▼
Protocol parser
```

The framing implementation must handle:

- Partial messages
- Multiple messages in one read
- Corrupted data
- Unexpected bytes
- Message boundaries

---

# 41. Integrity Protection

The transport layer should provide an integrity mechanism appropriate to the selected transport.

For a serial binary implementation, a checksum or CRC should be used.

Conceptually:

```text
HEADER
VERSION
MESSAGE TYPE
MESSAGE ID
PAYLOAD LENGTH
PAYLOAD
CRC
```

The exact CRC algorithm should be fixed when the transport implementation is selected.

The protocol must not claim CRC protection until it is implemented and tested.

---

# 42. Protocol Layering

Final conceptual stack:

```text
┌──────────────────────────────────┐
│ AutoChair Application Protocol   │
├──────────────────────────────────┤
│ Commands / Responses / Events    │
├──────────────────────────────────┤
│ Telemetry / Heartbeat            │
├──────────────────────────────────┤
│ Message Validation               │
├──────────────────────────────────┤
│ Framing / Integrity               │
├──────────────────────────────────┤
│ Transport                        │
├──────────────────────────────────┤
│ UART / USB Serial / Future       │
└──────────────────────────────────┘
```

---

# 43. No Direct Motor Command in V1

The V1 protocol deliberately does **not** define a physical motor command such as:

```text
SET_MOTOR_PWM
SET_MOTOR_SPEED
SET_MOTOR_DIRECTION
```

This is intentional.

The existing wheelchair's:

- Motor controller
- Brake interface
- Joystick/controller interface
- Electrical control signals
- Enable mechanism
- Safe stopping mechanism

must be verified before such commands can be defined.

---

# 44. Future Wheelchair Interface

A future protocol extension may introduce an abstract movement command.

Conceptually:

```text
SET_MOTION
    │
    ├── direction
    ├── speed
    └── duration / control mode
```

However this is **not part of V1 implementation**.

When the wheelchair interface is verified, the protocol must define:

- Command semantics
- Allowed operating states
- Speed limits
- Stop behavior
- Timeout behavior
- Fault behavior
- Controller acknowledgement
- Hardware feedback
- Emergency-stop interaction

before physical movement commands are enabled.

---

# 45. Version Compatibility

The ESP32 and Raspberry Pi must verify protocol compatibility during initialization.

Conceptually:

```text
Pi
 │
 │ Protocol Version
 ▼
ESP32
 │
 ├── Compatible → Continue
 │
 └── Incompatible → Reject operational enable
```

A communication link may remain available for diagnostics even when operational compatibility is unavailable.

---

# 46. Handshake

Initial connection:

```text
Raspberry Pi
     │
     │ HELLO / protocol information
     ▼
ESP32
     │
     │ CAPABILITIES / protocol information
     ▼
Raspberry Pi
```

The handshake should establish:

- Protocol version
- Firmware version
- Device identity
- Available sensors
- Supported commands
- Supported telemetry
- Current system state

Example capability response:

```json
{
  "protocol_version": 1,
  "firmware_version": "0.1.0",
  "device": "AUTOCHAIR_ESP32",
  "capabilities": {
    "ultrasonic": true,
    "imu": true,
    "encoder": true,
    "wheelchair_control": false
  }
}
```

`wheelchair_control: false` must remain true until the corresponding interface has actually been implemented and verified.

---

# 47. Security and Trust Boundary

The Raspberry Pi is a high-level command source.

The ESP32 is the final authority for whether a command is accepted within its configured safety and state constraints.

```text
Raspberry Pi
     │
     │ "ENABLE"
     ▼
ESP32
     │
     ├── State check
     ├── Safety check
     ├── Fault check
     └── Command validation
             │
       ┌─────┴─────┐
       ▼           ▼
     ACCEPT       REJECT
```

The Raspberry Pi cannot force the ESP32 into `READY` or `ACTIVE` through a protocol message.

---

# 48. Example Normal Session

```text
Pi → ESP32
HELLO

ESP32 → Pi
CAPABILITIES

Pi → ESP32
PING

ESP32 → Pi
PONG

Pi → ESP32
GET_STATUS

ESP32 → Pi
STATUS

Pi → ESP32
SET_MODE(MANUAL)

ESP32 → Pi
ACK

Pi → ESP32
ENABLE

ESP32
  ├── Safety check
  ├── State check
  └── Readiness check

ESP32 → Pi
ACK / READY

ESP32 → Pi
TELEMETRY

ESP32 → Pi
HEARTBEAT_ACK
```

---

# 49. Example Safety Event

```text
System:
READY
Safety:
CLEAR

        ↓

E-STOP ASSERTED

        ↓

Safety Manager

        ↓

Safety = SAFE

        ↓

State = SAFE_STOP

        ↓

ESP32 → Pi

EVENT:
ESTOP_ASSERTED

        ↓

Operational authorization revoked
```

The system must not automatically return to `READY` when the E-stop is released.

---

# 50. Example Communication Failure

```text
READY
  │
  ▼
Heartbeat messages received
  │
  │
  X
  │
Heartbeat timeout
  │
  ▼
Safety Manager
  │
  ▼
SAFE
  │
  ▼
SAFE_STOP
  │
  ▼
Event → HEARTBEAT_TIMEOUT
```

After communication returns:

```text
Heartbeat restored
       │
       ▼
Remain non-operational
       │
       ▼
Explicit recovery
       │
       ▼
IDLE
       │
       ▼
New ENABLE
```

No automatic operation resume is permitted.

---

# 51. Protocol Acceptance Criteria

The V1 protocol implementation is considered complete when:

- [ ] Protocol version is defined.
- [ ] Message categories are defined.
- [ ] Commands have unique identifiers.
- [ ] Commands receive explicit responses.
- [ ] Invalid commands return `NACK`.
- [ ] State validation occurs before command execution.
- [ ] Safety validation occurs before operational authorization.
- [ ] Telemetry can be transmitted.
- [ ] Asynchronous safety events can be transmitted.
- [ ] Heartbeat monitoring works.
- [ ] Heartbeat timeout produces the defined safety response.
- [ ] Communication recovery does not resume previous operation.
- [ ] Fault messages can be transmitted.
- [ ] Malformed messages are rejected safely.
- [ ] Message size limits are enforced.
- [ ] Transport framing handles partial messages.
- [ ] Integrity checking is implemented if required by the selected transport.
- [ ] Protocol version compatibility is checked.
- [ ] Simulation tests pass.
- [ ] No V1 protocol command directly controls wheelchair motors.

---

# 52. Implementation Sequence

The recommended implementation sequence is:

```text
1. Protocol data structures
          ↓
2. Message types
          ↓
3. Command IDs
          ↓
4. Response / NACK system
          ↓
5. Parser / validator
          ↓
6. Transport abstraction
          ↓
7. Framing
          ↓
8. Integrity checking
          ↓
9. PING / STATUS
          ↓
10. Sensor commands
          ↓
11. State commands
          ↓
12. Telemetry
          ↓
13. Events
          ↓
14. Heartbeat
          ↓
15. Fault reporting
          ↓
16. Simulation tests
          ↓
17. Raspberry Pi integration
```

---

# 53. V1 Boundary

The communication protocol is designed to support the complete AutoChair architecture while deliberately limiting what V1 can physically control.

### Implement now

```text
Pi
 │
 ├── Commands
 ├── Heartbeat
 └── High-level application
        │
        ▼
      ESP32
        │
        ├── Sensors
        ├── State machine
        ├── Safety
        ├── Diagnostics
        └── Telemetry
```

### Do not implement yet

```text
ESP32
   │
   ├── Direct motor PWM
   ├── Unverified controller commands
   ├── Brake actuation
   └── Joystick signal injection
```

Those capabilities require separate verified hardware-interface specifications and controlled testing.

---

# 54. Document Dependencies

This document depends on:

```text
ESP32_CONTROLLER_PRD.md
        │
        ▼
ESP32_ARCHITECTURE.md
        │
        ▼
SYSTEM_STATE_MACHINE.md
        │
        ▼
ESP32_PI_PROTOCOL.md
```

The next implementation-level document should define the **safety subsystem** in detail:

```text
SAFETY_ARCHITECTURE.md
```

That document should connect the state machine and communication protocol to the actual safety inputs, heartbeat supervision, fault manager, E-stop monitoring, watchdog, sensor-health rules, and SAFE/FAULT behavior—without assuming an unverified wheelchair braking interface.