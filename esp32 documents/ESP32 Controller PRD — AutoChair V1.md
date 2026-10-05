# ESP32_CONTROLLER_PRD

**Project:** Aivon Innovations — AutoChair  
**Document:** ESP32 Controller Product Requirements Document  
**Version:** V1.0  
**Status:** IN DEVELOPMENT  
**Primary Controller:** ESP32-WROOM-32D DevKit  
**High-Level Computer:** Raspberry Pi 4  
**Development Environment:** Antigravity + Git  
**Safety Classification:** Prototype / Engineering Development

---

# 1. Purpose

The ESP32 Controller is the real-time embedded hardware layer of the AutoChair prototype.

Its purpose is to provide a reliable interface between physical sensors, safety inputs, system-state logic and the Raspberry Pi high-level computing system.

The ESP32 shall be developed independently of the final wheelchair motor-control interface so that software development can continue while the existing wheelchair electrical architecture is being inspected and verified.

The ESP32 is **not initially responsible for directly driving the wheelchair motors**.

---

# 2. AutoChair System Context

AutoChair is being developed as an intelligent powered wheelchair platform combining:

- Multimodal user control
- Embedded computing
- Sensor perception
- Intelligent navigation
- Obstacle detection
- Wheel-motion feedback
- Safety systems
- High-level navigation software

The intended architecture is:

```text
                    USER
                      │
          ┌───────────┼───────────┐
          │           │           │
       Joystick   Touchscreen   Voice
          │           │           │
          └───────────┼───────────┘
                      │
                RASPBERRY PI
             High-Level Computing
                      │
             ┌────────┴────────┐
             │                 │
        Navigation          User Interface
        Route Logic         Voice / Touch
             │
             │
        ESP32 CONTROLLER
       Real-Time Hardware Layer
             │
    ┌────────┼─────────┐
    │        │         │
Ultrasonic  IMU     Encoders
    │        │         │
    └────────┼─────────┘
             │
       Safety Monitoring
             │
       Hardware State
             │
       Diagnostics
```

The master AutoChair architecture defines Raspberry Pi + ESP32 as the onboard computing layer and places sensing, navigation/control and safety around it.

---

# 3. Development Classification

Every feature shall be classified as one of:

### CONFIRMED

Hardware/software that exists and has been physically demonstrated or verified.

### IN DEVELOPMENT

Engineering work currently being implemented.

### TARGET

An intended AutoChair capability that has not yet been demonstrated.

### FUTURE

Optional/later functionality outside the immediate prototype.

The project must never represent a TARGET feature as a working feature.

---

# 4. ESP32 Responsibilities

The ESP32 shall initially be responsible for:

1. Sensor acquisition
2. Sensor health monitoring
3. Real-time hardware monitoring
4. Encoder pulse acquisition
5. IMU data acquisition
6. Ultrasonic distance acquisition
7. Safety-input monitoring
8. Emergency-stop monitoring where electrically integrated
9. System state management
10. Fault detection
11. Watchdog/heartbeat functionality
12. Raspberry Pi communication
13. Command validation
14. Telemetry generation
15. Hardware diagnostics
16. Simulation/test mode
17. Future wheelchair-interface abstraction

The ESP32 should provide deterministic real-time behaviour for functions that should not depend on Raspberry Pi application-level timing.

---

# 5. Raspberry Pi Responsibilities

The Raspberry Pi shall remain responsible for high-level functions including:

1. Touchscreen interface
2. Voice processing
3. Camera processing
4. High-level navigation
5. Route planning
6. Route memory
7. Destination selection
8. High-level user commands
9. User-interface logic
10. High-level mobility decisions
11. Communication with the ESP32
12. High-level system monitoring

The Raspberry Pi should not need to directly manage every low-level sensor timing operation.

---

# 6. Current Purchased Hardware

The following hardware is available for prototype development:

### Processing

- Raspberry Pi 4 Model B 4GB ×1
- ESP32-WROOM-32D DevKit ×3

### Sensors

- HC-SR04 ultrasonic sensors ×8
- MPU6500 IMU ×1
- MPU6050 IMU ×3
- 600 PPR ABZ optical rotary encoders ×2

### Safety / Feedback

- Emergency-stop switches ×3
- Active buzzer modules ×3
- Red LED indicators
- Green LED indicators

### User Interface / Audio

