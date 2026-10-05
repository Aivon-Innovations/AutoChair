# ESP32 Architecture — AutoChair V1

**Project:** AutoChair – AI-Powered Smart Wheelchair  
**Company:** Aivon Innovations Pvt. Ltd.  
**Platform:** ESP32-WROOM-32D  
**Document:** ESP32 Architecture  
**Version:** V1.0  
**Status:** Development Architecture

---

## 1. Purpose

The ESP32 is the **real-time hardware control and monitoring layer** of AutoChair.

Its primary responsibility is to interface with physical sensors and safety-related hardware, process real-time data, maintain the embedded system state, and communicate telemetry and validated commands with the Raspberry Pi.

The ESP32 should be designed as a **modular embedded platform**, not as a collection of independent sensor sketches.

### Core principle

> The ESP32 handles deterministic, real-time hardware operations.  
> The Raspberry Pi handles high-level computation, user interaction, AI, navigation, and application logic.

---

# 2. System Position

```text
                    USER
                     │
          ┌──────────┼──────────┐
          │          │          │
       Joystick   Touchscreen  Voice
          │          │          │
          └──────────┼──────────┘
                     │
                     ▼
              RASPBERRY PI 4
        ┌─────────────────────────┐
        │ UI / Voice / Camera     │
        │ High-Level Decisions    │
        │ Navigation              │
        │ Route Planning          │
        │ Application Logic       │
        └────────────┬────────────┘
                     │
              Pi ↔ ESP32 Protocol
                     │
                     ▼
              ┌───────────────┐
              │     ESP32     │
              │               │
              │ Real-Time HW  │
              │ Layer         │
              └───────┬───────┘
                      │
       ┌──────────────┼───────────────┐
       │              │               │
       ▼              ▼               ▼
   Ultrasonic        IMU           Encoders
   Sensors          MPU6500/       600 PPR
                    MPU6050
       │              │               │
       └──────────────┼───────────────┘
                      │
                      ▼
               Sensor Processing
                      │
                      ▼
                Safety Manager
                      │
              ┌───────┴────────┐
              │                │
            CLEAR             SAFE
              │                │
              └───────┬────────┘
                      │
                      ▼
              System State Machine
                      │
                      ▼
               Telemetry / Events
                      │
                      ▼
                Raspberry Pi
```

---

# 3. Architectural Responsibilities

## 3.1 ESP32 Responsibilities

The ESP32 is responsible for:

- Sensor acquisition
- Sensor health monitoring
- Ultrasonic distance measurement
- IMU data acquisition
- Wheel encoder pulse acquisition
- Emergency-stop monitoring
- Safety monitoring
- Real-time fault detection
- Embedded state management
- Command validation
- Watchdog handling
- Heartbeat monitoring
- Telemetry generation
- Diagnostics
- Communication with Raspberry Pi
- Hardware abstraction
- Future wheelchair-interface integration

---

## 3.2 Raspberry Pi Responsibilities

The Raspberry Pi is responsible for:

- Touchscreen interface
- Voice input and processing
- Audio management
- Camera processing
- High-level user interaction
- Navigation
- Route planning
- Route memory
- AI-related processing
- Operating-mode selection
- High-level command generation
- Application-level logging
- User feedback
- High-level system orchestration

The Raspberry Pi should **not depend on directly managing timing-critical sensor acquisition**.

---

# 4. Layered ESP32 Architecture

The ESP32 firmware should use the following layers:

```text
┌───────────────────────────────────────────┐
│          Application / Command Layer      │
│                                           │
│  Command Handler / Mode Manager           │
└──────────────────────┬────────────────────┘
                       │
┌──────────────────────▼────────────────────┐
│              Safety Layer                 │
│                                           │
│  Safety Manager / Fault Manager           │
└──────────────────────┬────────────────────┘
                       │
┌──────────────────────▼────────────────────┐
│              State Layer                  │
│                                           │
│  System State Machine                     │
└──────────────────────┬────────────────────┘
                       │
┌──────────────────────▼────────────────────┐
│             Processing Layer              │
│                                           │
│  Sensor Manager / Data Processing         │
└──────────────────────┬────────────────────┘
                       │
┌──────────────────────▼────────────────────┐
│           Hardware Abstraction            │
│                                           │
│  GPIO / I2C / Pulse Counter / UART/etc.   │
└──────────────────────┬────────────────────┘
                       │
┌──────────────────────▼────────────────────┐
│                Hardware                   │
│                                           │
│ Sensors / E-stop / LEDs / Buzzer / ESP32  │
└───────────────────────────────────────────┘
```

