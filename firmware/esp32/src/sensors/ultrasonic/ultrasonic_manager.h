/**
 * @file ultrasonic_manager.h
 * @brief AutoChair ESP32 — Ultrasonic Sensor Manager
 *
 * Project:  Aivon Innovations — AutoChair
 * Platform: ESP32-WROOM-32D
 * Status:   IN DEVELOPMENT
 *
 * Manages the array of HC-SR04 ultrasonic sensors according to SENSOR_INTERFACE.md §11-13.
 *
 * Key features:
 *   - Sequential scheduling to eliminate acoustic interference between adjacent sensors.
 *   - Aggregated validated distance readings.
 *   - Aggregate and per-sensor health monitoring.
 *   - Zero dynamic memory allocation (static array storage).
 */

#pragma once

#include "hcsr04_driver.h"
#include "autochair_config.h"

#include <cstdint>
#include <cstddef>

namespace autochair {

class UltrasonicManager {
public:
    UltrasonicManager();

    /**
     * @brief Initialize all configured ultrasonic sensor drivers.
     * @return true if manager initialized.
     */
    bool begin();

    /**
     * @brief Execute sequential sensor acquisition cycle.
     *
     * Sequentially samples the active sensors to prevent cross-talk.
     */
    void update();

    /**
     * @brief Get reading for a specific sensor index (0 to ULTRASONIC_COUNT - 1).
     */
    const UltrasonicReading& getReading(uint8_t index) const;

    /**
     * @brief Get driver instance for a specific sensor index.
     */
    HCSR04Driver& getDriver(uint8_t index);

    /**
     * @brief Get driver instance (const) for a specific sensor index.
     */
    const HCSR04Driver& getDriver(uint8_t index) const;

    /**
     * @brief Check if all active sensors are healthy.
     */
    bool areAllHealthy() const;

    /**
     * @brief Count of configured active sensors.
     */
    static constexpr uint8_t getSensorCount() {
        return config::ULTRASONIC_COUNT;
    }

    /**
     * @brief Find the minimum valid distance among all active sensors.
     * @return Minimum distance in mm, or -1.0f if no valid readings exist.
     */
    float getMinValidDistanceMm() const;

    /**
     * @brief Get currently scheduled round-robin sensor index (0 to ULTRASONIC_COUNT - 1).
     */
    uint8_t getCurrentSensorIndex() const {
        return _currentSensorIndex;
    }

private:
    HCSR04Driver _drivers[config::ULTRASONIC_COUNT];
    uint8_t      _currentSensorIndex = 0;
};

}  // namespace autochair