- Waveshare 7-inch touchscreen
- DFRobot I2S microphone
- PAM8403 amplifier
- 8 Ω 3 W speakers ×2

### Camera

- Arducam 8MP camera ×1

The master hardware list identifies these components as purchased hardware available for beginning prototype work.

---

# 7. ESP32 Hardware Scope — V1

The first ESP32 prototype shall focus on:

```text
ESP32
 │
 ├── Ultrasonic sensors
 │
 ├── Wheel encoders
 │
 ├── IMU
 │
 ├── Safety inputs
 │
 ├── LEDs
 │
 ├── Buzzer
 │
 └── Raspberry Pi communication
```

The initial configuration should use the purchased hardware without unnecessarily adding new components.

The master context specifies an initial ultrasonic configuration of:

- Front ×3
- Side ×2
- Rear ×1

with two additional sensors available as spares.

---

# 8. Ultrasonic Subsystem

## 8.1 Hardware

HC-SR04 ×8 are available.

Initial prototype configuration:

```text
              FRONT

        [US1] [US2] [US3]


   [US4]                 [US5]
    LEFT                  RIGHT


              [US6]
               REAR
```

Two additional sensors remain spare.

## 8.2 ESP32 Responsibilities

The ESP32 shall:

- Trigger ultrasonic measurements
- Measure echo timing
- Convert measurements to distance
- Timestamp measurements
- Detect invalid measurements
- Detect timeout conditions
- Report sensor health
- Publish sensor data to Raspberry Pi

## 8.3 Safety Requirement

A missing or invalid ultrasonic measurement shall **not automatically be interpreted as "no obstacle."**

The system shall distinguish:

```text
VALID
INVALID
TIMEOUT
OUT_OF_RANGE
NOT_INITIALIZED
```

---

# 9. IMU Subsystem

Available:

- MPU6500 ×1
- MPU6050 ×3

The final architecture shall not require all four IMUs.

The first implementation should use **one verified IMU**.

The ESP32 shall provide:

- Accelerometer data
- Gyroscope data
- Sensor timestamps
- Sensor health
- Raw sensor values
- Basic orientation-related data where implemented

The first implementation should preserve raw measurements before introducing advanced filtering.

---

# 10. Wheel Encoder Subsystem

Two 600 PPR ABZ optical rotary encoders are available.

Intended use:

```text
LEFT ENCODER  → ESP32
RIGHT ENCODER → ESP32
```

The ESP32 shall acquire:

- Pulse counts
- A/B phase relationship
- Direction
- Pulse frequency
- Timestamp
- Encoder health

The system should expose:

```text
left_count
right_count

left_direction
right_direction

left_rate
right_rate
```

Wheel distance and odometry calculations shall initially be treated as software-level calculations and shall use verified wheel/encoder geometry.

---

# 11. Safety Subsystem

Safety is a first-class subsystem.

The ESP32 shall monitor safety-related inputs and system faults independently of high-level navigation.

Conceptually:

```text
Sensors
   │
   ▼
Hardware State
   │
   ▼
Safety Manager
   │
   ├── CLEAR
   │
   └── SAFE
          │
          ▼
    Command Permission
```

The Safety Manager shall consider:

- Emergency-stop state
- Sensor faults
- Communication loss
- Invalid commands
- System initialization
- Watchdog faults
- Critical hardware faults
- Obstacle information where configured

---

# 12. Emergency Stop

Emergency-stop hardware exists in the purchased inventory.

However, the final electrical implementation of the emergency-stop system must be verified before treating it as a complete wheelchair safety circuit.

The ESP32 software may monitor an emergency-stop input.

Conceptually:

```text
E-STOP ACTIVE
      │
      ▼
  SAFETY FAULT
      │
      ▼
 MOVEMENT PERMISSION = FALSE
```

The ESP32 must not assume that software alone provides physical emergency isolation.

The physical emergency-stop architecture must be validated separately.

---

# 13. System States

The ESP32 shall use an explicit state machine.

Initial state model:

```text
BOOT
  │
  ▼
INITIALIZING
  │
  ├──── failure ────► FAULT
  │
  ▼
SELF_TEST
  │
  ├──── failure ────► FAULT
  │
  ▼
IDLE
  │
  ▼
READY
  │
  ├───────────────┐
  │               │
  ▼               ▼
ASSISTED       MANUAL
  │               │
  └───────┬───────┘
          │
          ▼
       SAFETY
          │
     ┌────┴────┐
     │         │
   CLEAR      SAFE
     │         │
     ▼         ▼
 continue    stop/block
```