---

# 5. Core Firmware Modules

The firmware should be divided into independent modules.

```text
ESP32 Firmware
│
├── Core
│   ├── System
│   ├── State Machine
│   ├── Configuration
│   └── Scheduler
│
├── Sensors
│   ├── Ultrasonic
│   ├── IMU
│   └── Encoder
│
├── Safety
│   ├── Safety Manager
│   ├── E-Stop Monitor
│   ├── Fault Manager
│   └── Watchdog
│
├── Communication
│   ├── Pi Protocol
│   ├── Command Handler
│   ├── Telemetry
│   └── Heartbeat
│
├── Diagnostics
│   ├── Logger
│   ├── Health Monitor
│   └── Self Test
│
└── Hardware
    ├── GPIO
    ├── I2C
    ├── Encoder Interface
    └── Future Wheelchair Interface
```

---

# 6. Core System Module

The Core System module initializes and coordinates the firmware.

### Responsibilities

- ESP32 startup
- Configuration loading
- Hardware initialization
- Module initialization
- Startup diagnostics
- System state initialization
- Main scheduling
- Watchdog initialization

### Startup sequence

```text
BOOT
 │
 ▼
Load Configuration
 │
 ▼
Initialize Hardware
 │
 ▼
Initialize Sensors
 │
 ▼
Initialize Safety
 │
 ▼
Initialize Communication
 │
 ▼
Run Self-Test
 │
 ▼
SELF_TEST COMPLETE
 │
 ▼
IDLE
```

---

# 7. State Machine

The ESP32 uses a centralized state machine.

```text
                 ┌─────────────┐
                 │    BOOT     │
                 └──────┬──────┘
                        │
                        ▼
              ┌──────────────────┐
              │  INITIALIZING    │
              └────────┬─────────┘
                       │
                       ▼
              ┌──────────────────┐
              │    SELF_TEST     │
              └────────┬─────────┘
                       │
                 PASS  │
                       ▼
              ┌──────────────────┐
              │       IDLE       │
              └────────┬─────────┘
                       │
                       ▼
              ┌──────────────────┐
              │      READY       │
              └────────┬─────────┘
                       │
                ┌──────┴──────┐
                ▼             ▼
             MANUAL        ASSISTED
                │             │
                └──────┬──────┘
                       │
                       ▼
                 SAFETY CHECK
                  /        \
               CLEAR       SAFE
                 │           │
                 │           ▼
                 │        STOP/SAFE
                 │
                 ▼
              CONTINUE

Any critical fault
        │
        ▼
      FAULT
```

### Important

The state machine does **not** imply that the ESP32 currently drives the wheelchair motors.

Movement-control states are architectural placeholders until the actual wheelchair controller and safe interface are verified.

---

# 8. Sensor Architecture

The ESP32 acts as the real-time sensor acquisition layer.

```text
                 SENSOR MANAGER
                      │
       ┌──────────────┼──────────────┐
       │              │              │
       ▼              ▼              ▼
 Ultrasonic          IMU          Encoders
       │              │              │
       ▼              ▼              ▼
 Distance         Accel/Gyro     Pulse Count
       │              │              │
       └──────────────┼──────────────┘
                      ▼
                 Sensor Data
                      │
                      ▼
               Safety / Telemetry
```

---

# 9. Ultrasonic Subsystem

The project has eight HC-SR04 ultrasonic sensors available.

Initial prototype architecture:

```text
Front:
    US1     US2     US3

Left:  US4

Right: US5

Rear:  US6

US7 + US8 = Spare
```

The exact physical mounting should be finalized during hardware integration.

### Ultrasonic module responsibilities

- Trigger sensor
- Measure echo duration
- Convert measurement to distance
- Validate measurements
- Detect timeout
- Detect invalid readings
- Maintain sensor health
- Publish latest distance

