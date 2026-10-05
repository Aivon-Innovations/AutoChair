/**
 * @file hcsr04_driver.cpp
 * @brief AutoChair ESP32 — HC-SR04 Ultrasonic Sensor Driver implementation
 *
 * Project:  Aivon Innovations — AutoChair
 * Platform: ESP32-WROOM-32D
 * Status:   IN DEVELOPMENT — SOFTWARE DRIVER
 */

#include "hcsr04_driver.h"
#include "diagnostics/logger.h"

#ifdef ENV_NATIVE
#  include <chrono>
static uint32_t currentMillis() {
    using namespace std::chrono;
    return static_cast<uint32_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count()
    );
}
#else
#  include <Arduino.h>
static inline uint32_t currentMillis() {
    return millis();
}
#endif

namespace autochair {

HCSR04Driver::HCSR04Driver(const HCSR04Config& config, IUltrasonicIO* io)
    : _config(config),
      _io(io ? io : &_defaultHardwareIO)
{
    _reading.sensor_id    = _config.sensor_id;
    _reading.timestamp_ms = 0;
    _reading.status       = SensorStatus::UNINITIALIZED;
    _reading.distance_mm  = -1.0f;

    _health.status        = SensorStatus::UNINITIALIZED;
}

void HCSR04Driver::setIO(IUltrasonicIO* io) {
    _io = (io != nullptr) ? io : &_defaultHardwareIO;
}

void HCSR04Driver::reset() {
    _health = HCSR04Health{};
    _health.status = _initialized ? SensorStatus::INITIALIZING : SensorStatus::UNINITIALIZED;

    _reading.status = _health.status;
    _reading.distance_mm = -1.0f;
    _reading.timestamp_ms = currentMillis();
}

bool HCSR04Driver::begin() {
    Logger::info("HCSR04Driver", "Initializing ultrasonic sensor driver");

    // Initialize IO pins via the HAL
    bool pinsOk = _io->initPins(_config.trig_pin, _config.echo_pin);
    if (!pinsOk) {
        // When pins are 0 (placeholder), the driver enters UNINITIALIZED state safely.
        Logger::warning("HCSR04Driver",
                        "Pins not initialized (placeholder / unverified). Software driver ready.");
        _initialized = false;
        _health.status = SensorStatus::UNINITIALIZED;
        _reading.status = SensorStatus::UNINITIALIZED;
        _reading.distance_mm = -1.0f;
        return false;
    }

    _initialized = true;
    _health.status = SensorStatus::INITIALIZING;
    _reading.status = SensorStatus::INITIALIZING;
    _reading.distance_mm = -1.0f;
    _reading.timestamp_ms = currentMillis();

    return true;
}

void HCSR04Driver::update() {
    _reading.timestamp_ms = currentMillis();
    _health.total_measurements++;

    if (!_initialized) {
        _reading.status = SensorStatus::UNINITIALIZED;
        _reading.distance_mm = -1.0f;
        _health.status = SensorStatus::UNINITIALIZED;
        return;
    }

    // Trigger measurement via HAL
    uint32_t durationUs = _io->measurePulseUs(_config.trig_pin,
                                              _config.echo_pin,
                                              _config.timeout_us);

    // Case 1: Timeout / No echo pulse received
    if (durationUs == 0) {
        handleFailure(SensorStatus::TIMEOUT);
        return;
    }

    // Case 2: Pulse duration exceeded maximum timeout
    if (durationUs > _config.timeout_us) {
        handleFailure(SensorStatus::TIMEOUT);
        return;
    }

    // Calculate distance
    float distanceMm = pulseUsToDistanceMm(durationUs);

    // Case 3: Out of valid measurement bounds (20 mm to 4000 mm)
    if (distanceMm < _config.min_distance_mm || distanceMm > _config.max_distance_mm) {
        handleFailure(SensorStatus::INVALID);
        return;
    }

    // Case 4: Valid measurement
    handleSuccess(distanceMm);
}

bool HCSR04Driver::isHealthy() const {
    return _initialized &&
           (_health.status == SensorStatus::OK) &&
           (_health.consecutive_failures < _config.max_consecutive_failures);
}

SensorStatus HCSR04Driver::getStatus() const {
    return _health.status;
}

// -----------------------------------------------------------------------------
// Private failure & success handling
// -----------------------------------------------------------------------------

void HCSR04Driver::handleFailure(SensorStatus failureStatus) {
    _health.consecutive_failures++;

    // Check if consecutive failures warrant escalating status to FAULT
    if (_health.consecutive_failures >= _config.max_consecutive_failures) {
        _health.status = SensorStatus::FAULT;
        _reading.status = SensorStatus::FAULT;
    } else {
        _health.status = failureStatus;
        _reading.status = failureStatus;
    }

    // MANDATORY: Invalid distance must NEVER be reported as 0.0 mm
    _reading.distance_mm = -1.0f;
}

void HCSR04Driver::handleSuccess(float distanceMm) {
    _health.last_success_ms = currentMillis();
    _health.consecutive_failures = 0;
    _health.successful_measurements++;
    _health.status = SensorStatus::OK;

    _reading.status = SensorStatus::OK;
    _reading.distance_mm = distanceMm;
}

}  // namespace autochair