Additional states may be introduced during implementation if required.

---

# 14. Minimum State Definitions

## BOOT

ESP32 has started execution.

No external movement command is accepted.

## INITIALIZING

Initialize:

- GPIO
- Sensors
- Timers
- Communication
- Watchdog
- Internal state

## SELF_TEST

Verify required subsystems.

Example:

```text
Ultrasonic → CHECK
IMU        → CHECK
Encoder    → CHECK
Safety     → CHECK
Pi Link    → CHECK
```

## IDLE

System is initialized but not actively controlling mobility.

## READY

All required conditions for the current operating mode are satisfied.

## MANUAL

Reserved for manual-control-related integration.

Actual wheelchair movement control remains outside V1 until the wheelchair controller interface is verified.

## ASSISTED

High-level assisted-control mode.

The ESP32 provides sensor/safety information and validates hardware-level conditions.

## SAFE

Movement-related commands are blocked or the system enters its configured safe condition.

## FAULT

A critical software/hardware fault has occurred.

---

# 15. Command Architecture

Commands originate from the Raspberry Pi.

Example:

```text
Raspberry Pi
     │
     │ command
     ▼
ESP32
     │
     ▼
Command Validator
     │
     ▼
Safety Manager
     │
     ├── ACCEPT
     │
     └── REJECT
```

The ESP32 must never blindly execute a command received from the Raspberry Pi.

---

# 16. Initial Command Types

Initial protocol commands may include:

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

Movement commands shall remain abstract initially:

```text
MOVE_REQUEST
```

rather than directly implementing:

```text
MOTOR_LEFT_PWM
MOTOR_RIGHT_PWM
```

until the actual wheelchair controller interface has been verified.

---

# 17. Raspberry Pi ↔ ESP32 Communication

A formal communication protocol shall be created before high-level integration.

The protocol shall eventually support:

### Raspberry Pi → ESP32

```text
COMMAND
MODE
CONFIGURATION
RESET
HEARTBEAT
```

### ESP32 → Raspberry Pi

```text
STATUS
SENSOR_DATA
ENCODER_DATA
IMU_DATA
SAFETY_STATUS
FAULT
HEARTBEAT
ACK
NACK
```

---

# 18. Heartbeat

Both systems should maintain communication health.

Conceptually:

```text
Raspberry Pi ── HEARTBEAT ──► ESP32
Raspberry Pi ◄─ HEARTBEAT ─── ESP32
```

If the ESP32 detects loss of communication beyond the configured timeout:

```text
COMMUNICATION LOST
       │
       ▼
SAFETY CONDITION
       │
       ▼
BLOCK UNSAFE COMMANDS
```

The exact timeout shall be defined during implementation/testing rather than arbitrarily assumed.

---

# 19. Sensor Data Model

All sensor messages should contain enough information for debugging.

Example:

```json
{
  "sensor": "ultrasonic_front_center",
  "distance_mm": 1240,
  "status": "VALID",
  "timestamp_ms": 123456
}
```

Encoder example:

```json
{
  "left_count": 10240,
  "right_count": 10231,
  "left_direction": "FORWARD",
  "right_direction": "FORWARD",
  "timestamp_ms": 123456
}
```

IMU example:

```json
{
  "ax": 0.01,
  "ay": 0.02,
  "az": 0.98,
  "gx": 0.01,
  "gy": 0.02,
  "gz": 0.00,
  "status": "VALID",
  "timestamp_ms": 123456
}
```

The exact transport encoding will be specified separately in:

`ESP32_PI_PROTOCOL.md`

---

# 20. Diagnostics

The ESP32 shall provide diagnostic information for:

- Boot status
- Sensor initialization
- Sensor failures
- Encoder failures
- IMU failures
- Communication status
- Safety status
- Current system state
- Fault codes
- Firmware version

Example:

```text
ESP32 STATUS

Firmware: 0.1.0
State: READY

Ultrasonic: 6/6 OK
Encoder L: OK
Encoder R: OK
IMU: OK
Safety: CLEAR
Pi Link: CONNECTED
Fault: NONE
```

---

# 21. Logging

The firmware should produce structured logs during development.

Minimum levels:

```text
DEBUG
INFO
WARNING
ERROR
FAULT
```

Example:

