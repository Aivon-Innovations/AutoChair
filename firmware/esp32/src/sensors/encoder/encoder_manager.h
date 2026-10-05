/**
 * @file encoder_manager.h
 * @brief AutoChair ESP32 — Dual Wheel Encoder Manager
 *
 * Project:  Aivon Innovations — AutoChair
 * Platform: ESP32-WROOM-32D
 * Status:   IN DEVELOPMENT
 *
 * Coordinates Left and Right wheel encoder drivers according to
 * ENCODER_INTERFACE.md and SENSOR_INTERFACE.md.
 *
 * Key features:
 *   - Manages independent Left and Right EncoderDriver instances.
 *   - Aggregates readings into WheelEncoderData for the Pi telemetry interface.
 *   - Provides health and status checking.
 *   - Allows injection of custom/mock HAL instances for unit testing.
 *
 * SAFETY BOUNDARY:
 *   - If pins are unverified placeholders (0), drivers safely remain in UNINITIALIZED state.
 *   - Does NOT convert pulse counts to distance or wheel speed in m/s.
 *   - Does NOT command motor controllers or apply wheelchair gearbox conversions.
 */

#pragma once

#include "encoder_driver.h"
#include "autochair_config.h"

#include <cstdint>
#include <cstddef>

namespace autochair {

class EncoderManager {
public:
    EncoderManager();

    /**
     * @brief Construct with custom configurations (useful for test fixtures / calibration).
     */
    EncoderManager(const EncoderConfig& leftConfig, const EncoderConfig& rightConfig);

    /**
     * @brief Initialize both wheel encoder drivers.
     * @return true if at least one driver initialized, false if both uninitialized/failed.
     */
    bool begin();

    /**
     * @brief Update both encoder drivers and aggregate readings.
     */
    void update();

    /**
     * @brief Get aggregated wheel encoder readings.
     */
    const WheelEncoderData& getWheelData() const { return _wheelData; }

    /**
     * @brief Access Left encoder driver.
     */
    EncoderDriver& getLeftDriver() { return _leftDriver; }
    const EncoderDriver& getLeftDriver() const { return _leftDriver; }

    /**
     * @brief Access Right encoder driver.
     */
    EncoderDriver& getRightDriver() { return _rightDriver; }
    const EncoderDriver& getRightDriver() const { return _rightDriver; }

    /**
     * @brief Reset pulse counts and index events on both encoders.
     */
    void resetCounts();

    /**
     * @brief Check if both encoders are healthy.
     */
    bool areAllHealthy() const;

private:
    EncoderDriver    _leftDriver;
    EncoderDriver    _rightDriver;
    WheelEncoderData _wheelData{};
};

}  // namespace autochair
