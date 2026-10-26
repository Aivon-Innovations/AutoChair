/**
 * @file hcsr04_driver.h
 * @brief AutoChair ESP32 — HC-SR04 single-sensor driver
 *
 * Project:  Aivon Innovations — AutoChair
 * Platform: ESP32-WROOM-32D
 * Branch:   alvira-dev
 * Status:   IN DEVELOPMENT
 *
 * Manages one HC-SR04 sensor: trigger, measure, validate, track failures.
 * Hardware IO is injected via UltrasonicIO to allow native-host testing.
 *
 * Fault policy:
 *   Each consecutive timeout increments a failure counter.
 *   After max_consecutive_failures+1 failures the status escalates to FAULT.
 *   A FAULT driver stops updating until reset() is called.
 *   A valid reading resets the failure counter.
 */

#pragma once

#include "autochair_config.h"
#include "autochair_types.h"
#include "sensors/sensor_interface.h"

#include <cstdint>

namespace autochair {

// =============================================================================
// HCSR04Config
// =============================================================================

/**
 * @brief Compile-time / init-time configuration for one HC-SR04 channel.
 */
struct HCSR04Config {
    uint8_t  sensor_id               = 0;
    uint8_t  trig_pin                = 0;
    uint8_t  echo_pin                = 0;
    uint32_t timeout_us              = config::ULTRASONIC_TIMEOUT_US;
    uint16_t max_consecutive_failures = config::ULTRASONIC_MAX_FAILURES;
};

// =============================================================================
// HCSR04Driver
// =============================================================================

/**
 * @brief Single HC-SR04 driver with injectable IO and failure tracking.
 *
 * Usage (hardware):
 *   HCSR04Config cfg;
 *   cfg.sensor_id = 0; cfg.trig_pin = 4; cfg.echo_pin = 34;
 *   HardwareUltrasonicIO hw;
 *   HCSR04Driver driver(cfg, &hw);
 *   driver.begin();
 *   driver.update();  // called once per scheduler tick
 *   auto& r = driver.getReading();
 *
 * Usage (tests):
 *   MockUltrasonicIO mock;
 *   HCSR04Driver driver(cfg, &mock);
 */
class HCSR04Driver {
public:
    /**
     * @brief Construct with explicit config and IO pointer.
     * @param cfg  Sensor configuration (pin, timeout, fault threshold).
     * @param io   IO abstraction — must remain valid for the driver's lifetime.
     *             May be nullptr; update() will no-op until a non-null IO is set.
     */
    HCSR04Driver(const HCSR04Config& cfg, UltrasonicIO* io);

    /**
     * @brief Default constructor — leaves driver uninitialised.
     *
     * Must be followed by init() before use. Used by UltrasonicManager to
     * create its internal driver array before configs are available.
     */
    HCSR04Driver() = default;

    /**
     * @brief Initialise or reinitialise the driver with a new config and IO.
     *
     * Resets all state, including the failure counter and reading.
     */
    void init(const HCSR04Config& cfg, UltrasonicIO* io);

    /**
     * @brief Inject or replace the IO implementation.
     *
     * Called by tests to substitute a MockUltrasonicIO, or by the manager
     * to inject the hardware IO after construction.
     */
    void setIO(UltrasonicIO* io);

    /**
     * @brief Prepare GPIO pins (hardware-only) and reset internal state.
     *
     * Must be called once before the first update(). On native (ENV_NATIVE)
     * this is a no-op except for resetting the reading to UNINITIALIZED.
     */
    void begin();

    /**
     * @brief Trigger the sensor and update the stored reading.
     *
     * No-op if IO is nullptr or status is FAULT.
     * Call once per scheduler tick (one sensor per tick in round-robin).
     */
    void update();

    /**
     * @brief Reset driver to UNINITIALIZED state.
     *
     * Clears failure counter and stored reading. Does NOT reconfigure GPIO.
     * Used after a FAULT to attempt recovery.
     */
    void reset();

    /**
     * @brief Return the most recent reading (const reference).
     */
    const UltrasonicReading& getReading() const;

    /**
     * @brief Return false if the driver has entered the FAULT state.
     */
    bool isHealthy() const;

private:
    HCSR04Config      _cfg{};
    UltrasonicIO*     _io             = nullptr;
    UltrasonicReading _reading{};
    uint16_t          _consec_failures = 0;

    void _resetReading();
};

}  // namespace autochair
