/**
 * @file obstacle_zone.cpp
 * @brief AutoChair ESP32 — Ultrasonic Obstacle-Zone Classifier implementation
 *
 * Project:  Aivon Innovations — AutoChair
 * Platform: ESP32-WROOM-32D
 * Status:   IN DEVELOPMENT
 */

#include "obstacle_zone.h"

namespace autochair {

// =============================================================================
// Free-function classifier
// =============================================================================

ObstacleZone classifyZone(const UltrasonicReading& reading, uint32_t now_ms) {
    // Rule 1: Non-OK sensor status → FAULT
    if (reading.status != SensorStatus::OK) {
        return ObstacleZone::FAULT;
    }

    // Rule 2: Negative distance sentinel → FAULT
    if (reading.distance_mm < 0.0f) {
        return ObstacleZone::FAULT;
    }

    // Rule 3: Stale reading → FAULT
    // Guard against timestamp wrap (now_ms < timestamp_ms would underflow).
    uint32_t age_ms = (now_ms >= reading.timestamp_ms)
                      ? (now_ms - reading.timestamp_ms)
                      : 0xFFFFFFFFu;  // Treat wrap as maximum age (stale).
    if (age_ms > config::ULTRASONIC_STALE_THRESHOLD_MS) {
        return ObstacleZone::FAULT;
    }

    // Rule 4: Below obstacle threshold → OBSTACLE
    if (reading.distance_mm < config::ZONE_OBSTACLE_MM) {
        return ObstacleZone::OBSTACLE;
    }

    // Rule 5: Below caution threshold → CAUTION
    if (reading.distance_mm < config::ZONE_CAUTION_MM) {
        return ObstacleZone::CAUTION;
    }

    // Rule 6: Clear
    return ObstacleZone::CLEAR;
}

// =============================================================================
// ObstacleZoneClassifier
// =============================================================================

void ObstacleZoneClassifier::update(const UltrasonicReading* readings,
                                    uint8_t count,
                                    uint32_t now_ms) {
    _count = (count <= config::ULTRASONIC_COUNT) ? count : config::ULTRASONIC_COUNT;

    // Default aggregate starts at CLEAR; escalates as each sensor is evaluated.
    _overallZone = ObstacleZone::CLEAR;

    for (uint8_t i = 0; i < _count; ++i) {
        ObstacleZone z = classifyZone(readings[i], now_ms);
        _zones[i] = z;

        // Escalate overall zone to the most restrictive value seen.
        // Enum ordering: CLEAR(0) < CAUTION(1) < OBSTACLE(2) < FAULT(3).
        if (static_cast<uint8_t>(z) > static_cast<uint8_t>(_overallZone)) {
            _overallZone = z;
        }
    }

    // Any uninitialised tail slots are left as FAULT (default array init).
    for (uint8_t i = _count; i < config::ULTRASONIC_COUNT; ++i) {
        _zones[i] = ObstacleZone::FAULT;
    }
}

ObstacleZone ObstacleZoneClassifier::getSensorZone(uint8_t index) const {
    if (index >= config::ULTRASONIC_COUNT) {
        return ObstacleZone::FAULT;
    }
    return _zones[index];
}

ObstacleZone ObstacleZoneClassifier::getOverallZone() const {
    return _overallZone;
}

const char* ObstacleZoneClassifier::zoneName(ObstacleZone zone) {
    switch (zone) {
        case ObstacleZone::CLEAR:    return "CLEAR";
        case ObstacleZone::CAUTION:  return "CAUTION";
        case ObstacleZone::OBSTACLE: return "OBSTACLE";
        case ObstacleZone::FAULT:    return "FAULT";
        default:                     return "UNKNOWN";
    }
}

}  // namespace autochair
