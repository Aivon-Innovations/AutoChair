# `WHEELCHAIR_INTERFACE.md`

**Project:** AutoChair – AI-Powered Smart Wheelchair  
**Company:** Aivon Innovations Pvt. Ltd.  
**Platform:** ESP32-WROOM-32D  
**Version:** V1.0  
**Status:** Development / Hardware Interface TBD

---

# 1. Purpose

This document defines the software architecture for connecting the AutoChair ESP32 subsystem to the existing powered wheelchair.

The purpose of this interface is to provide a **hardware-independent abstraction** so that the ESP32 firmware can eventually communicate with the verified wheelchair controller without requiring changes to the sensor, safety, state-machine, or communication architecture.

---

# 2. Critical Hardware Boundary

The existing wheelchair interface is **not yet sufficiently verified for direct ESP32 control**.

Therefore this document does **not** define an electrical motor-control implementation.

The following must not be assumed:

- Motor-controller input type
- Motor PWM interface
- Direction-control signals
- Controller enable signal
- Brake-control signal
- Joystick protocol meaning
- Safe stopping mechanism
- Controller voltage/current requirements
- Whether the existing controller accepts external commands

Physical wheelchair movement must remain disabled until the interface is identified and experimentally validated.

---

# 3. Existing Wheelchair

The inspected wheelchair is:

```text
Manufacturer: HERO ECO / Hero Eco Med
Model: MHL 1007-A
```

Inspection established:

- Two DC motors
- 250 W motor rating ×2
- Two 12 V, 14 Ah sealed lead-acid batteries
- Batteries connected in series
- 24 V nominal battery system
- Existing joystick/controller assembly
- Electromagnetic braking stated in the wheelchair documentation
- Manual braking mechanism also physically identified

The ESP32 must not directly assume that any of these components expose an external control interface.

---

# 4. Architectural Position

```text
                  Raspberry Pi
                       │
                High-level command
                       │
                       ▼
                    ESP32
                       │
               Safety / State
                       │
                       ▼
             Wheelchair Interface
                       │
             ┌─────────┴─────────┐
             │                   │
       Verified Controller   Future Interface
             │
             ▼
      Existing Wheelchair
             │
       ┌─────┴─────┐
       ▼           ▼
    Left Motor   Right Motor
```

The Wheelchair Interface is an abstraction layer between the ESP32 firmware and the physical wheelchair controller.

---

# 5. Interface Principle

Higher-level software should interact with an abstract interface:

```cpp
class IWheelchairInterface {
public:
    virtual bool initialize() = 0;
    virtual bool enable() = 0;
    virtual bool disable() = 0;
    virtual bool stop() = 0;
    virtual bool getStatus() = 0;
};
```

Movement-related methods must remain disabled or unimplemented until the physical interface is verified.

---

# 6. Current Implementation State

For V1:

```text
WheelchairInterface
        │
        ▼
   NOT CONNECTED
        │
        ▼
Simulation / Stub
```

The ESP32 can implement the interface as a simulated or non-driving subsystem.

Example:

```text
ENABLE
   ↓
Software state only

STOP
   ↓
Software state only
```

No GPIO output should be connected to the wheelchair controller merely because the software interface exists.

---

# 7. Hardware Interface Investigation

Before implementing physical control, the following must be established:

```text
1. Existing controller manufacturer/model
2. Controller electrical specifications
3. Motor-controller input interface
4. Joystick/controller communication
5. Enable/disable mechanism
6. Brake architecture
7. Safe-stop behavior
8. Signal voltage levels
9. Signal timing
10. Electrical isolation requirements
11. Controller fault behavior
12. External control feasibility
```

Only verified information should be added to the physical implementation.

---

# 8. Existing Joystick Interface

The inspected joystick assembly has a 9-pin interface.

Known observed labels:

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

Observed measurements:

```text
Pin 1 → 3.301 V
Pin 2 → 3.281 V
Pin 3 → 3.276 V
Pin 4 → 3.299 V
Pin 5 → 0 V
Pin 6 → approximately 1.203–1.445 V
Pin 7 → approximately 1.984 V
Pin 8 → 3.088 V
Pin 9 → 4.955 V
```

These values describe the inspected hardware and must not be interpreted as permission to connect the ESP32 directly.

---

# 9. Observed Joystick Communication

Passive logic-analyzer observation identified:

