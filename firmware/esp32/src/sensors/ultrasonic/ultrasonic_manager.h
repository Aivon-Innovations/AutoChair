/**
 * @file ultrasonic_manager.h
 * @brief AutoChair ESP32 — Six-sensor ultrasonic round-robin manager
 *
 * Project:  Aivon Innovations — AutoChair
 * Platform: ESP32-WROOM-32D
 * Branch:   alvira-dev
 * Status:   IN DEVELOPMENT
 *
 * Owns the six HCSR04Driver instances and advances them one per update() call
 * in strict round-robin order (0 → 1 → 2 → 3 → 4 → 5 → 0 → …).
 *
 * Scheduling behaviour:
 *   - Exactly ONE sensor is triggered per update() call.
 *   - A FAULT driver is still visited in the rotation (its update() is a no-op),
 *     so the scheduler never blocks and other sensors continue receiving updates.
 *   - Update cadence: caller controls timing (ULTRASONIC_CYCLE_INTERVAL_MS).
 *
 * IO injection for tests:
 *   UltrasonicManager mgr;
 *   mgr.getDriver(i).setIO(&mockIO);
 *   mgr.begin();
 *   mgr.update();
 */

#pragma once

#include "autochair_config.h"
#include "autochair_types.h"
#include "sensors/ultrasonic/hcsr04_driver.h"

#include <cstdint>

namespace autochair {

class UltrasonicManager {
public:
    /**
     * @brief Construct and initialise all six drivers from autochair_config.h.
     *
     * Each driver is configured with the correct sensor_id, TRIG/ECHO pins,
     * timeout, and failure threshold from the validated pin map. IO is set
     * to nullptr; tests inject mocks via getDriver(i).setIO(&mock).
     * Hardware builds inject HardwareUltrasonicIO via begin().
     */
    UltrasonicManager();

    /**
     * @brief Call begin() on all drivers (GPIO setup on hardware; reset on native).
     *
     * On the ESP32 target, this also injects a HardwareUltrasonicIO into each
     * driver that still has a nullptr IO. On native tests, hardware IOs are not
     * injected; callers must have already injected mocks.
     */
    void begin();

    /**
     * @brief Advance one sensor in the round-robin and update it.
     *
     * Call once per scheduler tick. Each six calls complete one full cycle
     * across all sensors.
     */
    void update();

    /**
     * @brief Get the most recent reading for sensor at 0-based index.
     *
     * If index >= ULTRASONIC_COUNT, returns a sentinel reading with
     * sensor_id = 0xFF and status = INVALID.
     */
    const UltrasonicReading& getReading(uint8_t index) const;

    /**
     * @brief Direct access to a driver by 0-based index (for IO injection and testing).
     *
     * Undefined behaviour if index >= ULTRASONIC_COUNT.
     */
    HCSR04Driver& getDriver(uint8_t index);

private:
    HCSR04Driver      _drivers[config::ULTRASONIC_COUNT];
    uint8_t           _current = 0;
    UltrasonicReading _invalid_reading{};  // Sentinel for out-of-range access
};

}  // namespace autochair
