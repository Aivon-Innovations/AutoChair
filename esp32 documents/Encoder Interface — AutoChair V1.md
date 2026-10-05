# `ENCODER_INTERFACE.md`

**Project:** AutoChair – AI-Powered Smart Wheelchair  
**Company:** Aivon Innovations Pvt. Ltd.  
**Platform:** ESP32-WROOM-32D  
**Version:** V1.0  
**Status:** Development Specification

---

# 1. Purpose

This document defines the interface between the ESP32 and the AutoChair wheel encoder subsystem.

Available hardware:

- 600 PPR ABZ optical encoder ×2
- ESP32-WROOM-32D ×3 available

The initial configuration uses:

```text
Encoder 1 → Left wheel
Encoder 2 → Right wheel
```

The encoder subsystem provides:

- Pulse counting
- Direction detection
- Index/reference detection
- Wheel-motion feedback
- Timestamped measurements
- Encoder health/status
- Data for telemetry
- Data for future odometry

---

# 2. Architectural Position

```text
             LEFT ENCODER
                  │
                  ▼
            Encoder Driver
                  │
                  │
             RIGHT ENCODER
                  │
                  ▼
            Encoder Driver
                  │
                  ▼
          Encoder Manager
                  │
       ┌──────────┼──────────┐
       ▼          ▼          ▼
    Safety     Telemetry   Odometry
```

The encoder subsystem must remain independent from direct motor control.

---

# 3. Encoder Hardware

Available:

```text
600 PPR ABZ optical encoder × 2
```

Planned assignment:

| Encoder | Wheel |
|---|---|
| Encoder 1 | Left |
| Encoder 2 | Right |

The physical mounting location and mechanical coupling must be verified before final distance calculations are enabled.

---

# 4. ABZ Signals

Each encoder provides three logical channels:

```text
A
B
Z
```

Conceptually:

```text
Encoder
 ├── A ──► ESP32 GPIO
 ├── B ──► ESP32 GPIO
 └── Z ──► ESP32 GPIO
```

### Channel A/B

A and B provide quadrature information.

Their phase relationship can be used to determine direction.

### Channel Z

Z is the index/reference signal.

Its exact use in AutoChair must be determined during encoder integration and calibration.

---

# 5. Electrical Interface

Before connecting an encoder to the ESP32, verify:

- Encoder supply voltage
- Output voltage
- Output type
- Common ground
- A/B/Z signal levels
- Maximum pulse frequency
- Cable requirements

The encoder output must be electrically compatible with ESP32 GPIO inputs.

Do not assume the encoder's output is 3.3 V simply because the ESP32 is 3.3 V logic.

---

# 6. Encoder Acquisition

The ESP32 should use an appropriate hardware-supported pulse/interrupt mechanism.

Conceptually:

```text
A/B signals
    │
    ▼
Quadrature Decoder
    │
    ├── Count
    └── Direction
```

Polling should not be used if it cannot reliably capture the encoder pulse rate.

The final acquisition implementation must be tested at the maximum expected wheel speed.

---

# 7. Quadrature Operation

Conceptually:

```text
Forward:

A: ──┐    ┌────┐    ┌────
      └────┘    └────

B: ─────┐    ┌────┐    ┌──
        └────┘    └────


Reverse:

The phase relationship is reversed.
```

The firmware must determine the actual direction convention experimentally.

Do not assume:

```text
A leads B = forward
```

until the physical encoder installation has been tested.

---

# 8. Pulse Counting

The encoder driver maintains a cumulative count.

Example:

```cpp
struct EncoderReading {
    int64_t count;
    int8_t direction;
    uint32_t timestamp_ms;
    SensorStatus status;
};
```

Possible direction values:

```text
FORWARD
REVERSE
STATIONARY
UNKNOWN
```

The actual sign convention must be configured after physical testing.

---

# 9. PPR Definition

The hardware is specified as:

```text
600 PPR
```

The firmware must explicitly document how the encoder manufacturer defines PPR.

Do not automatically assume:

```text
600 PPR × 4 = 2400 counts/revolution
```

unless the encoder specification and selected quadrature decoding mode establish that relationship.

The implementation must distinguish:

```text
PPR
CPR
counts/revolution
```

where relevant.

---

# 10. Encoder Distance

Raw encoder counts must initially remain available.

Distance can later be calculated using experimentally verified parameters:

```text
Encoder counts
       ↓
Counts/revolution
       ↓
Mechanical transmission
       ↓
Wheel rotations
       ↓
Wheel circumference
       ↓
Distance
```

Conceptually:

```text
distance =
    wheel_revolutions × wheel_circumference
```

The exact calculation depends on where the encoder is mechanically mounted.

---

# 11. Existing AutoChair Mechanical Data

The existing wheelchair inspection recorded an approximate motor-to-wheel reduction of:

```text
≈ 23.3 : 1
```

However, this value must **not** automatically be used in the encoder conversion.

The encoder's physical mounting point must first be established.

For example:

```text
Encoder on motor shaft
        ↓
gear reduction matters

Encoder on wheel shaft
        ↓
gear reduction may not be part of the conversion
```