Example internal representation:

```text
UltrasonicSensor
{
    id
    distance_mm
    timestamp
    status
}
```

Possible statuses:

```text
OK
TIMEOUT
INVALID
OUT_OF_RANGE
DISCONNECTED
```

---

# 10. IMU Subsystem

The available IMUs include MPU6500 and MPU6050 devices.

The architecture should support multiple IMU implementations through a common interface.

```text
             IMU Interface
                  │
          ┌───────┴────────┐
          │                │
       MPU6500          MPU6050
```

The first implementation should use **one verified IMU**.

### IMU outputs

```text
Acceleration
    X
    Y
    Z

Angular Velocity
    X
    Y
    Z
```

Optional derived values can later include:

- Orientation
- Tilt
- Angular displacement
- Motion detection

Sensor fusion should not be assumed until it is implemented and validated.

---

# 11. Encoder Subsystem

Two 600 PPR ABZ optical rotary encoders are available.

```text
Left Encoder                Right Encoder
     │                           │
     ▼                           ▼
 Channel A/B/Z               Channel A/B/Z
     │                           │
     └───────────┬───────────────┘
                 ▼
          Encoder Manager
                 │
        ┌────────┼────────┐
        ▼        ▼        ▼
      Count    Direction   Z
                 │
                 ▼
              Odometry
```

### Encoder module responsibilities

- Count pulses
- Determine direction
- Track left/right wheel counts
- Detect abnormal pulse behavior
- Provide timestamped measurements
- Provide data to odometry

Initial encoder output:

```text
left_count
right_count
left_direction
right_direction
timestamp
```

Wheel circumference and final encoder-to-distance conversion should use experimentally verified mechanical measurements.

---

# 12. Safety Architecture

Safety is an independent layer.

```text
             Safety Manager
                   ▲
        ┌──────────┼──────────┐
        │          │          │
    Heartbeat   Commands     Faults
        │          │          │
        └──────────┼──────────┘
                   │
             Safety Decision
                   │
             ┌─────┴─────┐
             │           │
           CLEAR        SAFE
```

### Safety inputs

- Emergency-stop status
- Communication heartbeat
- Sensor health
- Critical sensor faults
- Firmware faults
- Watchdog status
- Command validity
- System state

### Safety outputs

- Safety state
- Fault state
- Stop request
- Warning event
- Diagnostic event

---

# 13. Emergency Stop

The emergency-stop input must be treated as a safety-critical input.

Architecture:

```text
E-STOP
   │
   ▼
E-Stop Monitor
   │
   ▼
Safety Manager
   │
   ├── SAFE
   │
   └── Fault/Event
```

The software architecture must not claim that the ESP32 can physically remove motor power until the actual electrical safety circuit is verified.

The physical emergency-stop implementation and wheelchair controller behavior must be validated separately.

---

# 14. Fault Manager

All modules should be capable of reporting faults through a common mechanism.

Example:

```text
FAULT_SENSOR_TIMEOUT
FAULT_SENSOR_INVALID
FAULT_IMU_FAILURE
FAULT_ENCODER_FAILURE
FAULT_ESTOP
FAULT_HEARTBEAT_TIMEOUT
FAULT_COMMUNICATION
FAULT_WATCHDOG
FAULT_INTERNAL
```

Fault lifecycle:

```text
Detected
   │
   ▼
Recorded
   │
   ▼
Safety Evaluation
   │
   ├──── Non-critical ────► Warning
   │
   └──── Critical ────────► SAFE / FAULT
```

---

# 15. Communication Architecture

The Raspberry Pi and ESP32 communicate through a defined protocol.

```text
                RASPBERRY PI
                     │
                     │
              Command Messages
                     │
                     ▼
              ┌──────────────┐
              │ ESP32        │
              │ Communication│
              │ Manager      │
              └──────┬───────┘
                     │
              Command Handler
                     │
                     ▼
              Validation Layer
                     │
                     ▼
                Application
```

ESP32 → Raspberry Pi:

```text
Telemetry
Sensor data
Encoder data
IMU data
Safety state
Fault events
Health status
Heartbeat
Diagnostics
```

