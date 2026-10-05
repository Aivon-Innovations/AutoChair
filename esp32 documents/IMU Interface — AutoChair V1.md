# `IMU_INTERFACE.md`

**Project:** AutoChair – AI-Powered Smart Wheelchair  
**Company:** Aivon Innovations Pvt. Ltd.  
**Platform:** ESP32-WROOM-32D  
**Version:** V1.0  
**Status:** Development Specification

---

# 1. Purpose

This document defines the interface between the ESP32 and the AutoChair IMU subsystem.

Available IMU hardware:

- MPU6500 ×1
- MPU6050 ×3

The V1 architecture uses **one verified IMU initially**. The remaining IMUs are available for later experiments and redundancy research.

The IMU subsystem provides:

- Accelerometer measurements
- Gyroscope measurements
- Sensor health/status
- Timestamped data
- Data for telemetry
- Data for future motion/odometry processing

---

# 2. Architectural Position

```text
                 ESP32
                   │
                   ▼
              IMU Driver
                   │
                   ▼
             IMU Interface
                   │
                   ▼
             Sensor Manager
                   │
        ┌──────────┼──────────┐
        ▼          ▼          ▼
      Safety    Telemetry   Processing
```

The IMU driver must not directly control motors, brakes, or the wheelchair controller.

---

# 3. Supported Devices

The common interface must support:

```text
IMU Interface
     │
 ┌───┴────┐
 ▼        ▼
MPU6500  MPU6050
```

The driver should hide device-specific register/configuration details from higher-level firmware.

---

# 4. Communication Interface

The IMU is expected to communicate with the ESP32 through a supported digital bus.

The architecture should support:

- I²C
- SPI where required/implemented

The exact bus must be selected according to the actual IMU board, wiring, and verified hardware configuration.

The firmware must not assume a particular bus merely from the sensor model.

---

# 5. Initialization

The initialization sequence is:

```text
ESP32 Startup
     │
     ▼
IMU Driver Initialization
     │
     ▼
Bus Initialization
     │
     ▼
Device Detection
     │
     ▼
Configuration
     │
     ▼
Basic Read Test
     │
     ▼
IMU READY
```

If initialization fails:

```text
Initialization Failure
        │
        ▼
IMU = FAULT
        │
        ▼
Fault Manager
        │
        ▼
Safety Manager evaluates severity
```

The IMU driver must report the failure rather than independently deciding the complete system safety state.

---

# 6. IMU Data

The basic V1 data set is:

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

Conceptual structure:

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

The exact units must be defined by the implemented driver.

---

# 7. Raw vs Converted Data

The driver should maintain a clear distinction between:

```text
Raw sensor values
        ↓
Calibration / conversion
        ↓
Engineering units
        ↓
IMUReading
```

Raw register values must not be silently interpreted as physical units without the correct device configuration.

---

# 8. Accelerometer

The accelerometer measures linear acceleration along three axes:

```text
X
Y
Z
```

The driver should provide the configured measurement range.

The exact accelerometer range is a firmware configuration parameter and must correspond to the actual device configuration.

---

# 9. Gyroscope

The gyroscope provides angular velocity:

```text
X
Y
Z
```

The driver should expose readings in clearly documented engineering units after conversion.

The configured gyroscope range must match the actual hardware configuration.

---

# 10. Timestamping

Every published IMU reading must have a timestamp.

Example:

```cpp
timestamp_ms
```

The timestamp should use the ESP32 monotonic runtime clock.

This allows:

- Sensor-data correlation
- Telemetry correlation
- Motion analysis
- Debugging
- Future sensor fusion

The timestamp is not a synchronized wall-clock timestamp unless time synchronization is implemented separately.

---

# 11. Sampling

The IMU driver should acquire data at a configurable sampling rate.

Conceptually:

```text
IMU
 │
 ├── Sample
 ├── Sample
 ├── Sample
 └── Sample
       │
       ▼
 IMU Driver
       │
       ▼
 Sensor Manager
```

The final sampling frequency should be selected during implementation based on:

- Device configuration
- ESP32 processing load
- Communication requirements
- Required motion resolution
- Experimental testing

Do not treat an arbitrary frequency as the final validated value.

---

# 12. Data Validation

Each IMU reading should be checked before publication.

Validation may include:

- Successful device communication
- Valid register response
- Expected data format
- Timestamp validity
- Configured measurement-range consistency
- Detection of persistent communication failure

Invalid data must be marked invalid rather than silently replaced with a fabricated value.

---

# 13. Sensor Status

The IMU uses the common sensor status model:

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

Example:

```text
Communication successful
        ↓
Measurement valid
        ↓
status = OK
```

Communication failure:

```text
Communication failure
        ↓
status = FAULT
        ↓
Fault Manager
```

---

# 14. IMU Health Monitoring

The IMU driver should maintain health information such as:

```text
Last successful reading
Last successful timestamp
Consecutive failures
Current status
```

Conceptual structure:

```cpp
struct IMUHealth {
    uint32_t last_success_ms;
    uint32_t consecutive_failures;
    SensorStatus status;
};
```

The Safety Manager determines whether an IMU fault is operationally significant.

---

# 15. Calibration

Calibration should be treated as a separate layer from basic sensor communication.

```text
Hardware
   ↓
Raw IMU
   ↓
Calibration
   ↓
Processed IMU data
```

Possible calibration parameters include:

```text
Accelerometer bias
Gyroscope bias
Axis alignment
Scale factors
```

Calibration values must be measured experimentally.

They must not be invented or hard-coded without verification.

---

# 16. Axis Convention

The firmware must define one consistent coordinate convention.

Example conceptual convention:

```text
             +Z
              ↑
              │
              │
      -X ─────┼───── +X
             /
           /
         +Y
```

