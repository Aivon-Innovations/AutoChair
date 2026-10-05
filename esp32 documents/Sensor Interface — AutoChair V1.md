# `SENSOR_INTERFACE.md`

**Project:** AutoChair – AI-Powered Smart Wheelchair  
**Company:** Aivon Innovations Pvt. Ltd.  
**Platform:** ESP32-WROOM-32D  
**Version:** V1.0  
**Status:** Development Specification

---

# 1. Purpose

This document defines the common architecture and interface requirements for sensors connected to the ESP32.

The V1 sensor layer covers:

- HC-SR04 ultrasonic sensors
- MPU6500 / MPU6050 IMU
- 600 PPR ABZ wheel encoders

The sensor layer provides validated measurements to:

```text
Sensor Drivers
      ↓
Sensor Manager
      ↓
Safety / Telemetry / Application
```

---

# 2. Sensor Architecture

```text
                    SENSOR MANAGER
                         │
          ┌──────────────┼──────────────┐
          │              │              │
          ▼              ▼              ▼
     Ultrasonic         IMU          Encoders
          │              │              │
          ▼              ▼              ▼
      Distance       Motion Data    Pulse/Direction
          │              │              │
          └──────────────┼──────────────┘
                         ▼
                  Validated Data
                         │
             ┌───────────┼───────────┐
             ▼           ▼           ▼
          Safety     Telemetry    Diagnostics
```

---

# 3. Design Principles

The sensor architecture must:

- Keep hardware drivers separate from application logic.
- Validate measurements before publishing them.
- Detect sensor failures.
- Provide timestamps.
- Provide sensor health/status.
- Avoid blocking critical firmware operations.
- Allow sensors to be replaced without rewriting the application.
- Support simulation/testing without physical sensors.

---

# 4. Common Sensor Interface

All sensor modules should expose a common conceptual interface.

```cpp
class ISensor {
public:
    virtual bool begin() = 0;
    virtual void update() = 0;
    virtual bool isHealthy() const = 0;
    virtual SensorStatus getStatus() const = 0;
};
```

The exact C++ implementation can be adapted during firmware development.

---

# 5. Sensor Status

Common status values:

```cpp
enum class SensorStatus {
    UNINITIALIZED,
    INITIALIZING,
    OK,
    WARNING,
    TIMEOUT,
    INVALID,
    DISCONNECTED,
    FAULT
};
```

A sensor must not report `OK` simply because its driver initialized successfully.

`OK` means that the latest required measurement has passed the module's validation checks.

---

# 6. Sensor Data Requirements

Every measurement should provide:

```text
Sensor ID
Timestamp
Measurement
Status
```

Example:

```cpp
struct SensorReading {
    uint8_t sensor_id;
    uint32_t timestamp_ms;
    SensorStatus status;
};
```

Sensor-specific structures can extend this representation.

---

# 7. Ultrasonic Sensors

## 7.1 Hardware

Available:

```text
HC-SR04 × 8
```

Initial planned configuration:

```text
Front:  3
Left:   1
Right:  1
Rear:   1

Spare:  2
```

The final physical mounting arrangement must be verified during hardware integration.

---

# 8. HC-SR04 Interface

Each HC-SR04 provides:

```text
VCC
TRIG
ECHO
GND
```

Conceptually:

```text
ESP32
 │
 ├── GPIO → TRIG
 │
 └── GPIO ← ECHO
             │
             ▼
          HC-SR04
```

The electrical interface must be verified before connecting the HC-SR04 ECHO output directly to an ESP32 GPIO, particularly because the ESP32 GPIO is not a 5 V logic input.

A suitable level-shifting/protection arrangement must be used where required.

---

# 9. Ultrasonic Measurement

Measurement process:

```text
Trigger
   ↓
Send ultrasonic pulse
   ↓
Wait for ECHO
   ↓
Measure pulse duration
   ↓
Validate duration
   ↓
Calculate distance
   ↓
Publish reading
```

The driver must handle:

- No echo
- Timeout
- Invalid pulse duration
- Out-of-range result
- Sensor disagreement where applicable

---

# 10. Ultrasonic Data Structure

Conceptually:

```cpp
struct UltrasonicReading {
    uint8_t id;
    float distance_mm;
    uint32_t timestamp_ms;
    SensorStatus status;
};
```

An invalid reading should not be represented as a legitimate `0 mm` distance.

Example:

```text
distance = INVALID
status   = TIMEOUT
```

---

# 11. Ultrasonic Sensor Manager

The Ultrasonic Manager controls all configured ultrasonic sensors.

```text
             Ultrasonic Manager
                    │
      ┌─────────────┼─────────────┐
      ▼             ▼             ▼
    US1            US2           US3
      │             │             │
     ...           ...           ...
      │
      ▼
Validated Distance Array
```

Example:

```cpp
UltrasonicReading readings[8];
```

Only configured sensors need to be active.

---

# 12. Ultrasonic Scheduling

Ultrasonic sensors should not necessarily be triggered simultaneously.