Raspberry Pi → ESP32:

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

Movement commands should remain abstract until the physical wheelchair interface is verified.

---

# 16. Heartbeat

A heartbeat mechanism should verify that the Raspberry Pi and ESP32 remain connected.

```text
Raspberry Pi
     │
     │ HEARTBEAT
     ▼
   ESP32
     │
     │ ACK
     ▼
Raspberry Pi
```

If the heartbeat is lost for a configured timeout:

```text
Heartbeat Lost
      │
      ▼
Safety Manager
      │
      ▼
SAFE / FAULT
```

The exact timeout should be configurable and validated during testing rather than arbitrarily treated as a final safety value.

---

# 17. Command Processing

Commands must pass through validation before reaching the relevant subsystem.

```text
Pi Command
    │
    ▼
Protocol Parser
    │
    ▼
Command Validator
    │
    ├── Invalid ──► NACK
    │
    ▼
State/Safety Validation
    │
    ├── Not Allowed ──► NACK
    │
    ▼
Command Handler
    │
    ▼
Subsystem
    │
    ▼
ACK / Result
```

This prevents the communication layer from directly manipulating hardware.

---

# 18. Telemetry Architecture

Telemetry should be generated centrally.

```text
Sensors
   │
   ▼
Sensor Manager
   │
   ▼
Telemetry Manager
   │
   ├── Sensor data
   ├── Encoder data
   ├── IMU data
   ├── Safety state
   ├── System state
   ├── Faults
   └── Health
   │
   ▼
Raspberry Pi
```

Telemetry should include timestamps so that Raspberry Pi software can correlate sensor measurements.

---

# 19. Diagnostics

Diagnostics should provide a consistent view of the embedded system.

Example:

```text
System Health
├── ESP32
├── Communication
├── Ultrasonic
├── IMU
├── Encoders
├── Safety
├── E-Stop
└── Watchdog
```

Each subsystem can report:

```text
INITIALIZING
READY
WARNING
FAULT
DISABLED
```

---

# 20. Watchdog

The ESP32 should use a watchdog mechanism to detect firmware/software hangs.

```text
Firmware Running
      │
      ▼
Watchdog Fed
      │
      ▼
Normal Operation

If firmware stops responding
      │
      ▼
Watchdog Timeout
      │
      ▼
Reset / Safe Recovery
```

The watchdog is a recovery mechanism; it should not be treated as the sole safety mechanism.

---

# 21. Hardware Abstraction Layer

Hardware-specific implementation should be isolated from higher-level logic.

```text
Application
     │
     ▼
Subsystem Interface
     │
     ▼
Hardware Abstraction Layer
     │
     ▼
ESP32 Hardware
```

For example:

```text
IUltrasonicSensor
IImu
IEncoder
ISafetyInput
ICommunication
IWheelchairInterface
```

This allows hardware implementations to be replaced without rewriting the entire application.

---

# 22. Wheelchair Interface Abstraction

A dedicated abstraction must exist for the future wheelchair controller integration.

```text
                 WheelchairInterface
                         │
             ┌───────────┴───────────┐
             │                       │
       Current Hardware         Future Hardware
       Interface TBD            Interface
```

The interface may eventually represent:

```text
enable()
disable()
stop()
set_direction()
set_speed()
get_status()
```

However, these functions are **architectural abstractions only** at this stage.

The existing wheelchair controller's electrical interface has not been verified sufficiently to implement direct ESP32 motor/control commands.

Therefore:

> No GPIO/PWM output should be connected to the wheelchair motor controller simply because the abstraction exists.

---

# 23. Joystick Architecture

The existing wheelchair joystick interface is treated as a separate hardware investigation.

Known observations include:

```text
9-pin interface

Pin 1  3V_393
Pin 2  INT
Pin 3  SCL
Pin 4  SDA
Pin 5  GND
Pin 6  16_DIO
Pin 7  16_CLK
Pin 8  EMGC
Pin 9  VCC
```

Observed interfaces include:

```text
SCL/SDA
≈ 75 kHz
I²C address observed: 0x0C

Pin 7 / Pin 6
clock/data-like activity
SPI-compatible/SPI-like behavior observed
```

