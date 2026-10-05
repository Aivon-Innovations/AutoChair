/**
 * @file ultrasonic_manager.cpp
 * @brief AutoChair ESP32 — Ultrasonic Sensor Manager implementation
 *
 * Project:  Aivon Innovations — AutoChair
 * Platform: ESP32-WROOM-32D
 * Status:   IN DEVELOPMENT
 */

#include "ultrasonic_manager.h"
#include "diagnostics/logger.h"

namespace autochair {

static HCSR04Config makeSensorConfig(uint8_t index) {
    HCSR04Config cfg{};
    cfg.sensor_id                = index;
    cfg.trig_pin                 = config::US_TRIG_PIN[index];
    cfg.echo_pin                 = config::US_ECHO_PIN[index];
    cfg.min_distance_mm          = config::ULTRASONIC_MIN_MM;
    cfg.max_distance_mm          = config::ULTRASONIC_MAX_MM;
    cfg.timeout_us               = config::ULTRASONIC_TIMEOUT_US;
    cfg.max_consecutive_failures = 5;
    return cfg;
}

UltrasonicManager::UltrasonicManager()
    : _drivers{
          HCSR04Driver(makeSensorConfig(0)),
          HCSR04Driver(makeSensorConfig(1)),
          HCSR04Driver(makeSensorConfig(2)),
          HCSR04Driver(makeSensorConfig(3)),
          HCSR04Driver(makeSensorConfig(4)),
          HCSR04Driver(makeSensorConfig(5))
      },
      _currentSensorIndex(0)
{
}

bool UltrasonicManager::begin() {
    Logger::info("UltrasonicManager", "Initializing ultrasonic sensor array");
    bool allOk = true;

    for (uint8_t i = 0; i < config::ULTRASONIC_COUNT; ++i) {
        if (!_drivers[i].begin()) {
            allOk = false;
        }
    }

    return allOk;
}

void UltrasonicManager::update() {
    // Sequential single-sensor round-robin polling with bounded per-update blocking.
    // Only one sensor is triggered and measured per scheduler update to prevent
    // acoustic crosstalk and eliminate multi-sensor blocking loops.
    if (_currentSensorIndex < config::ULTRASONIC_COUNT) {
        _drivers[_currentSensorIndex].update();
        _currentSensorIndex = static_cast<uint8_t>((_currentSensorIndex + 1) % config::ULTRASONIC_COUNT);
    }
}

const UltrasonicReading& UltrasonicManager::getReading(uint8_t index) const {
    if (index < config::ULTRASONIC_COUNT) {
        return _drivers[index].getReading();
    }
    static const UltrasonicReading invalidReading{0xFF, 0, SensorStatus::INVALID, -1.0f};
    return invalidReading;
}

HCSR04Driver& UltrasonicManager::getDriver(uint8_t index) {
    if (index < config::ULTRASONIC_COUNT) {
        return _drivers[index];
    }
    return _drivers[0];
}

const HCSR04Driver& UltrasonicManager::getDriver(uint8_t index) const {
    if (index < config::ULTRASONIC_COUNT) {
        return _drivers[index];
    }
    return _drivers[0];
}

bool UltrasonicManager::areAllHealthy() const {
    for (uint8_t i = 0; i < config::ULTRASONIC_COUNT; ++i) {
        if (!_drivers[i].isHealthy()) {
            return false;
        }
    }
    return true;
}

float UltrasonicManager::getMinValidDistanceMm() const {
    float minDistance = -1.0f;

    for (uint8_t i = 0; i < config::ULTRASONIC_COUNT; ++i) {
        const auto& r = _drivers[i].getReading();
        if (r.status == SensorStatus::OK && r.distance_mm >= 0.0f) {
            if (minDistance < 0.0f || r.distance_mm < minDistance) {
                minDistance = r.distance_mm;
            }
        }
    }

    return minDistance;
}

}  // namespace autochair