However, the final physical X/Y/Z orientation must be documented according to how the IMU is actually mounted on the wheelchair.

Software must not assume that the sensor-board axes automatically correspond to wheelchair axes.

---

# 17. Mounting

The IMU should be mounted securely so that:

- It does not move independently of the wheelchair structure.
- Its orientation remains known.
- Excessive vibration is minimized where practical.
- The cable connection is secure.
- The mounting position can be documented.

The final mounting location is a hardware-integration decision and must be experimentally verified.

---

# 18. Orientation Estimation

Raw accelerometer and gyroscope measurements are available in V1.

Orientation estimation is a separate processing function.

```text
Accelerometer ──┐
                ├──► Sensor Fusion ──► Orientation
Gyroscope ──────┘
```

Possible future processing includes:

- Roll
- Pitch
- Yaw estimation
- Tilt detection
- Motion classification

V1 must not claim validated orientation estimation until the algorithm and physical behavior have been tested.

---

# 19. IMU and Odometry

The IMU may later contribute to motion estimation.

Conceptually:

```text
Encoders ──────┐
               ├──► Sensor Fusion ──► Motion Estimate
IMU ───────────┘
```

For V1:

> Encoder data and IMU data should remain independently available.

Sensor fusion should be introduced only after both individual sensor streams have been verified.

---

# 20. IMU and Safety

The IMU can provide information relevant to future safety functions such as:

- Excessive tilt detection
- Unexpected acceleration
- Motion detection
- Abnormal movement

However, these functions are **not automatically part of V1 safety behavior**.

A safety rule should only be enabled after:

1. The measurement is validated.
2. The threshold is experimentally established.
3. False-positive behavior is evaluated.
4. The resulting safety action is tested.

---

# 21. Raspberry Pi Interface

The ESP32 provides IMU data to the Raspberry Pi through the ESP32↔Pi protocol.

```text
IMU
 │
 ▼
ESP32 IMU Driver
 │
 ▼
Sensor Manager
 │
 ▼
Telemetry Manager
 │
 ▼
Pi Protocol
 │
 ▼
Raspberry Pi
```

The Raspberry Pi may request:

```text
GET_IMU_DATA
```

or receive periodic IMU telemetry.

---

# 22. Example IMU Response

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
    },
    "timestamp_ms": 15240
  }
}
```

The numerical values above are only an example of the message structure, not measured AutoChair data.

---

# 23. Error Handling

Possible IMU errors:

```text
IMU_NOT_INITIALIZED
IMU_COMMUNICATION_FAILURE
IMU_INVALID_DATA
IMU_TIMEOUT
IMU_CONFIGURATION_FAILURE
```

The driver should expose the error to the Sensor Manager/Fault Manager.

It should not:

- Crash the firmware.
- Block the entire ESP32 indefinitely.
- Fabricate sensor values.
- Directly command wheelchair movement.

---

# 24. Simulation Interface

The IMU interface should support simulated data.

```text
             IIMU
              │
       ┌──────┴──────┐
       ▼             ▼
 Real IMU        Simulated IMU
       │             │
       └──────┬──────┘
              ▼
         Sensor Manager
```

This allows testing of:

- Telemetry
- State machine
- Safety rules
- Fault handling
- Raspberry Pi communication

without physical IMU hardware.

---

# 25. Testing

## Initialization

- [ ] Device detected
- [ ] Configuration succeeds
- [ ] Basic read succeeds
- [ ] Failure is reported correctly

## Accelerometer

- [ ] X/Y/Z readings available
- [ ] Units verified
- [ ] Static test performed
- [ ] Axis orientation documented

## Gyroscope

- [ ] X/Y/Z readings available
- [ ] Units verified
- [ ] Stationary behavior tested
- [ ] Rotation response tested

## Reliability

- [ ] Communication failure
- [ ] Invalid response
- [ ] Timeout
- [ ] Recovery
- [ ] Reinitialization

## Integration

- [ ] Sensor Manager
- [ ] Telemetry
- [ ] Pi protocol
- [ ] Diagnostics
- [ ] Safety Manager

---

# 26. Acceptance Criteria

The V1 IMU interface is ready for integration when:

- [ ] One physical IMU has been successfully initialized.
- [ ] Accelerometer data is available.
- [ ] Gyroscope data is available.
- [ ] Units are documented.
- [ ] Timestamps are available.
- [ ] Sensor status is available.
- [ ] Communication failures are detected.
- [ ] Invalid data is handled.
- [ ] IMU data is available through the Pi protocol.
- [ ] Simulation is supported.
- [ ] Physical mounting orientation is documented.
- [ ] No unvalidated IMU-derived safety behavior is enabled.

---

# 27. Implementation Order

```text
IMU interface
      ↓
Bus abstraction
      ↓
MPU6500 / MPU6050 driver
      ↓
Device initialization
      ↓
Accelerometer
      ↓
Gyroscope
      ↓
Validation
      ↓
Health monitoring
      ↓
Sensor Manager
      ↓
Telemetry
      ↓
Pi protocol
      ↓
Simulation tests
      ↓
Physical IMU testing
      ↓
Calibration
      ↓
Future sensor fusion
```

---

# 28. V1 Boundary

V1 provides a reliable software path for:

```text
MPU6500 / MPU6050
        ↓
ESP32
        ↓
Validated IMU data
        ↓
Safety / Telemetry / Raspberry Pi
```

V1 does **not** claim:

- Validated autonomous localization
- Validated orientation estimation
- Complete sensor fusion
- Automatic braking based on IMU
- Tilt-based emergency braking
- Autonomous wheelchair control

Those capabilities require separate implementation and experimental validation.