# `SAFETY_ARCHITECTURE.md`

**Project:** AutoChair – AI-Powered Smart Wheelchair  
**Company:** Aivon Innovations Pvt. Ltd.  
**Platform:** ESP32-WROOM-32D + Raspberry Pi 4  
**Version:** V1.0  
**Status:** Development Architecture

---

## 1. Purpose

The Safety Architecture defines how AutoChair detects safety-related conditions, evaluates them, and places the ESP32 system into a safe software state.

Safety operates independently of the normal application and movement logic.

```text
Sensors / Inputs
      │
      ▼
Safety Monitoring
      │
      ▼
Safety Manager
      │
      ▼
Safety Decision
   ┌──┴──┐
 CLEAR  SAFE
```

**Important:** `SAFE` is an ESP32 software state. It does not by itself prove that the physical wheelchair has stopped. Physical braking/stop behavior depends on the verified wheelchair controller and brake architecture.

---

# 2. Safety Principles

1. Safety conditions have priority over normal commands.
2. Safety faults must not be silently ignored.
3. Loss of communication must not automatically resume operation.
4. E-stop release must not automatically re-enable the system.
5. Fault recovery must be deliberate.
6. The ESP32 must not assume an unverified motor-controller or brake interface.
7. Safety monitoring must continue independently of high-level Raspberry Pi logic.
8. Safety-critical behavior must be experimentally validated before being treated as implemented.

---

# 3. Safety Architecture

```text
                         ┌─────────────────┐
                         │  Raspberry Pi   │
                         │                 │
                         │ Commands        │
                         │ Heartbeat       │
                         └────────┬────────┘
                                  │
                                  ▼
                         ┌─────────────────┐
                         │ Communication   │
                         │ Monitor         │
                         └────────┬────────┘
                                  │
                                  │
 ┌──────────────┐                 │
 │ E-STOP       │─────────────────┤
 └──────────────┘                 │
                                  │
 ┌──────────────┐                 │
 │ Sensors      │─────────────────┤
 └──────────────┘                 │
                                  ▼
                         ┌─────────────────┐
                         │ Safety Manager  │
                         └────────┬────────┘
                                  │
                    ┌─────────────┴─────────────┐
                    │                           │
                  CLEAR                        SAFE
                    │                           │
                    ▼                           ▼
             Normal operation            SAFE_STOP / FAULT
```

---

# 4. Safety Manager

The Safety Manager is the central software component responsible for evaluating safety conditions.

### Inputs

- Emergency-stop status
- Raspberry Pi heartbeat
- Communication health
- Sensor health
- System state
- Active faults
- Watchdog status
- Command validity
- Required subsystem readiness

### Outputs

- `CLEAR`
- `SAFE`
- Safety events
- Fault requests
- Operational authorization decision

---

# 5. Safety State

```cpp
enum class SafetyState {
    CLEAR,
    SAFE
};
```

### `CLEAR`

All currently configured safety conditions are satisfied.

This does **not** mean that the wheelchair is physically safe or that physical movement is authorized.

### `SAFE`

At least one safety condition prevents normal operation.

The system must revoke operational authorization and enter the appropriate safe state.

---

# 6. Safety Inputs

## 6.1 Emergency Stop

The E-stop is a high-priority safety input.

```text
E-STOP
   │
   ▼
E-Stop Monitor
   │
   ▼
Safety Manager
```

When asserted:

```text
E-stop = ACTIVE
       ↓
Safety = SAFE
       ↓
Operational authorization revoked
       ↓
SAFE_STOP
```

The software must not assume that the ESP32's E-stop GPIO physically disconnects motor power.

The physical E-stop circuit must be independently verified.

---

## 6.2 Raspberry Pi Heartbeat

The ESP32 monitors the Raspberry Pi heartbeat.

```text
Pi
 │
 │ HEARTBEAT
 ▼
ESP32
 │
 ▼
Heartbeat Monitor
 │
 ├── Valid → continue
 │
 └── Timeout → SAFE
```

A heartbeat timeout during an operational state must cause the ESP32 to revoke operational authorization.

Communication recovery must not automatically resume the previous operation.

---

## 6.3 Sensor Health

Safety monitoring may use sensor-health information from:

- Ultrasonic sensors
- IMU
- Wheel encoders

Each sensor should expose a health state.

```text
OK
WARNING
TIMEOUT
INVALID
DISCONNECTED
FAULT
```

A sensor failure is not automatically a system-wide critical fault.

Its severity depends on whether that sensor is required for the currently active function.

---

# 7. Safety Decision Logic

Conceptually:

```text
                Safety Manager
                      │
        ┌─────────────┼─────────────┐
        │             │             │
      E-stop      Heartbeat      Faults
        │             │             │
        └─────────────┼─────────────┘
                      ▼
                Evaluate Rules
                      │
              ┌───────┴───────┐
              │               │
            CLEAR            SAFE
              │               │
              ▼               ▼
        Continue         Revoke Enable
                              │
                              ▼
                         SAFE_STOP
```

---

# 8. Safety Priority

Safety conditions are evaluated according to severity.

