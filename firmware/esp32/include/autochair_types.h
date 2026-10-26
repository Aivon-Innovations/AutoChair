/**
 * @file autochair_types.h
 * @brief AutoChair ESP32 — Shared sensor data types
 *
 * Project:  Aivon Innovations — AutoChair
 * Platform: ESP32-WROOM-32D
 * Branch:   alvira-dev
 * Status:   IN DEVELOPMENT
 *
 * Defines the core sensor reading struct and status enumeration
 * shared across all sensor drivers, the ultrasonic manager,
 * and the obstacle-zone classifier.
 */

#pragma once

#include <cstdint>

namespace autochair {

// =============================================================================
// SensorStatus
// =============================================================================

/**
 * @brief Operational status of a sensor reading.
 *
 * Ordered so that a simple comparison can detect degraded states.
 */
enum class SensorStatus : uint8_t {
    UNINITIALIZED = 0, ///< Driver created but begin() not yet called or no measurement taken.
    OK            = 1, ///< Valid measurement; distance_mm is trustworthy.
    TIMEOUT       = 2, ///< No echo received within ULTRASONIC_TIMEOUT_US.
    INVALID       = 3, ///< Reading structurally invalid (e.g., sentinel / out-of-range index).
    FAULT         = 4  ///< Driver escalated after ULTRASONIC_MAX_FAILURES consecutive failures.
};

// =============================================================================
// UltrasonicReading
// =============================================================================

/**
 * @brief A single ultrasonic distance measurement with associated metadata.
 *
 * Produced by HCSR04Driver on each update() call and consumed by
 * ObstacleZoneClassifier to determine per-sensor and aggregate zone states.
 *
 * Field definitions:
 *   sensor_id    — 0-based index (0–5 for the six-sensor array). 0xFF = invalid/sentinel.
 *   distance_mm  — Measured distance in millimetres; -1.0f if no valid echo.
 *   status       — See SensorStatus.
 *   timestamp_ms — millis() value at measurement time, for staleness detection.
 */
struct UltrasonicReading {
    uint8_t      sensor_id    = 0xFF;
    float        distance_mm  = -1.0f;
    SensorStatus status       = SensorStatus::UNINITIALIZED;
    uint32_t     timestamp_ms = 0;
};

}  // namespace autochair