A sequential schedule should be used to reduce potential ultrasonic interference.

Conceptually:

```text
US1 → wait for result
          ↓
US2 → wait for result
          ↓
US3 → wait for result
          ↓
...
```

The exact measurement interval must be selected during implementation and bench testing.

---

# 13. Ultrasonic Health

Each sensor should track:

```text
Last successful measurement
Last measurement timestamp
Consecutive failures
Current status
```

Example:

```cpp
struct UltrasonicHealth {
    uint32_t last_success_ms;
    uint16_t consecutive_failures;
    SensorStatus status;
};
```

A single missed measurement should not automatically create a critical system fault.

Fault severity is determined by the Safety Manager and the operational context.

---

# 14. IMU Interface

Available hardware:

```text
MPU6500 × 1
MPU6050 × 3
```

The architecture supports both devices through an abstract IMU interface.

```text
              IIMU
               │
        ┌──────┴──────┐
        ▼             ▼
     MPU6500       MPU6050
```

V1 should begin with **one verified IMU**.

---

# 15. IMU Communication

The IMU interface should be isolated behind an I²C/SPI hardware abstraction.

Conceptually:

```text
IMU Driver
    │
    ▼
Bus Interface
    │
    ▼
ESP32
```

The exact bus and address configuration must match the actual selected IMU hardware and wiring.

---

# 16. IMU Measurements

The basic V1 measurement set is:

```text
Accelerometer
    X
    Y
    Z

Gyroscope
    X
    Y
    Z
```

Example:

```cpp
struct IMUReading {
    float accel_x;
    float accel_y;
    float accel_z;

    float gyro_x;
    float gyro_y;
    float gyro_z;

    uint32_t timestamp_ms;
    SensorStatus status;
};
```

Units must be explicitly defined by the final driver implementation.

---

# 17. IMU Validation

The driver should detect:

- Initialization failure
- Communication failure
- Invalid register response
- Missing measurements
- Out-of-range readings where applicable

Example:

```text
IMU communication lost
        ↓
IMU status = FAULT
        ↓
Fault Manager
        ↓
Safety Manager evaluates severity
```

The sensor driver itself should report the condition; it should not independently decide that the entire wheelchair must enter a particular safety state.

---

# 18. Orientation

The IMU driver provides raw/processed accelerometer and gyroscope data.

Orientation estimation is a separate processing layer.

```text
IMU
 ↓
Raw measurements
 ↓
IMU processing
 ↓
Optional orientation estimation
```

V1 must not claim validated orientation estimation unless it has been implemented and tested.

---

# 19. Encoder Interface

Available hardware:

```text
600 PPR ABZ optical encoders × 2
```

Planned assignment:

```text
Left encoder  → Left wheel
Right encoder → Right wheel
```

The physical mounting and exact relationship between encoder shaft rotation and wheel rotation must be verified.

---

# 20. Encoder Signals

Each encoder provides:

```text
A
B
Z
```

Conceptually:

```text
Encoder
 ├── A ──► ESP32
 ├── B ──► ESP32
 └── Z ──► ESP32
```

A/B provide quadrature information.

Z is the index/reference signal.

The exact electrical output type and voltage levels must be verified before connecting to ESP32 GPIOs.

---

# 21. Encoder Acquisition

The ESP32 should use an appropriate hardware-supported pulse/interrupt mechanism rather than relying on slow polling.

Conceptually:

```text
Encoder A/B
     │
     ▼
Quadrature Decoder
     │
     ├── Pulse count
     └── Direction
```

Z should be captured separately as an index event where required.

---

# 22. Encoder Data

Example:

```cpp
struct EncoderReading {
    int64_t count;
    int8_t direction;
    uint32_t timestamp_ms;
    SensorStatus status;
};
```

For two wheels:

```cpp
struct WheelEncoderData {
    EncoderReading left;
    EncoderReading right;
};
```

---

# 23. Encoder Count vs Distance

Encoder pulses must not automatically be treated as centimeters or meters.

The conversion requires:

```text
Encoder PPR
×
Quadrature decoding factor
×
Gear/mechanical ratio
×
Wheel circumference
×
Encoder mounting relationship
```

These values must be experimentally verified.

The ESP32 should initially expose raw counts and direction.

Distance/velocity estimation can be added after calibration.

---

# 24. Encoder Health

The encoder subsystem should monitor:

- Pulse activity
- Signal validity
- Direction consistency
- Index events where used
- Communication/interrupt subsystem health

Example states:

```text
OK
NO_ACTIVITY
INVALID
FAULT
```

No encoder pulses does not automatically mean the encoder is broken; the system must consider whether wheel movement is expected.

---

# 25. Sensor Manager

The Sensor Manager provides the common interface between individual drivers and the rest of the firmware.

```text
             SENSOR MANAGER
                  │
      ┌───────────┼───────────┐
      │           │           │
      ▼           ▼           ▼
 Ultrasonic      IMU       Encoders
      │           │           │
      └───────────┼───────────┘
                  │
                  ▼
             Sensor Data
```

