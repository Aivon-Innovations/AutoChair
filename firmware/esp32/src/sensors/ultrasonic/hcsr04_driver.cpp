/**
 * @file hcsr04_driver.cpp
 * @brief AutoChair ESP32 — HC-SR04 single-sensor driver implementation
 *
 * Project:  Aivon Innovations — AutoChair
 * Platform: ESP32-WROOM-32D
 * Branch:   alvira-dev
 * Status:   IN DEVELOPMENT
 */

#include "sensors/ultrasonic/hcsr04_driver.h"

// Hardware GPIO setup is only available in the ESP32 build.
#ifndef ENV_NATIVE
#include <Arduino.h>
#endif

namespace autochair {

// =============================================================================
// Construction and initialisation
// =============================================================================

HCSR04Driver::HCSR04Driver(const HCSR04Config& cfg, UltrasonicIO* io) {
    init(cfg, io);
}

void HCSR04Driver::init(const HCSR04Config& cfg, UltrasonicIO* io) {
    _cfg = cfg;
    _io  = io;
    _consec_failures = 0;
    _resetReading();
}

void HCSR04Driver::setIO(UltrasonicIO* io) {
    _io = io;
}

// =============================================================================
// Lifecycle
// =============================================================================

void HCSR04Driver::begin() {
    _consec_failures = 0;
    _resetReading();

#ifndef ENV_NATIVE
    // Configure GPIO pins on hardware only.
    pinMode(_cfg.trig_pin, OUTPUT);
    digitalWrite(_cfg.trig_pin, LOW);
    pinMode(_cfg.echo_pin, INPUT);
#endif
}

void HCSR04Driver::reset() {
    _consec_failures = 0;
    _resetReading();
    // NOTE: Does not reconfigure GPIO — call begin() again if GPIO reset is needed.
}

// =============================================================================
// Measurement
// =============================================================================

void HCSR04Driver::update() {
    // Do not re-measure a faulted sensor; caller must reset() first.
    if (_reading.status == SensorStatus::FAULT) {
        return;
    }

    // Require a valid IO before attempting any measurement.
    if (_io == nullptr) {
        return;
    }

    const uint32_t now_ms   = _io->getMillis();
    const uint32_t pulse_us = _io->measurePulseUs(_cfg.trig_pin,
                                                   _cfg.echo_pin,
                                                   _cfg.timeout_us);

    _reading.timestamp_ms = now_ms;

    if (pulse_us == 0) {
        // No echo received → timeout.
        _reading.distance_mm  = -1.0f;
        _reading.status       = SensorStatus::TIMEOUT;
        _consec_failures++;

        // Escalate to FAULT after exceeding the allowed failure budget.
        if (_consec_failures > _cfg.max_consecutive_failures) {
            _reading.status = SensorStatus::FAULT;
        }
    } else {
        // Valid echo — convert pulse to distance.
        // distance_mm = (pulse_us * speed_of_sound_mm_per_us) / 2
        // Using 0.3430 mm/µs (speed of sound at ~20 °C).
        _reading.distance_mm  = (static_cast<float>(pulse_us) * 0.3430f) / 2.0f;
        _reading.status       = SensorStatus::OK;
        _consec_failures      = 0;
    }
}

// =============================================================================
// Accessors
// =============================================================================

const UltrasonicReading& HCSR04Driver::getReading() const {
    return _reading;
}

bool HCSR04Driver::isHealthy() const {
    return _reading.status != SensorStatus::FAULT;
}

// =============================================================================
// Private helpers
// =============================================================================

void HCSR04Driver::_resetReading() {
    _reading.sensor_id    = _cfg.sensor_id;
    _reading.distance_mm  = -1.0f;
    _reading.status       = SensorStatus::UNINITIALIZED;
    _reading.timestamp_ms = 0;
}

}  // namespace autochair