Therefore:

> Encoder calibration must be based on the actual encoder installation, not merely the wheelchair gearbox ratio.

---

# 12. Wheel Circumference

Wheel circumference must be measured from the actual wheel used by the encoder system.

Conceptually:

```text
C = π × D
```

where `D` is the verified effective wheel diameter.

The measured effective rolling circumference may differ from the nominal physical diameter because of:

- Tire deformation
- Load
- Wheel construction
- Surface conditions

The final odometry calibration should therefore be experimentally verified.

---

# 13. Encoder Velocity

Once calibrated, wheel velocity can be estimated from count changes over time.

```text
Δcounts
   ↓
Δwheel rotation
   ↓
Δdistance
   ↓
Δdistance / Δtime
   ↓
wheel velocity
```

Conceptually:

```cpp
velocity = delta_distance / delta_time;
```

The initial V1 implementation should expose raw counts before relying on calculated velocity for safety-critical decisions.

---

# 14. Encoder Data Structure

Recommended structure:

```cpp
struct EncoderReading {
    int64_t count;
    int8_t direction;

    uint32_t timestamp_ms;

    uint32_t pulses_since_last_update;

    SensorStatus status;
};
```

For both wheels:

```cpp
struct WheelEncoderData {
    EncoderReading left;
    EncoderReading right;
};
```

Future fields may include:

```text
wheel_speed
distance
revolutions
index_count
```

---

# 15. Index / Z Channel

The Z channel provides an index/reference pulse.

Architecture:

```text
Z
│
▼
Index Detector
│
▼
Index Event
│
▼
Encoder Manager
```

The V1 implementation should record Z events even if they are not yet used for odometry.

Possible future uses include:

- Reference position
- Calibration
- Revolution tracking
- Encoder diagnostics

The exact behavior must be determined from the actual encoder specification and testing.

---

# 16. Encoder Health

The encoder subsystem should monitor:

```text
Signal activity
Pulse validity
Direction validity
Index events
Acquisition errors
```

Example status:

```text
OK
WARNING
NO_ACTIVITY
INVALID
FAULT
```

---

# 17. No-Activity Handling

No encoder pulses do not automatically mean encoder failure.

For example:

```text
Wheel stationary
      ↓
No pulses
      ↓
Expected
```

But:

```text
Wheel known to be rotating
      ↓
No pulses
      ↓
Potential encoder fault
```

Therefore encoder health evaluation should consider system context.

---

# 18. Encoder Manager

The Encoder Manager coordinates both encoder drivers.

```text
                 ENCODER MANAGER
                       │
             ┌─────────┴─────────┐
             ▼                   ▼
       Left Encoder        Right Encoder
             │                   │
             ▼                   ▼
        Count/Direction     Count/Direction
             │                   │
             └─────────┬─────────┘
                       ▼
                Wheel Encoder Data
```

Responsibilities:

- Initialize both encoders
- Acquire A/B signals
- Acquire Z signals
- Maintain counts
- Determine direction
- Track health
- Timestamp readings
- Provide telemetry
- Provide data for odometry

---

# 19. Sensor Manager Integration

The Encoder Manager is part of the general Sensor Manager.

```text
                 SENSOR MANAGER
                       │
       ┌───────────────┼───────────────┐
       ▼               ▼               ▼
 Ultrasonic           IMU          Encoders
                                       │
                                       ▼
                               Encoder Manager
```

Encoder data must remain available independently from ultrasonic and IMU data.

---

# 20. Telemetry

The ESP32 can provide encoder telemetry to the Raspberry Pi.

Example:

```json
{
  "message_type": "TELEMETRY",
  "telemetry_type": "ENCODER",
  "left": {
    "count": 10500,
    "direction": "FORWARD",
    "status": "OK"
  },
  "right": {
    "count": 10498,
    "direction": "FORWARD",
    "status": "OK"
  }
}
```

The values above are examples only.

---

# 21. `GET_ENCODER_DATA`

The Raspberry Pi can request current encoder data:

```text
GET_ENCODER_DATA
```

Example response:

```json
{
  "command": "GET_ENCODER_DATA",
  "status": "OK",
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

The protocol must not interpret counts as meters unless encoder calibration has been completed.

---

# 22. Encoder and Odometry

The encoder subsystem provides the primary wheel-motion measurements for basic odometry.

Conceptually:

```text
Left encoder ─────┐
                  ├──► Odometry
Right encoder ────┘
```

For a differential-drive-style model, future odometry can use:

```text
Left wheel distance
Right wheel distance
Wheel separation
```

to estimate:

```text
Δx
Δy
Δheading
```

However, the exact kinematic model must match the actual wheelchair geometry and be validated experimentally.

---

# 23. Encoder + IMU Fusion

Future architecture:

```text
Encoder Data ──┐
               ├──► Sensor Fusion ──► Motion Estimate