```text
Highest
  │
  ├── Emergency Stop
  ├── Critical Fault
  ├── Verified communication timeout
  ├── Required safety subsystem failure
  ├── Required sensor failure
  └── Warnings
Lowest
```

The exact priority and severity of individual sensor failures must be configured according to the operation being performed.

---

# 9. Operational Authorization

The Safety Manager must provide a decision separate from the system state.

Conceptually:

```cpp
bool isOperationAllowed();
```

It should return `true` only when all required conditions are satisfied.

```text
Operation request
      │
      ▼
Safety Manager
      │
      ├── SAFE → Reject
      │
      ▼
State Manager
      │
      ├── Not allowed → Reject
      │
      ▼
Required subsystem check
      │
      ▼
Accept
```

---

# 10. E-Stop Latching

An E-stop event should be treated as latched until the required recovery conditions are satisfied.

```text
E-stop asserted
      │
      ▼
SAFE_STOP
      │
      │ E-stop released
      ▼
Remain SAFE_STOP
      │
      │ Explicit recovery
      ▼
Safety checks
      │
      ▼
IDLE
```

The system must never do:

```text
E-stop released
      ↓
Automatically READY
```

---

# 11. Heartbeat Safety

The ESP32 must maintain a heartbeat timestamp.

```text
last_heartbeat = current_time
```

Conceptually:

```text
if current_time - last_heartbeat > timeout:
    safety_state = SAFE
```

The actual timeout value must be configurable and validated experimentally.

---

# 12. Fault Manager

The Safety Manager receives fault information from the Fault Manager.

Example fault classes:

```cpp
enum class FaultCode {
    NONE,
    SENSOR_TIMEOUT,
    SENSOR_INVALID,
    IMU_FAILURE,
    ENCODER_FAILURE,
    ESTOP_ACTIVE,
    HEARTBEAT_TIMEOUT,
    COMMUNICATION_FAILURE,
    WATCHDOG_FAILURE,
    INTERNAL_FAILURE
};
```

Faults should contain:

```text
Code
Severity
Source
Timestamp
Active / Cleared
Acknowledged
```

---

# 13. Fault Severity

```text
INFO
  │
  ▼
WARNING
  │
  ▼
SAFETY_RESTRICTION
  │
  ▼
CRITICAL / BLOCKING
```

Examples:

### Warning

A non-required sensor reports an intermittent invalid reading.

### Safety restriction

Heartbeat timeout while operational authorization is active.

### Critical fault

A required subsystem fails in a way that prevents safe operation.

The classification must be configurable rather than hard-coded purely by sensor type.

---

# 14. Watchdog

The watchdog protects against firmware execution failures.

```text
Firmware
   │
   ├── Healthy → watchdog serviced
   │
   └── Hung → watchdog timeout
                    │
                    ▼
                  Reset
```

The watchdog is a recovery mechanism.

It is **not** a replacement for the Safety Manager, E-stop, or physical safety systems.

After a watchdog reset:

```text
BOOT
  ↓
INITIALIZING
  ↓
SELF_TEST
  ↓
IDLE
```

The system must not restore previous operational authorization.

---

# 15. Safe-State Behavior

When the ESP32 enters `SAFE_STOP`:

1. Revoke operational authorization.
2. Cancel active operational commands.
3. Reject new operational commands.
4. Record the reason.
5. Publish a safety event.
6. Continue essential monitoring.
7. Continue diagnostics.
8. Maintain communication where possible.
9. Request physical stop behavior only through a verified interface.

---

# 16. Safety and Raspberry Pi

The Raspberry Pi is not the final authority for ESP32 safety decisions.

Example:

```text
Pi:
"ENABLE"

        ↓

ESP32:
Safety Check

        ├── CLEAR → may accept
        │
        └── SAFE → reject
```

Therefore a malfunctioning or incorrectly programmed Raspberry Pi cannot directly force the ESP32 into an operational state that violates its safety rules.

---

# 17. Safety and Sensor Data

Raw sensor values must be validated before being used for safety decisions.

```text
Raw measurement
      │
      ▼
Range validation
      │
      ▼
Timeout validation
      │
      ▼
Sensor health
      │
      ▼
Safety Manager
```

For example, an ultrasonic timeout must not be treated as a valid `0 mm` obstacle measurement.

---

# 18. Obstacle Detection Boundary

The ESP32 can provide reliable sensor measurements to the higher-level system.

```text
Ultrasonic
     │
     ▼
ESP32
     │
     ▼
Validated distance
     │
     ▼
Raspberry Pi
```

Whether a particular distance constitutes an obstacle requiring movement restriction is an application/safety policy that must be explicitly defined and tested.

Ultrasonic sensing alone must not be represented as complete indoor navigation or mapping.

---

# 19. Safety Event Reporting

Important safety changes must generate asynchronous events.

Example:

```json
{
  "event_type": "SAFETY_TRIGGERED",
  "reason": "ESTOP_ACTIVE"
}
```

Other events:

```text
ESTOP_ASSERTED
ESTOP_RELEASED
HEARTBEAT_TIMEOUT
HEARTBEAT_RESTORED
FAULT_DETECTED
FAULT_CLEARED
SAFETY_TRIGGERED
SAFETY_CLEARED
```

---

# 20. Recovery

Recovery must be explicit.

### From `SAFE_STOP`

```text
Safety condition active
        ↓
SAFE_STOP
        ↓
Condition cleared
        ↓
Required acknowledgement
        ↓
Safety checks
        ↓
IDLE
```

### From `FAULT`

```text
FAULT
  ↓
Underlying fault checked
  ↓
Fault recoverable?
  ├── NO → remain FAULT
  │
  └── YES
       ↓
   RESET_FAULT
       ↓
   Self-check
       ↓
      IDLE
```

No recovery path should directly return to `ACTIVE`.

---

# 21. Safety Invariants

The following must always remain true:

### Invariant 1

```text
Safety = SAFE
→ operational authorization = false
```

### Invariant 2

A blocking fault prevents `READY` and `ACTIVE`.

### Invariant 3

E-stop release does not automatically enable operation.

### Invariant 4

Communication recovery does not automatically resume operation.

### Invariant 5

A system restart never restores previous enable authorization.

### Invariant 6

The Raspberry Pi cannot override an ESP32 safety restriction through a normal command.

### Invariant 7

An unverified wheelchair interface cannot execute physical movement commands.

---

# 22. Physical Safety Boundary

The current software architecture does **not** assume:

- A specific motor-controller interface
- A specific electronic brake-control signal
- A specific motor PWM interface
- A specific controller enable signal
- That ESP32 can electrically disconnect motor power
- That software `STOP` equals physical braking

These must be verified against the actual wheelchair hardware.

---

# 23. Safety Testing

Testing must progress from simulation to hardware.

```text
Simulation
    ↓
Bench Testing
    ↓
Sensor Testing
    ↓
Safety Input Testing
    ↓
Communication Failure Testing
    ↓
Fault Injection
    ↓
Verified Wheelchair Interface
    ↓
Controlled Movement Testing
```

No physical movement test should be performed solely because the software state machine reports `READY`.

---

# 24. Required Safety Tests

The following tests must eventually be implemented:

- [ ] E-stop assertion
- [ ] E-stop release
- [ ] E-stop recovery
- [ ] Heartbeat loss
- [ ] Heartbeat restoration
- [ ] Sensor timeout
- [ ] Invalid sensor data
- [ ] IMU failure
- [ ] Encoder failure
- [ ] Communication failure
- [ ] Watchdog reset
- [ ] Blocking fault
- [ ] Fault recovery
- [ ] Invalid command during `SAFE_STOP`
- [ ] Enable request while unsafe
- [ ] Restart while previously enabled
- [ ] Recovery without automatic resume

These are acceptance tests, not claims that they have already passed.

---

# 25. V1 Safety Architecture

```text
                         RASPBERRY PI
                              │
                         HEARTBEAT
                              │
                              ▼
                       ┌─────────────┐
                       │ COMM MONITOR│
                       └──────┬──────┘
                              │
                              ▼
 ┌─────────────┐       ┌─────────────┐
 │ E-STOP      │──────►│             │
 └─────────────┘       │    SAFETY   │
                       │   MANAGER   │
 ┌─────────────┐       │             │
 │ SENSOR      │──────►│             │
 │ HEALTH      │       └──────┬──────┘
 └─────────────┘              │
                              ▼
                       ┌─────────────┐
                       │   SAFETY    │
                       │  DECISION   │
                       └──────┬──────┘
                              │
                     ┌────────┴────────┐
                     ▼                 ▼
                   CLEAR              SAFE
                     │                 │
                     ▼                 ▼
                 Continue          SAFE_STOP
                                       │
                                       ▼
                                  Fault / Recovery
```

---

# 26. Acceptance Criteria

The V1 Safety Architecture is ready for implementation when:

- [ ] Safety Manager is independent of application logic.
- [ ] E-stop monitoring is defined.
- [ ] Heartbeat monitoring is defined.
- [ ] Sensor-health monitoring is defined.
- [ ] Fault classification is defined.
- [ ] Watchdog behavior is defined.
- [ ] SAFE state behavior is defined.
- [ ] Recovery rules are defined.
- [ ] Safety events are defined.
- [ ] Safety invariants are testable.
- [ ] Simulation tests are defined.
- [ ] No unverified physical braking or motor-control behavior is assumed.

---

# 27. Implementation Order

```text
Safety data structures
        ↓
Safety Manager
        ↓
E-Stop Monitor
        ↓
Fault Manager
        ↓
Heartbeat Monitor
        ↓
Watchdog
        ↓
State Machine integration
        ↓
Safety event reporting
        ↓
Simulation tests
        ↓
Bench tests
        ↓
Hardware validation
```

---

## 28. V1 Safety Boundary

The goal of V1 is to build a **safe embedded software foundation**.

It is not yet a validated wheelchair safety system.

The ESP32 safety architecture must therefore be developed so that verified physical safety mechanisms can be integrated later without rewriting the core Safety Manager.