The SPI-like interface should **not** currently be treated as confirmed standard SPI because a dedicated chip-select signal has not been established.

Movement-specific byte meanings remain unresolved.

Therefore:

```text
Joystick
   │
   ▼
Passive Observation
   │
   ▼
Protocol Analysis
   │
   ▼
Movement Mapping TBD
```

No signal injection should be performed during the current investigation.

---

# 24. Scheduling Model

The ESP32 firmware should use deterministic periodic tasks where appropriate.

Conceptually:

```text
High Frequency
│
├── Safety monitoring
├── Encoder acquisition
└── Critical inputs

Medium Frequency
│
├── Ultrasonic measurements
├── IMU sampling
└── Health monitoring

Lower Frequency
│
├── Telemetry
├── Diagnostics
└── Status reporting
```

Exact task frequencies should be determined during implementation and hardware testing.

The architecture should avoid blocking delays inside critical tasks.

---

# 25. Data Flow

### Sensor data

```text
Physical Sensor
      │
      ▼
Driver
      │
      ▼
Validation
      │
      ▼
Sensor Manager
      │
      ├────────► Safety Manager
      │
      └────────► Telemetry Manager
                         │
                         ▼
                    Raspberry Pi
```

### Command data

```text
Raspberry Pi
      │
      ▼
Communication
      │
      ▼
Parser
      │
      ▼
Validator
      │
      ▼
State/Safety Check
      │
      ▼
Subsystem
```

---

# 26. Recommended Firmware Repository Structure

```text
firmware/
└── esp32/
    │
    ├── src/
    │   ├── main/
    │   │   ├── main.cpp
    │   │   ├── system.cpp
    │   │   └── system.h
    │   │
    │   ├── core/
    │   │   ├── state_machine.cpp
    │   │   ├── state_machine.h
    │   │   ├── config.cpp
    │   │   └── config.h
    │   │
    │   ├── sensors/
    │   │   ├── ultrasonic/
    │   │   ├── imu/
    │   │   └── encoder/
    │   │
    │   ├── safety/
    │   │   ├── safety_manager.cpp
    │   │   ├── fault_manager.cpp
    │   │   ├── estop_monitor.cpp
    │   │   └── watchdog.cpp
    │   │
    │   ├── communication/
    │   │   ├── protocol.cpp
    │   │   ├── command_handler.cpp
    │   │   ├── telemetry.cpp
    │   │   └── heartbeat.cpp
    │   │
    │   ├── diagnostics/
    │   │   ├── logger.cpp
    │   │   └── health_monitor.cpp
    │   │
    │   └── hardware/
    │       ├── gpio/
    │       ├── i2c/
    │       ├── encoder/
    │       └── wheelchair_interface/
    │
    ├── include/
    │
    ├── tests/
    │
    └── platformio.ini
```

The exact build system can be selected during project setup; this architecture does not require a particular build tool.

---

# 27. Simulation Architecture

Before connecting safety-critical wheelchair hardware, the firmware should support simulated inputs.

```text
             Simulation Layer
                    │
        ┌───────────┼───────────┐
        │           │           │
   Sim Ultrasonic Sim Encoder Sim IMU
        │           │           │
        └───────────┼───────────┘
                    ▼
              Same Interfaces
                    │
                    ▼
              Normal Firmware
```

This allows:

- State-machine testing
- Safety testing
- Communication testing
- Command validation
- Fault testing
- Sensor-processing testing

without requiring physical wheelchair movement.

---

# 28. Testing Architecture

Testing should occur at multiple levels.

```text
Unit Tests
    │
    ▼
Module Tests
    │
    ▼
Hardware-in-the-Loop / Bench Tests
    │
    ▼
Integrated Sensor Tests
    │
    ▼
Safety Tests
    │
    ▼
Wheelchair Interface Tests
    │
    ▼
Controlled Movement Tests
```

No movement test should be performed until the wheelchair controller, braking behavior, power architecture, and external interface have been sufficiently verified.

---

# 29. Safety Boundaries

The following boundaries are mandatory for V1:

### ESP32 may

