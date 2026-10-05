/**
 * @file hcsr04_driver.h
 * @brief AutoChair ESP32 — HC-SR04 Ultrasonic Sensor Driver
 *
 * Project:  Aivon Innovations — AutoChair
 * Platform: ESP32-WROOM-32D
 * Status:   IN DEVELOPMENT — SOFTWARE DRIVER (BENCH / SIMULATION SUPPORTED)
 *
 * Implements the concrete HC-SR04 driver according to SENSOR_INTERFACE.md §7-13.
 *
 * Key features:
 *   - ISensor base implementation.
 *   - Pulse duration to distance conversion using calibrated speed of sound (343 m/s).
 *   - Strict range validation (ULTRASONIC_MIN_MM to ULTRASONIC_MAX_MM).
 *   - Hardware-isolated via IUltrasonicIO.
 *   - Safe failure reporting: invalid distance defaults to -1.0f (never 0.0 mm).
 *   - Health tracking: consecutive failure counter, last success timestamp, fault escalation.
 *
 * SAFETY BOUNDARIES (MANDATORY):
 *   - GPIO assignments remain PLACEHOLDERS until wiring verification.
 *   - 5V ECHO logic level must be stepped down to 3.3V before physical connection.
 *   - A failing sensor reports SensorStatus::TIMEOUT/INVALID/FAULT and isHealthy() = false.
 */

#pragma once

#include "../sensor_interface.h"
#include "ultrasonic_io.h"
#include "autochair_config.h"
#include "autochair_types.h"

#include <cstdint>

namespace autochair {

/**
 * @brief Configuration parameters for a single HC-SR04 sensor instance.
 */
struct HCSR04Config {
    uint8_t  sensor_id                = 0;
    uint8_t  trig_pin                 = 0;        ///< Trigger GPIO (PLACEHOLDER).
    uint8_t  echo_pin                 = 0;        ///< Echo GPIO (PLACEHOLDER, requires 3.3V protection).
    float    min_distance_mm          = 20.0f;    ///< Minimum measurable distance (20 mm).
    float    max_distance_mm          = 4000.0f;  ///< Maximum valid range (4000 mm).
    uint32_t timeout_us               = 30000;    ///< Pulse timeout (~5.1 m max range).
    uint16_t max_consecutive_failures = 5;        ///< Threshold before escalating to FAULT.
};

/**
 * @brief Health metrics for an HC-SR04 sensor instance.
 */
struct HCSR04Health {
    uint32_t     last_success_ms      = 0;
    uint16_t     consecutive_failures = 0;
    uint32_t     total_measurements   = 0;
    uint32_t     successful_measurements = 0;
    SensorStatus status               = SensorStatus::UNINITIALIZED;
};

/**
 * @brief Concrete driver for HC-SR04 Ultrasonic Distance Sensor.
 */
class HCSR04Driver : public ISensor {
public:
    /**
     * @brief Construct a new HCSR04Driver.
     * @param config Configuration parameters.
     * @param io Optional hardware abstraction layer instance (default: internal hardware IO).
     */
    explicit HCSR04Driver(const HCSR04Config& config, IUltrasonicIO* io = nullptr);

    // -------------------------------------------------------------------------
    // ISensor Interface
    // -------------------------------------------------------------------------

    /**
     * @brief Initialize the driver and underlying GPIO via HAL.
     * @return true if initialized successfully; false if pins/HAL rejected.
     */
    bool begin() override;

    /**
     * @brief Execute one measurement cycle.
     *
     * Triggers pulse, measures echo, calculates distance, validates limits,
     * and updates reading and health metrics.
     */
    void update() override;

    /**
     * @brief Returns true if the sensor is initialized, valid, and recent reading was OK.
     */
    bool isHealthy() const override;

    /**
     * @brief Get the current SensorStatus.
     */
    SensorStatus getStatus() const override;

    // -------------------------------------------------------------------------
    // Accessors
    // -------------------------------------------------------------------------

    /**
     * @brief Get the latest UltrasonicReading.
     */
    const UltrasonicReading& getReading() const { return _reading; }

    /**
     * @brief Get sensor health metrics.
     */
    const HCSR04Health& getHealth() const { return _health; }

    /**
     * @brief Get driver configuration.
     */
    const HCSR04Config& getConfig() const { return _config; }

    /**
     * @brief Set or replace the IO abstraction layer (for testing or simulation).
     */
    void setIO(IUltrasonicIO* io);

    /**
     * @brief Reset health metrics and reading state.
     */
    void reset();

    // -------------------------------------------------------------------------
    // Conversion utilities
    // -------------------------------------------------------------------------

    /**
     * @brief Convert echo pulse duration (microseconds) to distance (millimeters).
     * Speed of sound = 343 m/s = 0.343 mm/us.
     * Round-trip distance = (duration_us * 0.343) / 2 = duration_us * 0.1715.
     */
    static constexpr float pulseUsToDistanceMm(uint32_t durationUs) {
        return static_cast<float>(durationUs) * 0.1715f;
    }

    /**
     * @brief Convert distance (millimeters) to round-trip echo pulse duration (microseconds).
     */
    static constexpr uint32_t distanceMmToPulseUs(float distanceMm) {
        return static_cast<uint32_t>(distanceMm / 0.1715f);
    }

private:
    HCSR04Config         _config;
    IUltrasonicIO*       _io;
    HardwareUltrasonicIO _defaultHardwareIO;
    UltrasonicReading    _reading{};
    HCSR04Health         _health{};
    bool                 _initialized = false;

    void handleFailure(SensorStatus failureStatus);
    void handleSuccess(float distanceMm);
};

}  // namespace autochair