IMU Data ──────┘
```

V1 should first validate:

1. Encoder data independently.
2. IMU data independently.
3. Timestamp alignment.
4. Wheel calibration.

Only then should sensor fusion be introduced.

---

# 24. Calibration Procedure

The encoder calibration procedure should eventually include:

### Step 1 — Identify mounting

Determine whether the encoder measures:

```text
Motor shaft
Gearbox output
Wheel shaft
Other mechanical element
```

### Step 2 — Verify counts

Rotate the encoder through a known number of revolutions.

Record:

```text
Expected revolutions
Measured counts
```

### Step 3 — Verify direction

Rotate forward and reverse.

Record the sign/direction.

### Step 4 — Measure wheel movement

Move the wheelchair a known distance under controlled conditions.

Compare:

```text
Encoder-estimated distance
vs
Measured physical distance
```

### Step 5 — Calibrate

Determine the experimentally verified conversion factor.

---

# 25. Encoder Fault Handling

Possible faults:

```text
ENCODER_NOT_INITIALIZED
ENCODER_SIGNAL_INVALID
ENCODER_NO_ACTIVITY
ENCODER_OVERFLOW
ENCODER_CONFIGURATION_ERROR
ENCODER_HARDWARE_FAILURE
```

Fault handling:

```text
Encoder fault
     │
     ▼
Encoder Manager
     │
     ▼
Fault Manager
     │
     ▼
Safety Manager
     │
     ▼
Determine severity
```

The encoder driver should not independently force the entire system into `FAULT`.

---

# 26. Counter Overflow

The firmware must define how encoder count overflow is handled.

Using a sufficiently wide signed counter is recommended for long-running cumulative measurements.

```cpp
int64_t count;
```

If a smaller hardware counter is used internally, the firmware must safely extend it into a wider software counter.

Overflow behavior must be tested.

---

# 27. Timing

Encoder acquisition must remain reliable at the maximum expected pulse rate.

The implementation must account for:

- Encoder PPR
- Quadrature decoding mode
- Maximum wheel RPM
- Mechanical ratio if applicable
- ESP32 interrupt/hardware capability

The maximum expected pulse frequency should be calculated after the encoder mounting and mechanical relationship are known.

---

# 28. Simulation

The encoder interface must support simulated counts.

```text
             Encoder Interface
                    │
             ┌──────┴──────┐
             ▼             ▼
        Real Encoder   Sim Encoder
             │             │
             └──────┬──────┘
                    ▼
             Encoder Manager
```

Simulation should support:

- Forward counts
- Reverse counts
- Different wheel speeds
- Stationary state
- Left/right speed differences
- Missing pulses
- Invalid direction
- Fault conditions

---

# 29. Testing

## Electrical

- [ ] Supply voltage verified
- [ ] Ground verified
- [ ] A/B/Z voltage levels verified
- [ ] ESP32 input compatibility verified

## Signal

- [ ] Channel A detected
- [ ] Channel B detected
- [ ] Channel Z detected
- [ ] Quadrature relationship verified
- [ ] Direction verified

## Counting

- [ ] Known revolution count
- [ ] Forward counting
- [ ] Reverse counting
- [ ] Long-duration counting
- [ ] High-speed counting

## Mechanical

- [ ] Encoder mounting verified
- [ ] Wheel relationship verified
- [ ] Mechanical ratio established if applicable
- [ ] Wheel circumference measured

## Software

- [ ] Driver initialization
- [ ] Sensor Manager integration
- [ ] Health monitoring
- [ ] Telemetry
- [ ] Pi protocol
- [ ] Simulation
- [ ] Fault handling

---

# 30. Acceptance Criteria

The V1 encoder interface is ready for integration when:

- [ ] Both encoder interfaces can be initialized.
- [ ] A/B signals are captured reliably.
- [ ] Direction is experimentally verified.
- [ ] Z/index is captured.
- [ ] Counts are timestamped.
- [ ] Encoder health is reported.
- [ ] Faults are detectable.
- [ ] Raw counts are available through the Pi protocol.
- [ ] Simulation works.
- [ ] Electrical signal compatibility is verified.
- [ ] Maximum expected pulse rate is tested.
- [ ] Mechanical mounting is documented.
- [ ] Distance conversion is calibrated before being used as a validated measurement.
- [ ] No encoder logic directly controls wheelchair motors.

---

# 31. Implementation Order

```text
Encoder interface
       ↓
GPIO / hardware acquisition
       ↓
A/B quadrature detection
       ↓
Direction detection
       ↓
Z/index detection
       ↓
Counter management
       ↓
Health monitoring
       ↓
Encoder Manager
       ↓
Sensor Manager
       ↓
Telemetry
       ↓
Pi protocol
       ↓
Simulation
       ↓
Electrical testing
       ↓
Mechanical calibration
       ↓
Odometry
```

---

# 32. V1 Boundary

The V1 encoder path is:

```text
600 PPR ABZ Encoder
        ↓
ESP32
        ↓
Encoder Driver
        ↓
Encoder Manager
        ↓
Validated Counts / Direction
        ↓
Telemetry / Future Odometry
```

V1 does **not** claim:

- Validated wheel distance
- Validated wheel velocity
- Complete odometry
- Autonomous localization
- Sensor-fusion localization
- Motor control
- Automatic braking

Those capabilities require calibration and separate validation.