```text
[INFO] ESP32 boot
[INFO] Ultrasonic subsystem initialized
[INFO] Encoder subsystem initialized
[INFO] IMU initialized
[INFO] Raspberry Pi connected
[INFO] System READY
```

Safety-critical faults must be clearly distinguishable from normal informational logs.

---

# 22. Watchdog

The ESP32 firmware shall use a watchdog mechanism appropriate to the selected ESP32 framework.

The watchdog is intended to detect software execution failures.

A watchdog reset shall not be treated as a normal event.

The system should record:

```text
WATCHDOG_RESET
```

and expose this through diagnostics after reboot where practical.

---

# 23. Simulation Mode

This is a critical requirement for parallel development.

The ESP32 software shall support development/testing without connecting to the actual wheelchair movement system.

Example:

```text
SIMULATION MODE
      │
      ├── simulated ultrasonic data
      ├── simulated encoder data
      ├── simulated IMU data
      ├── simulated safety events
      └── simulated Pi commands
```

This allows development to continue while wheelchair inspection is ongoing.

---

# 24. Hardware Abstraction

Hardware-specific implementation shall be separated from system logic.

Preferred structure:

```text
Application
    │
    ▼
System Manager
    │
    ├── Safety Manager
    ├── Sensor Manager
    ├── Command Manager
    └── Communication Manager
            │
            ▼
     Hardware Abstraction
            │
    ┌───────┼────────┐
    │       │        │
 Ultrasonic IMU   Encoder
```

This allows hardware drivers to change without rewriting the complete system.

---

# 25. Wheelchair Interface Abstraction

The existing wheelchair controller interface is **NOT yet fully specified**.

Therefore V1 shall create an abstraction layer:

```text
WheelchairInterface
```

rather than directly connecting the rest of the system to a guessed controller protocol.

Possible future interface:

```text
WheelchairInterface
 ├── stop()
 ├── enable()
 ├── disable()
 ├── set_direction()
 ├── set_speed()
 └── get_status()
```

These functions are architectural placeholders.

They must not be implemented against the physical wheelchair until the actual controller interface has been verified.

---

# 26. Existing Joystick Interface

The physical joystick investigation established:

```text
Pin 1 → 3V_393
Pin 2 → INT
Pin 3 → SCL
Pin 4 → SDA
Pin 5 → GND
Pin 6 → 16_DIO
Pin 7 → 16_CLK
Pin 8 → EMGC
Pin 9 → VCC
```

Measured logic levels include approximately:

```text
3.3 V logic
5 V supply
```

The joystick's SCL/SDA lines have confirmed I²C activity around:

```text
Address: 0x0C
Frequency: approximately 75 kHz
```

The Pin 6/7 interface produced repeatable synchronous data and was classified as **SPI-compatible / SPI-like**, rather than definitively standard SPI because no dedicated chip-select/frame delimiter was confirmed. 

The actual movement meaning of the I²C data has **not been reliably decoded**.

Therefore:

```text
Joystick movement-byte mapping = TBD
```

The ESP32 may investigate the interface later using passive observation.

It must not inject signals into the existing joystick/controller during this development stage.

---

# 27. Safety Rule — No Assumed Motor Control

The ESP32 shall NOT directly control the wheelchair motors in V1.

Reason:

The wheelchair already contains an existing controller/braking architecture.

The inspection established:

- Two 250 W DC motors
- 24 V motor rating
- Existing gearbox
- Existing electromagnetic braking system
- Existing joystick/controller assembly
- No Hall/encoder feedback built into the motors

The actual external controller interface still requires verification. 

Therefore:

```text
ESP32
  X
  │
  X── Direct motor drive — NOT V1
```

Instead:

```text
ESP32
  │
  ▼
Abstract Wheelchair Interface
  │
  ▼
TBD / VERIFIED CONTROLLER INTERFACE
```

---

# 28. Safety Philosophy

The fundamental AutoChair principle is:

> AutoChair should assist the user, not make the user completely dependent on automation.

Manual control must remain available in the final system.

The ESP32 safety architecture must therefore support:

```text
USER
 │
 ├── Manual control
 │
 └── Assisted control
          │
          ▼
     Safety Manager
          │
          ▼
    Safe system response
```

This follows the master AutoChair product philosophy.

---

# 29. Fault Categories

Initial fault categories:

```text
SENSOR_FAULT
IMU_FAULT
ENCODER_FAULT
ULTRASONIC_FAULT
COMMUNICATION_FAULT
WATCHDOG_FAULT
SAFETY_INPUT_FAULT
INVALID_COMMAND
SYSTEM_INIT_FAULT
UNKNOWN_FAULT
```

Every fault should have:

```text
fault_code
fault_source
timestamp
severity
current_state
```

---

# 30. Testing Requirements

Testing must happen in stages.

## Stage 1 — Software Only

Test:

- State machine
- Command parser
- Safety logic
- Fault handling
- Communication protocol
- Simulation mode

## Stage 2 — Individual Hardware

Test independently:

```text
ESP32
 ├── HC-SR04
 ├── IMU
 ├── Encoder
 ├── LED
 ├── Buzzer
 └── Safety input
```

## Stage 3 — Integrated ESP32

Test:

```text
Sensors
   ↓
ESP32
   ↓
State Manager
   ↓
Safety Manager
   ↓
Telemetry
```

## Stage 4 — Raspberry Pi Integration

Test:

```text
Raspberry Pi
      ↕
    ESP32
      ↕
   Sensors
```

## Stage 5 — Wheelchair Interface

Only after the existing wheelchair controller/interface has been verified.

---

# 31. Initial Acceptance Criteria

ESP32 V1 shall be considered functional when:

### Boot

- ESP32 boots reliably.
- Firmware version is reported.
- Initialization sequence executes correctly.

### Sensors

- Ultrasonic sensors provide validated measurements.
- IMU provides valid readings.
- Both encoders can be read.
- Sensor failures are detectable.

### State Machine

- BOOT works.
- INITIALIZING works.
- SELF_TEST works.
- IDLE works.
- READY works.
- FAULT works.
- SAFE works.

### Safety

- Safety inputs can be detected.
- Invalid sensor conditions can be identified.
- Communication loss can be detected.
- Unsafe commands are rejected.

### Communication

- Raspberry Pi can connect.
- Heartbeat works.
- Commands can be received.
- Telemetry can be transmitted.
- ACK/NACK behaviour works.

### Diagnostics

- Firmware version available.
- Current state available.
- Sensor status available.
- Fault status available.

### Simulation

- System can operate without wheelchair motor connection.
- Simulated sensor data can be injected.
- Safety behaviour can be tested safely.

---

# 32. Non-Goals for ESP32 V1

The following are explicitly **outside the first ESP32 implementation**:

- Direct motor PWM control
- Direct H-bridge control
- Replacing the wheelchair's existing controller
- Autonomous wheelchair movement
- Full autonomous navigation
- Route learning
- SLAM
- LiDAR integration
- GPS
- Caregiver application
- Production-grade safety certification
- Clinical validation

These may become later development targets.

---

# 33. Future Integration

After the physical wheelchair architecture is fully verified, the ESP32 layer may eventually integrate with:

```text
Existing wheelchair controller
        │
        ▼
Verified interface
        │
        ▼
ESP32
        │
        ▼
Safety Manager
        │
        ▼
Raspberry Pi
```

Before this stage, the following must be known:

- Controller manufacturer/model
- Controller voltage
- Controller current rating
- Brake interface
- Enable/disable mechanism
- External control interface
- Communication protocol, if any
- Electrical isolation requirements
- Safe stopping behaviour
- Emergency-stop architecture

The project master context explicitly identifies these as items that must be verified before autonomous movement.

---

# 34. Development Order

The implementation order for Antigravity shall be:

```text
PHASE 0
Project setup
        ↓
PHASE 1
Core firmware architecture
        ↓
PHASE 2
System state machine
        ↓
PHASE 3
Logging + diagnostics
        ↓
PHASE 4
Ultrasonic driver
        ↓
PHASE 5
IMU driver
        ↓
PHASE 6
Encoder driver
        ↓
PHASE 7
Safety Manager
        ↓
PHASE 8
Simulation framework
        ↓
PHASE 9
Raspberry Pi communication
        ↓
PHASE 10
Integrated sensor testing
        ↓
PHASE 11
ESP32 ↔ Raspberry Pi integration
        ↓
PHASE 12
Wheelchair interface abstraction
        ↓
PHASE 13
Verified wheelchair-controller integration
        ↓
PHASE 14
Controlled movement testing
```

**Do not skip directly to Phase 13.**

---

# 35. Recommended Initial Repository Structure

