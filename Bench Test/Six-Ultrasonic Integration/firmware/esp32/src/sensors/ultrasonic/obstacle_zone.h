/**
 * @file obstacle_zone.h
 * @brief AutoChair ESP32 — Ultrasonic Obstacle-Zone Classifier
 *
 * Project:  Aivon Innovations — AutoChair
 * Platform: ESP32-WROOM-32D
 * Status:   IN DEVELOPMENT
 *
 * Classifies each ultrasonic sensor reading into one of four logical
 * obstacle zones based on measured distance, sensor status, and reading
 * staleness. Also computes an overall (most-restrictive) zone across
 * all active sensors.
 *
 * Zone definitions:
 *   CLEAR    — No proximate obstacle detected; path appears open.
 *   CAUTION  — Object present in the approach region; alert state.
 *   OBSTACLE — Object at close range; immediate proximity.
 *   FAULT    — Sensor invalid, timed out, or reading is stale.
 *              A FAULT must NOT be treated as CLEAR.
 *
 * Thresholds are defined in autochair_config.h and are explicitly
 * labelled as UNVALIDATED PROVISIONAL ENGINEERING PARAMETERS.
 *
 * SAFETY BOUNDARY:
 *   This module performs distance classification only.
 *   It does NOT issue motor commands, activate brakes, or change
 *   the wheelchair's physical motion state in any way.
 *   Integration with the SafetyManager is a future phase item.
 */

#pragma once

#include "autochair_config.h"
#include "autochair_types.h"
#include "sensors/sensor_interface.h"

#include <cstdint>

namespace autochair {

// =============================================================================
// ObstacleZone enumeration
// =============================================================================

/**
 * @brief Distance-based obstacle zone classification for a single sensor.
 *
 * Ordered by severity; higher numeric value = higher priority in aggregation.
 */
enum class ObstacleZone : uint8_t {
    CLEAR    = 0,  ///< distance >= ZONE_CAUTION_MM; path appears open.
    CAUTION  = 1,  ///< ZONE_OBSTACLE_MM <= distance < ZONE_CAUTION_MM.
    OBSTACLE = 2,  ///< distance < ZONE_OBSTACLE_MM (and valid reading).
    FAULT    = 3   ///< Invalid, timed-out, or stale reading.
};

// =============================================================================
// Free-function classifier
// =============================================================================

/**
 * @brief Classify a single UltrasonicReading into an ObstacleZone.
 *
 * Rules applied in order:
 *  1. If status is not OK → FAULT.
 *  2. If distance_mm < 0  → FAULT (invalid sentinel value).
 *  3. If reading is older than config::ULTRASONIC_STALE_THRESHOLD_MS → FAULT.
 *  4. If distance_mm < ZONE_OBSTACLE_MM → OBSTACLE.
 *  5. If distance_mm < ZONE_CAUTION_MM  → CAUTION.
 *  6. Otherwise → CLEAR.
 *
 * @param reading    The ultrasonic reading to classify.
 * @param now_ms     Current millisecond timestamp (from millis() or equivalent).
 * @return           Corresponding ObstacleZone.
 */
ObstacleZone classifyZone(const UltrasonicReading& reading, uint32_t now_ms);

// =============================================================================
// ObstacleZoneClassifier
// =============================================================================

/**
 * @brief Stateless classifier that evaluates all six ultrasonic channels
 *        and produces per-sensor zones and an aggregate zone.
 *
 * The aggregate zone is the most-restrictive (highest-severity) zone
 * across all sensors. A single FAULT sensor escalates the aggregate to FAULT.
 *
 * Design note: This class holds no mutable state; it re-evaluates from
 * fresh readings on every call to update(). The UltrasonicManager owns
 * the readings and the HCSR04Driver instances.
 */
class ObstacleZoneClassifier {
public:
    ObstacleZoneClassifier() = default;

    /**
     * @brief Evaluate all sensor readings and update zone states.
     *
     * Call this once per scheduler tick, after UltrasonicManager::update().
     *
     * @param readings  Array of ULTRASONIC_COUNT UltrasonicReading structs.
     * @param count     Number of entries in the array (must == ULTRASONIC_COUNT).
     * @param now_ms    Current millisecond timestamp.
     */
    void update(const UltrasonicReading* readings, uint8_t count, uint32_t now_ms);

    /**
     * @brief Get the zone for a specific sensor by 0-based index.
     * @return FAULT if index is out of range.
     */
    ObstacleZone getSensorZone(uint8_t index) const;

    /**
     * @brief Get the most-restrictive zone across all sensors.
     *
     * Priority: FAULT > OBSTACLE > CAUTION > CLEAR.
     * A single FAULT sensor makes the aggregate FAULT.
     */
    ObstacleZone getOverallZone() const;

    /**
     * @brief Human-readable zone name for serial telemetry.
     */
    static const char* zoneName(ObstacleZone zone);

private:
    ObstacleZone _zones[config::ULTRASONIC_COUNT] = {};
    ObstacleZone _overallZone = ObstacleZone::FAULT;
    uint8_t      _count       = 0;
};

}  // namespace autochair
