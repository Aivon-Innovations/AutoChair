/**
 * @file sensor_interface.h
 * @brief AutoChair ESP32 — Common sensor interface (ISensor)
 *
 * Project:  Aivon Innovations — AutoChair
 * Platform: ESP32-WROOM-32D
 * Status:   IN DEVELOPMENT
 *
 * Defines the abstract interface that all sensor modules implement.
 * (SENSOR_INTERFACE.md §4)
 *
 * Hardware-specific drivers implement ISensor.
 * Simulation drivers also implement ISensor, allowing firmware logic
 * to be tested without physical hardware.
 *
 * SAFETY NOTE:
 * Sensor drivers report conditions to the Fault Manager / Safety Manager.
 * They must not independently change the system safety state.
 */

#pragma once

#include "autochair_types.h"
#include <cstdint>

namespace autochair {

// =============================================================================
// Base sensor reading  (SENSOR_INTERFACE.md §6)
// =============================================================================

struct SensorReading {
    uint8_t      sensor_id    = 0;
    uint32_t     timestamp_ms = 0;
    SensorStatus status       = SensorStatus::UNINITIALIZED;
};

// =============================================================================
// Ultrasonic reading  (SENSOR_INTERFACE.md §10)
// =============================================================================

struct UltrasonicReading : public SensorReading {
    float distance_mm = -1.0f;  ///< Negative = invalid / not measured.
    // An invalid reading must NEVER be represented as 0.0 mm (perceived obstacle).
};

// =============================================================================
// IMU reading  (IMU_INTERFACE.md §6)
// =============================================================================

struct IMUReading : public SensorReading {
    float accel_x = 0.0f;  ///< Units defined by driver implementation.
    float accel_y = 0.0f;
    float accel_z = 0.0f;
    float gyro_x  = 0.0f;  ///< Units defined by driver implementation.
    float gyro_y  = 0.0f;
    float gyro_z  = 0.0f;
    // NOTE: Units must be explicitly documented by the implemented driver.
    // Do not assume SI units until the driver verifies them.
};

// =============================================================================
// Encoder reading  (ENCODER_INTERFACE.md §14)
// =============================================================================

struct EncoderReading : public SensorReading {
    int64_t          count                 = 0;
    EncoderDirection direction             = EncoderDirection::UNKNOWN;
    uint32_t         pulses_since_last_update = 0;
    // NOTE: count is raw pulse count. Do NOT convert to distance without
    // verified encoder mounting, PPR, quadrature mode, and wheel geometry.
};

struct WheelEncoderData {
    EncoderReading left;
    EncoderReading right;
};

// =============================================================================
// ISensor — abstract base  (SENSOR_INTERFACE.md §4)
// =============================================================================

/**
 * @brief Abstract interface implemented by every sensor module.
 *
 * All hardware implementations AND simulation implementations must
 * satisfy this interface. The Sensor Manager holds pointers to ISensor
 * so implementations can be swapped without changing application code.
 */
class ISensor {
public:
    virtual ~ISensor() = default;

    /**
     * @brief Initialize the sensor hardware (or simulator).
     * @return true on success, false on failure.
     */
    virtual bool begin() = 0;

    /**
     * @brief Perform one acquisition cycle.
     * Called at the configured update interval from the Sensor Manager.
     */
    virtual void update() = 0;

    /**
     * @brief Returns true if the latest measurement passed validation.
     * A sensor that has initialized but has not yet completed a valid
     * measurement must NOT return true.
     */
    virtual bool isHealthy() const = 0;

    /** @brief Current sensor status. */
    virtual SensorStatus getStatus() const = 0;
};

}  // namespace autochair