- Read sensors
- Monitor E-stop
- Process encoder pulses
- Read IMU
- Measure ultrasonic distances
- Validate commands
- Monitor communication
- Generate telemetry
- Detect faults
- Enter SAFE/FAULT states
- Run simulations
- Perform passive interface observation

### ESP32 must not yet

- Directly drive wheelchair motors
- Generate unverified motor PWM
- Bypass the existing wheelchair controller
- Inject signals into the existing joystick interface
- Assume a brake-control signal
- Assume motor-controller voltage/current requirements
- Claim autonomous movement
- Claim automatic braking is implemented

---

# 30. Final Architecture

The complete V1 ESP32 architecture is:

```text
                         RASPBERRY PI 4
                              │
                    Pi ↔ ESP32 Protocol
                              │
                              ▼
                    ┌───────────────────┐
                    │   COMMUNICATION   │
                    │                   │
                    │ Commands          │
                    │ Heartbeat         │
                    │ Telemetry         │
                    └─────────┬─────────┘
                              │
                              ▼
                    ┌───────────────────┐
                    │ COMMAND VALIDATOR │
                    └─────────┬─────────┘
                              │
                              ▼
                    ┌───────────────────┐
                    │   STATE MACHINE   │
                    └─────────┬─────────┘
                              │
             ┌────────────────┼────────────────┐
             │                │                │
             ▼                ▼                ▼
       SENSOR MANAGER    SAFETY MANAGER   DIAGNOSTICS
             │                │                │
      ┌──────┼──────┐         │                │
      │      │      │         │                │
      ▼      ▼      ▼         ▼                ▼
 Ultrasonic IMU  Encoders   E-STOP          Health
      │      │      │         │              Faults
      └──────┼──────┘         │                │
             │                │                │
             └────────────────┼────────────────┘
                              │
                              ▼
                       SAFETY DECISION
                        /          \
                     CLEAR          SAFE
                       │              │
                       ▼              ▼
                   CONTINUE       STOP/FAULT
                       │
                       ▼
              WHEELCHAIR INTERFACE
                    (FUTURE /
                 INTERFACE TBD)
```

---

# 31. V1 Architectural Principle

The ESP32 firmware should be developed so that:

```text
TODAY

Sensors
   ↓
ESP32
   ↓
Safety + Telemetry
   ↓
Raspberry Pi
```

can later evolve into:

```text
FUTURE

Sensors
   ↓
ESP32
   ↓
Safety + Real-Time Control
   ↓
Verified Wheelchair Interface
   ↓
Wheelchair Controller
   ↓
Motors
```

without requiring a rewrite of the sensor, safety, communication, state-machine, and diagnostic subsystems.

---

# 32. Architecture Acceptance Criteria

The architecture is considered ready for implementation when:

- [ ] ESP32 responsibilities are separated from Raspberry Pi responsibilities.
- [ ] Sensor drivers are modular.
- [ ] Safety logic is independent from application logic.
- [ ] State transitions are centralized.
- [ ] Commands pass through validation.
- [ ] Telemetry has a defined path.
- [ ] Heartbeat monitoring exists.
- [ ] Fault handling exists.
- [ ] Watchdog handling exists.
- [ ] Hardware abstraction exists.
- [ ] Simulation is possible.
- [ ] Wheelchair interface is abstracted but not assumed.
- [ ] Direct motor control is excluded from V1 until hardware verification.
- [ ] Joystick interface remains passive-observation-only during the current investigation.
- [ ] Firmware can be tested without moving the wheelchair.

---

## 33. Development Dependency

The implementation sequence following this architecture should be:

```text
ESP32 Project Setup
        ↓
Core Firmware
        ↓
State Machine
        ↓
Logging / Diagnostics
        ↓
Ultrasonic Driver
        ↓
IMU Driver
        ↓
Encoder Driver
        ↓
Safety Manager
        ↓
Simulation
        ↓
Pi Communication Protocol
        ↓
Integrated Sensor Testing
        ↓
Raspberry Pi Integration
        ↓
Wheelchair Interface Abstraction
        ↓
Verified Hardware Interface
        ↓
Controlled Movement Testing
```

**This architecture does not authorize physical wheelchair movement.**

Physical movement remains dependent on verification of the existing wheelchair controller, brake mechanism, electrical interface, and safe control method.