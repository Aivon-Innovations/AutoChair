/**
 * @file wheelchair_interface.h
 * @brief AutoChair ESP32 — Wheelchair Interface abstraction
 *
 * Project:  Aivon Innovations — AutoChair
 * Platform: ESP32-WROOM-32D
 * Status:   IN DEVELOPMENT — SOFTWARE ABSTRACTION ONLY
 *
 * =========================================================================
 * V1 SAFETY BOUNDARY — READ BEFORE MODIFYING
 * =========================================================================
 *
 * This file defines a SOFTWARE ABSTRACTION ONLY.
 *
 * The following have NOT been verified for the Hero Eco Med MHL 1007-A:
 *   - Motor-controller input type / protocol
 *   - Motor PWM interface
 *   - Direction-control signals
 *   - Controller enable signal
 *   - Brake-control signal
 *   - Joystick/controller protocol byte meanings
 *   - Safe stopping mechanism
 *   - Controller voltage/current requirements
 *   - Whether the existing controller accepts external commands at all
 *
 * Therefore:
 *   - No GPIO or PWM output may be connected to the wheelchair controller.
 *   - No signal may be injected into the joystick interface.
 *   - The physical implementation (WheelchairInterfaceMode::PHYSICAL)
 *     is DISABLED in V1 and must remain so until the above are verified.
 *
 * Default V1 mode: SIMULATION (software state changes only, no hardware).
 *
 * (WHEELCHAIR_INTERFACE.md §1, §2, §5, §6, §26)
 * =========================================================================
 */

#pragma once

#include "autochair_types.h"
#include <cstdint>

#ifdef DISABLED
#  undef DISABLED
#endif

namespace autochair {

/**
 * @brief Abstract wheelchair interface.
 *
 * All methods are SOFTWARE ABSTRACTIONS only in V1.
 * Physical implementations must not be added until the controller interface
 * has been separately identified, verified, and safety-validated.
 */
class IWheelchairInterface {
public:
    virtual ~IWheelchairInterface() = default;

    virtual bool initialize() = 0;
    virtual bool enable()     = 0;
    virtual bool disable()    = 0;
    virtual bool stop()       = 0;

    virtual WheelchairInterfaceStatus getStatus() const = 0;
    virtual WheelchairInterfaceMode   getMode()   const = 0;
};

// =============================================================================
// V1 Simulation implementation
// =============================================================================

/**
 * @brief Simulated wheelchair interface — no GPIO, no PWM, no hardware output.
 *
 * All method calls change software state only.
 * This is the ONLY valid V1 implementation.
 *
 * The diagnostic output reports "SIMULATION" so the Raspberry Pi can verify
 * that physical control is not active.  (WHEELCHAIR_INTERFACE.md §24)
 */
class WheelchairInterfaceSimulation : public IWheelchairInterface {
public:
    WheelchairInterfaceSimulation()
        : _status(WheelchairInterfaceStatus::UNAVAILABLE),
          _mode(WheelchairInterfaceMode::SIMULATION) {}

    bool initialize() override {
        // Software state change only — no hardware output.
        _status = WheelchairInterfaceStatus::READY;
        return true;
    }

    bool enable() override {
        // Software state change only.
        if (_status == WheelchairInterfaceStatus::READY) {
            _status = WheelchairInterfaceStatus::ACTIVE;
            return true;
        }
        return false;
    }

    bool disable() override {
        // Software state change only.
        _status = WheelchairInterfaceStatus::DISABLED;
        return true;
    }

    bool stop() override {
        // Software state change only.
        // NOTE: This does NOT physically stop the wheelchair.
        // Physical stopping requires a verified hardware interface.
        _status = WheelchairInterfaceStatus::STOPPING;
        return true;
    }

    WheelchairInterfaceStatus getStatus() const override { return _status; }
    WheelchairInterfaceMode   getMode()   const override { return _mode;   }

private:
    WheelchairInterfaceStatus _status;
    WheelchairInterfaceMode   _mode;
};

}  // namespace autochair