```text
AutoChair/
│
├── docs/
│   ├── prd/
│   │   └── ESP32_CONTROLLER_PRD.md
│   │
│   ├── architecture/
│   │   ├── ESP32_ARCHITECTURE.md
│   │   ├── SYSTEM_STATE_MACHINE.md
│   │   ├── ESP32_PI_PROTOCOL.md
│   │   └── SAFETY_ARCHITECTURE.md
│   │
│   └── hardware/
│       ├── SENSOR_INTERFACE.md
│       ├── IMU_INTERFACE.md
│       ├── ENCODER_INTERFACE.md
│       └── WHEELCHAIR_INTERFACE.md
│
├── firmware/
│   └── esp32/
│       ├── src/
│       ├── include/
│       ├── tests/
│       └── platformio.ini
│
├── raspberry_pi/
│
├── simulation/
│
└── README.md
```

The exact build system can be finalized before implementation; this PRD does not force an IDE/toolchain choice.

---

# 36. Engineering Rules

The ESP32 implementation shall follow these rules:

### Rule 1

Never assume an unverified wheelchair interface.

### Rule 2

Never directly control the wheelchair motors during early development.

### Rule 3

Every safety-critical behaviour must have a test.

### Rule 4

Sensor failure must be distinguishable from valid zero/no-obstacle data.

### Rule 5

Communication failure must have defined behaviour.

### Rule 6

Simulation must be possible without the wheelchair.

### Rule 7

Hardware drivers must be separated from application logic.

### Rule 8

No unnecessary components shall be purchased before checking whether existing hardware can perform the function.

### Rule 9

Purchased hardware ≠ integrated hardware.

### Rule 10

Integrated hardware ≠ validated functionality.

### Rule 11

A prototype demonstration must not be described as a validated product.

### Rule 12

Safety takes priority over feature completeness.

---

# 37. V1 Definition of Done

ESP32 Controller V1 is complete when the following architecture works independently of the wheelchair motors:

```text
             RASPBERRY PI
                  │
            Communication
                  │
                  ▼
              ESP32 V1
                  │
       ┌──────────┼──────────┐
       │          │          │
       ▼          ▼          ▼
 Ultrasonic      IMU      Encoders
       │          │          │
       └──────────┼──────────┘
                  │
                  ▼
             Sensor Manager
                  │
                  ▼
            Safety Manager
                  │
                  ▼
            System State
                  │
                  ▼
              Telemetry
                  │
                  ▼
             Raspberry Pi
```

The result should be a **real, testable embedded subsystem**, not merely a collection of sensor-reading sketches.

---

# 38. Current Status

| Component / Capability | Status |
|---|---|
| ESP32 hardware | **CONFIRMED** |
| Raspberry Pi hardware | **CONFIRMED** |
| Ultrasonic hardware | **CONFIRMED** |
| IMU hardware | **CONFIRMED** |
| Encoder hardware | **CONFIRMED** |
| Safety hardware available | **CONFIRMED** |
| ESP32 firmware architecture | **IN DEVELOPMENT** |
| Sensor drivers | **IN DEVELOPMENT** |
| Safety Manager | **IN DEVELOPMENT** |
| Pi ↔ ESP32 protocol | **IN DEVELOPMENT** |
| Simulation mode | **IN DEVELOPMENT** |
| Wheelchair controller interface | **TBD / VERIFICATION REQUIRED** |
| Joystick movement-byte mapping | **TBD** |
| Autonomous motor control | **TARGET** |
| Automatic braking through ESP32 | **TARGET / REQUIRES VALIDATION** |
| Autonomous indoor navigation | **TARGET** |
| Route memory | **TARGET** |
| LiDAR | **FUTURE / OPTIONAL** |
| Caregiver application | **FUTURE** |

---

# 39. Final Objective

The immediate objective is **not** to make the ESP32 drive the wheelchair.

The immediate objective is to create a robust real-time embedded controller that can:

```text
READ
  ↓
VALIDATE
  ↓
UNDERSTAND HARDWARE STATE
  ↓
MONITOR SAFETY
  ↓
COMMUNICATE
  ↓
REPORT
```

Then, once the actual wheelchair controller and brake interfaces are verified:

```text
READ
  ↓
PERCEIVE
  ↓
DECIDE
  ↓
SAFETY CHECK
  ↓
CONTROL
  ↓
VERIFY RESPONSE
```

This allows AutoChair software development to continue **now**, while keeping the unknown physical wheelchair interfaces isolated until they are properly verified.