```text
SCL/SDA:
approximately 75 kHz
I²C address observed: 0x0C
```

The joystick also showed clock/data-like activity on:

```text
Pin 7 → 16_CLK
Pin 6 → 16_DIO
```

This was observed as SPI-compatible/SPI-like behavior.

A dedicated chip-select signal has not been established.

Therefore the interface must **not** currently be documented as confirmed standard SPI.

---

# 10. Joystick Protocol Status

Observed traffic included:

```text
0x46
0x36
0x01
0x21
```

However:

> The exact semantic meaning of these bytes has not been established.

Movement-specific mapping has also not been reliably decoded.

Therefore the ESP32 must not assume:

```text
byte → forward
byte → reverse
byte → left
byte → right
```

until experimentally verified.

---

# 11. Passive Observation Boundary

The current joystick investigation is:

```text
Joystick
   │
   ▼
Passive observation
   │
   ▼
Logic analyzer
   │
   ▼
Protocol analysis
```

No signal injection is permitted as part of the current observation phase.

The ESP32 may eventually be used as a passive monitoring device only after the electrical interface and protection requirements are verified.

---

# 12. Future Wheelchair Interface

Once the controller interface is verified, a physical implementation may follow:

```text
                IWheelchairInterface
                         │
             ┌───────────┴───────────┐
             │                       │
        Simulated                Physical
        Interface                Interface
                                     │
                                     ▼
                            Verified Controller
```

The physical implementation must be replaceable without modifying:

- Sensor drivers
- Sensor Manager
- Safety Manager
- State Machine
- Pi Protocol
- Diagnostics

---

# 13. Future Motion Abstraction

A future interface may expose:

```cpp
struct MotionCommand {
    float speed;
    float direction;
};
```

Possible future functions:

```cpp
setMotion(...)
stop()
getMotionStatus()
```

These are architectural placeholders only.

They are not currently connected to the wheelchair motors.

---

# 14. Safety Gate

Every future physical movement command must pass through:

```text
Raspberry Pi command
        │
        ▼
ESP32 Protocol
        │
        ▼
Command Validation
        │
        ▼
State Machine
        │
        ▼
Safety Manager
        │
        ▼
Wheelchair Interface
        │
        ▼
Verified Controller
```

The Wheelchair Interface must never bypass the Safety Manager.

---

# 15. Movement Authorization

A physical movement request should eventually require all of the following:

```text
System state permits operation
        AND
Valid operating mode
        AND
Safety state = CLEAR
        AND
No blocking fault
        AND
Heartbeat healthy
        AND
Wheelchair interface healthy
        AND
Command valid
```

Only then may the physical interface consider executing the command.

---

# 16. Stop Behavior

The software architecture defines:

```text
STOP
 ↓
Cancel operation
 ↓
Revoke operational authorization
 ↓
Wheelchair Interface.stop()
```

However:

> `WheelchairInterface.stop()` must not be implemented as a physical controller signal until the actual controller's stop mechanism is verified.

If no verified electronic stop interface exists, the software must not claim that the command physically brakes the wheelchair.

---

# 17. Brake Interface

The wheelchair documentation indicates electromagnetic braking.

The exact electrical implementation has not been established sufficiently for ESP32 control.

Therefore V1 must not define:

```text
BRAKE_ON GPIO
BRAKE_OFF GPIO
```

as actual hardware outputs.

The brake architecture must first be investigated and verified.

---

# 18. Enable / Disable

Similarly, the following are currently abstractions:

```text
enable()
disable()
```

They represent **software authorization** in V1.

They do not currently mean:

```text
Enable motor controller
Disable motor controller
```

unless the corresponding physical electrical interface has been identified and validated.

---

# 19. Electrical Isolation

If a future external controller interface is identified, the electrical design must determine whether isolation is required.

Potential considerations include:

- Logic-level compatibility
- Ground reference
- Voltage compatibility
- Current requirements
- Noise
- Ground loops
- Protection
- Isolation
- Fail-safe behavior

No direct GPIO connection should be made until these characteristics are verified.

---

# 20. Interface Health

A future physical interface should report:

```text
INITIALIZING
READY
ACTIVE
STOPPING
DISABLED
FAULT
UNKNOWN
```

Example:

```cpp
enum class WheelchairInterfaceStatus {
    UNAVAILABLE,
    INITIALIZING,
    READY,
    ACTIVE,
    STOPPING,
    DISABLED,
    FAULT
};
```

