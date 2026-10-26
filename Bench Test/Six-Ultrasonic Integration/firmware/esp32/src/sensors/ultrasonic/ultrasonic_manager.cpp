/**
 * @file ultrasonic_manager.cpp
 * @brief AutoChair ESP32 — Six-sensor ultrasonic round-robin manager implementation
 *
 * Project:  Aivon Innovations — AutoChair
 * Platform: ESP32-WROOM-32D
 * Branch:   alvira-dev
 * Status:   IN DEVELOPMENT
 */

#include "sensors/ultrasonic/ultrasonic_manager.h"

// Hardware IO is only compiled for the ESP32 target.
#ifndef ENV_NATIVE
#include "sensors/hardware_ultrasonic_io.h"

static autochair::HardwareUltrasonicIO s_hw_io[autochair::config::ULTRASONIC_COUNT];
#endif

namespace autochair {

// =============================================================================
// Construction
// =============================================================================

UltrasonicManager::UltrasonicManager() {
    // Sentinel reading returned for out-of-range index queries.
    _invalid_reading.sensor_id    = 0xFF;
    _invalid_reading.distance_mm  = -1.0f;
    _invalid_reading.status       = SensorStatus::INVALID;
    _invalid_reading.timestamp_ms = 0;

    // Initialise each driver with its configuration from autochair_config.h.
    // IO is left as nullptr; injected by tests or by begin() on hardware.
    for (uint8_t i = 0; i < config::ULTRASONIC_COUNT; ++i) {
        HCSR04Config cfg{};
        cfg.sensor_id               = i;
        cfg.trig_pin                = config::US_TRIG_PIN[i];
        cfg.echo_pin                = config::US_ECHO_PIN[i];
        cfg.timeout_us              = config::ULTRASONIC_TIMEOUT_US;
        cfg.max_consecutive_failures = config::ULTRASONIC_MAX_FAILURES;
        _drivers[i].init(cfg, nullptr);
    }

    _current = 0;
}

// =============================================================================
// Lifecycle
// =============================================================================

void UltrasonicManager::begin() {
#ifndef ENV_NATIVE
    // On hardware: inject HardwareUltrasonicIO into any driver that still
    // has a nullptr IO (i.e., not already overridden by a test mock).
    for (uint8_t i = 0; i < config::ULTRASONIC_COUNT; ++i) {
        // setIO only replaces if currently null — guarded in the driver
        // by checking nullptr before use; we unconditionally set here as
        // hardware is always the expected IO in the production path.
        _drivers[i].setIO(&s_hw_io[i]);
    }
#endif

    for (uint8_t i = 0; i < config::ULTRASONIC_COUNT; ++i) {
        _drivers[i].begin();
    }

    _current = 0;
}

// =============================================================================
// Scheduling
// =============================================================================

void UltrasonicManager::update() {
    // Trigger exactly one sensor per call, regardless of its health state.
    // FAULT drivers' update() is a no-op internally, so the scheduler
    // advances without blocking.
    _drivers[_current].update();

    // Advance round-robin index.
    _current = (_current + 1) % config::ULTRASONIC_COUNT;
}

// =============================================================================
// Accessors
// =============================================================================

const UltrasonicReading& UltrasonicManager::getReading(uint8_t index) const {
    if (index >= config::ULTRASONIC_COUNT) {
        return _invalid_reading;
    }
    return _drivers[index].getReading();
}

HCSR04Driver& UltrasonicManager::getDriver(uint8_t index) {
    // Callers are responsible for passing a valid index.
    // No bounds check here; undefined behaviour on out-of-range.
    return _drivers[index];
}

}  // namespace autochair