Responsibilities:

- Initialize sensors
- Schedule acquisition
- Collect readings
- Validate readings
- Track health
- Expose latest data
- Publish telemetry
- Report faults

---

# 26. Sensor Data Flow

```text
Physical Sensor
      │
      ▼
Hardware Driver
      │
      ▼
Raw Measurement
      │
      ▼
Validation
      │
      ▼
Sensor Manager
      │
      ├──────────────► Safety Manager
      │
      ├──────────────► Telemetry
      │
      └──────────────► Diagnostics
```

---

# 27. Sensor Configuration

Sensor configuration should be centralized rather than scattered throughout driver code.

Example:

```cpp
struct SensorConfig {
    bool enabled;
    uint8_t id;
    uint32_t update_interval_ms;
};
```

Ultrasonic-specific configuration may include:

```text
TRIG pin
ECHO pin
measurement timeout
minimum distance
maximum distance
```

IMU configuration may include:

```text
bus
address
accelerometer range
gyroscope range
sample rate
```

Encoder configuration may include:

```text
A pin
B pin
Z pin
PPR
decoding mode
```

Exact values should be populated from verified hardware and final wiring.

---

# 28. Sensor Initialization

Startup sequence:

```text
ESP32 BOOT
    ↓
INITIALIZING
    ↓
Initialize Sensor Manager
    ↓
Initialize Ultrasonic
    ↓
Initialize IMU
    ↓
Initialize Encoders
    ↓
Run basic health checks
    ↓
SELF_TEST
```

A sensor that is optional for the current configuration may be marked unavailable rather than preventing startup.

A sensor required by the configured operating mode must pass the corresponding readiness checks.

---

# 29. Sensor Simulation

Every sensor interface should support simulated data where practical.

```text
Simulation
    │
    ├── Sim Ultrasonic
    ├── Sim IMU
    └── Sim Encoder
          │
          ▼
    Same Sensor Interfaces
          │
          ▼
    Same Safety / Telemetry
```

This allows state-machine and communication development before all physical hardware is connected.

---

# 30. Error Handling

Sensor errors must be represented explicitly.

```text
Measurement
    │
    ▼
Validation
    │
 ┌──┴──────────────┐
 ▼                 ▼
VALID            INVALID
 │                 │
 ▼                 ▼
Publish          Record fault
                    │
                    ▼
               Safety Manager
```

The sensor driver must not fabricate a replacement measurement when a measurement is unavailable.

---

# 31. Safety Boundary

The sensor layer provides information.

It does not independently control the wheelchair.

```text
Sensor
  ↓
Measurement
  ↓
Sensor Manager
  ↓
Safety Manager / Application
```

The following are outside this document:

- Motor control
- Brake actuation
- Wheelchair controller commands
- Autonomous navigation
- Route planning
- Camera AI
- Voice processing

---

# 32. Testing Requirements

### Ultrasonic

- [ ] Single sensor test
- [ ] All configured sensors
- [ ] Distance validation
- [ ] Timeout handling
- [ ] Invalid measurement handling
- [ ] Sequential triggering
- [ ] Sensor failure simulation

### IMU

- [ ] Initialization
- [ ] Accelerometer readings
- [ ] Gyroscope readings
- [ ] Communication failure
- [ ] Invalid data handling
- [ ] Static sensor test

### Encoders

- [ ] A/B signal detection
- [ ] Forward direction
- [ ] Reverse direction
- [ ] Count accuracy
- [ ] Z/index detection
- [ ] High-speed pulse handling
- [ ] Disconnected/fault simulation

---

# 33. Acceptance Criteria

The sensor interface implementation is ready for V1 integration when:

- [ ] Drivers are separated from application logic.
- [ ] Sensor status is available.
- [ ] Measurements are timestamped.
- [ ] Invalid measurements are detectable.
- [ ] Sensor failures are reported.
- [ ] Ultrasonic acquisition works.
- [ ] One verified IMU works.
- [ ] Both encoder inputs can be acquired.
- [ ] Sensor data can be requested through the Pi protocol.
- [ ] Sensor telemetry works.
- [ ] Simulation interfaces work.
- [ ] No sensor driver directly controls wheelchair motors.
- [ ] Electrical signal levels have been verified before physical connection.

---

# 34. Implementation Order

```text
Sensor interfaces
      ↓
Sensor configuration
      ↓
Sensor Manager
      ↓
Ultrasonic driver
      ↓
IMU driver
      ↓
Encoder driver
      ↓
Health monitoring
      ↓
Telemetry integration
      ↓
Simulation
      ↓
Hardware testing
      ↓
Safety integration
```

---

# 35. V1 Hardware Boundary

The current sensor architecture uses the hardware already available:

```text
HC-SR04 × 8
MPU6500 × 1
MPU6050 × 3
600 PPR ABZ Encoder × 2
ESP32-WROOM-32D
```

V1 should initially activate only the sensors required for development and testing.

No additional sensor hardware is required merely to begin implementation.