For V1:

```text
status = UNAVAILABLE
```

or `SIMULATED` when running the software test implementation.

---

# 21. Interface Faults

Future physical-interface faults may include:

```text
CONTROLLER_NOT_RESPONDING
INVALID_CONTROLLER_RESPONSE
CONTROLLER_FAULT
BRAKE_INTERFACE_FAULT
ENABLE_FAILURE
STOP_FAILURE
COMMUNICATION_FAILURE
SIGNAL_ERROR
```

These must be defined only after the physical controller interface is understood.

---

# 22. Simulation

The Wheelchair Interface must support a simulated implementation.

```text
              IWheelchairInterface
                      │
               ┌──────┴──────┐
               ▼             ▼
          Simulated       Physical
          Interface       Interface
```

The simulator can emulate:

```text
ENABLE
DISABLE
STOP
MOTION
FAULT
CONTROLLER_STATUS
```

This allows the rest of the ESP32 architecture to be developed without moving the wheelchair.

---

# 23. Development Mode

A development configuration should explicitly identify whether physical wheelchair control is available.

Example:

```cpp
enum class WheelchairInterfaceMode {
    DISABLED,
    SIMULATION,
    PHYSICAL
};
```

Default V1:

```text
DISABLED
```

or:

```text
SIMULATION
```

Physical mode must not be enabled accidentally.

---

# 24. Configuration Safety

Physical wheelchair control should require an explicit configuration change.

For example:

```text
WHEELCHAIR_INTERFACE = SIMULATION
```

rather than silently defaulting to physical control.

The firmware should report the active interface mode through diagnostics and `GET_STATUS`.

---

# 25. Testing Strategy

Testing must proceed in stages:

```text
Simulation
    ↓
Electrical interface identification
    ↓
Bench-level interface testing
    ↓
Controller response verification
    ↓
Stop behavior verification
    ↓
Brake behavior verification
    ↓
Controlled wheelchair testing
```

The first physical tests must be performed with the wheelchair secured and under controlled conditions appropriate to the identified interface.

---

# 26. Do Not Do

Until the physical controller interface is verified, do not:

- Apply arbitrary voltage to controller pins.
- Inject joystick signals.
- Generate unknown PWM signals.
- Assume a GPIO pin is a controller input.
- Assume the brake can be software-controlled.
- Bypass the existing joystick/controller.
- Connect ESP32 outputs directly to unknown wheelchair wiring.
- Perform uncontrolled movement tests.
- Treat software `STOP` as physical braking.

---

# 27. Acceptance Criteria

The Wheelchair Interface V1 architecture is complete when:

- [ ] Hardware-independent interface exists.
- [ ] Simulation implementation exists.
- [ ] Physical implementation is isolated behind the interface.
- [ ] Safety Manager gates physical commands.
- [ ] State Machine gates physical commands.
- [ ] Physical controller interface is explicitly marked TBD.
- [ ] Brake interface is explicitly marked TBD.
- [ ] Enable/disable interface is explicitly marked TBD.
- [ ] Joystick protocol remains passive-observation-only.
- [ ] No unverified GPIO/PWM output controls the wheelchair.
- [ ] Physical integration requires separate verification and testing.

---

# 28. Implementation Order

```text
Interface definition
        ↓
Simulation implementation
        ↓
State Machine integration
        ↓
Safety Manager integration
        ↓
Pi Protocol integration
        ↓
Diagnostics
        ↓
Wheelchair controller investigation
        ↓
Electrical interface verification
        ↓
Physical interface implementation
        ↓
Bench testing
        ↓
Controlled movement testing
```

---

# 29. V1 Architecture Boundary

The current implementation is:

```text
Raspberry Pi
      │
      ▼
ESP32
      │
 ┌────┴─────────────┐
 │                  │
Safety          Wheelchair
Manager         Interface
 │                  │
 │             SIMULATION
 │                  │
 └────────┬─────────┘
          ▼
       No physical
       motor control
```

The physical wheelchair interface will be added only after the existing controller, braking system, and external control interface have been verified.

---

# 30. Final Principle

> **The software architecture must be ready for wheelchair control before the wheelchair is allowed to be controlled by the software.**

The interface therefore exists now as an abstraction and simulation layer, while physical motor/control integration remains a separate engineering and safety-validation stage.