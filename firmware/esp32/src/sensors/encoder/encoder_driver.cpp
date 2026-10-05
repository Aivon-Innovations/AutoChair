/**
 * @file encoder_driver.cpp
 * @brief AutoChair ESP32 — Wheel Encoder Concrete Driver implementation
 *
 * Project:  Aivon Innovations — AutoChair
 * Platform: ESP32-WROOM-32D
 * Status:   IN DEVELOPMENT — SOFTWARE DRIVER
 */

#include "encoder_driver.h"
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

EncoderDriver::EncoderDriver(const EncoderConfig& config, IEncoderHAL* hal)
    : _config(config),
      _hal(hal ? hal : &_defaultHardwareHAL),
      _defaultHardwareHAL(config.sensor_id)
{
    _reading.sensor_id                = _config.sensor_id;
    _reading.timestamp_ms             = 0;
    _reading.status                   = SensorStatus::UNINITIALIZED;
    _reading.count                    = 0;
    _reading.direction                = EncoderDirection::STATIONARY;
    _reading.pulses_since_last_update = 0;

    _health.status = SensorStatus::UNINITIALIZED;
}

void EncoderDriver::setHAL(IEncoderHAL* hal) {
    _hal = (hal != nullptr) ? hal : &_defaultHardwareHAL;
}

void EncoderDriver::resetCount() {
    if (_hal) {
        _hal->resetCount();
        _hal->resetIndexCount();
    }
    _reading.count                    = 0;
    _reading.pulses_since_last_update = 0;
    _reading.rpm                      = 0.0f;
    _reading.direction                = EncoderDirection::STATIONARY;
    _health.index_events              = 0;
    _lastHalCount                     = 0;
}

void EncoderDriver::reset() {
    _health = EncoderHealth{};
    _health.status = _initialized ? SensorStatus::INITIALIZING : SensorStatus::UNINITIALIZED;

    _reading.status                   = _health.status;
    _reading.count                    = 0;
    _reading.direction                = EncoderDirection::STATIONARY;
    _reading.pulses_since_last_update = 0;
    _reading.rpm                      = 0.0f;
    _reading.timestamp_ms             = currentMillis();
    _lastHalCount                     = 0;

    if (_hal) {
        _hal->resetCount();
        _hal->resetIndexCount();
    }
}

bool EncoderDriver::begin() {
    Logger::info("EncoderDriver", "Initializing wheel encoder driver");

    bool halOk = _hal->init(_config.pin_a, _config.pin_b, _config.pin_z,
                            _config.decoding_mode);
    if (!halOk) {
        // When pins are 0 (placeholder), driver enters UNINITIALIZED state safely.
        Logger::warning("EncoderDriver",
                        "Pins not configured (placeholder / unverified). Software driver ready.");
        _initialized = false;
        _health.status = SensorStatus::UNINITIALIZED;
        _reading.status = SensorStatus::UNINITIALIZED;
        _reading.rpm = 0.0f;
        return false;
    }

    _initialized = true;
    _health.status = SensorStatus::INITIALIZING;
    _reading.status = SensorStatus::INITIALIZING;
    _reading.timestamp_ms = currentMillis();
    _reading.rpm = 0.0f;
    _lastHalCount = _hal->getCount();

    return true;
}

void EncoderDriver::update() {
    update(currentMillis());
}

void EncoderDriver::update(uint32_t timestampMs) {
    uint32_t now = (timestampMs != 0) ? timestampMs : currentMillis();
    uint32_t elapsedMs = (now >= _reading.timestamp_ms) ? (now - _reading.timestamp_ms) : 0;
    _reading.timestamp_ms = now;

    if (!_initialized) {
        _reading.status = SensorStatus::UNINITIALIZED;
        _health.status  = SensorStatus::UNINITIALIZED;
        _reading.rpm    = 0.0f;
        return;
    }

    int64_t currentHalCount = _hal->getCount();
    int64_t delta = currentHalCount - _lastHalCount;
    _lastHalCount = currentHalCount;

    uint32_t absDelta = static_cast<uint32_t>(delta >= 0 ? delta : -delta);
    _reading.pulses_since_last_update = absDelta;

    // Apply polarity configuration
    int64_t orientedCount = _config.reverse_direction ? -currentHalCount : currentHalCount;
    int64_t orientedDelta = _config.reverse_direction ? -delta : delta;
    _reading.count = orientedCount;

    // RPM Calculation:
    // rpm = (signed_count_delta / counts_per_revolution) * (60000.0f / elapsed_ms)
    uint32_t cpr = getCountsPerRevolution();
    if (elapsedMs > 0 && cpr > 0 && orientedDelta != 0) {
        _reading.rpm = (static_cast<float>(orientedDelta) / static_cast<float>(cpr)) *
                       (60000.0f / static_cast<float>(elapsedMs));
    } else {
        _reading.rpm = 0.0f;
    }

    // Determine direction from delta
    if (orientedDelta > 0) {
        _reading.direction = EncoderDirection::FORWARD;
        _health.last_pulse_ms = now;
        _health.total_pulses += absDelta;
    } else if (orientedDelta < 0) {
        _reading.direction = EncoderDirection::REVERSE;
        _health.last_pulse_ms = now;
        _health.total_pulses += absDelta;
    } else {
        _reading.direction = EncoderDirection::STATIONARY;
    }

    // Update health metrics
    _health.index_events        = _hal->getIndexCount();
    _health.invalid_transitions = _hal->getInvalidTransitionCount();
    _health.status              = SensorStatus::OK;
    _reading.status             = SensorStatus::OK;
}

bool EncoderDriver::isHealthy() const {
    return _initialized &&
           (_health.status != SensorStatus::FAULT) &&
           (_health.status != SensorStatus::UNINITIALIZED);
}

SensorStatus EncoderDriver::getStatus() const {
    return _health.status;
}

}  // namespace autochair
