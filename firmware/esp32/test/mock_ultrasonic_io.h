/**
 * @file mock_ultrasonic_io.h
 * @brief AutoChair ESP32 — MockUltrasonicIO for native unit tests
 *
 * Project:  Aivon Innovations — AutoChair
 * Platform: Native (host build, ENV_NATIVE)
 * Branch:   alvira-dev
 * Status:   IN DEVELOPMENT
 *
 * Provides a configurable mock implementation of UltrasonicIO used by
 * test_ultrasonic_integration.cpp to drive HCSR04Driver and UltrasonicManager
 * without any real ESP32 hardware.
 *
 * Usage:
 *   MockUltrasonicIO mock;                         // FIXED_PULSE mode, 500 mm
 *   MockUltrasonicIO mock(MockMode::TIMEOUT);       // always returns 0 (no echo)
 *   mock.setDistanceMm(200.0f);                    // change simulated distance
 *   mock.setMode(MockMode::FIXED_PULSE);            // switch mode at runtime
 *   int count = mock.getMeasureCount();             // verify trigger count
 */

#pragma once

#include "sensors/sensor_interface.h"
#include <cstdint>

namespace autochair {

class MockUltrasonicIO : public UltrasonicIO {
public:
    enum class MockMode {
        FIXED_PULSE,  ///< Returns a computed pulse for the configured distance.
        TIMEOUT       ///< Always returns 0 (simulates no echo received).
    };

    /**
     * @brief Construct with optional mode. Defaults to FIXED_PULSE, 500 mm.
     */
    explicit MockUltrasonicIO(MockMode mode = MockMode::FIXED_PULSE)
        : _mode(mode), _dist_mm(500.0f), _count(0), _millis(0) {}

    /**
     * @brief Set the simulated obstacle distance (FIXED_PULSE mode only).
     */
    void setDistanceMm(float dist_mm) { _dist_mm = dist_mm; }

    /**
     * @brief Change the mock operating mode.
     */
    void setMode(MockMode mode) { _mode = mode; }

    /**
     * @brief Return total number of measurePulseUs() calls on this mock.
     *
     * Each call corresponds to one HCSR04Driver::update() that reached
     * the measurement step (i.e., driver was not in FAULT and IO was set).
     */
    int getMeasureCount() const { return _count; }

    /**
     * @brief Advance the simulated millisecond clock by the given amount.
     */
    void advanceMillis(uint32_t ms) { _millis += ms; }

    // -------------------------------------------------------------------------
    // UltrasonicIO overrides
    // -------------------------------------------------------------------------

    /**
     * @brief Simulate the trigger-and-echo sequence.
     *
     * TIMEOUT mode : returns 0 regardless of configured distance.
     * FIXED_PULSE  : converts _dist_mm to a pulse duration in µs.
     *                Formula: dist_mm = (us * 0.3430) / 2 →
     *                         us = dist_mm * 2 / 0.3430
     */
    uint32_t measurePulseUs(uint8_t /*trig_pin*/,
                             uint8_t /*echo_pin*/,
                             uint32_t /*timeout_us*/) override {
        _count++;

        if (_mode == MockMode::TIMEOUT) {
            return 0u;
        }

        // FIXED_PULSE: convert distance to pulse duration.
        return static_cast<uint32_t>(_dist_mm * 2.0f / 0.3430f);
    }

    /**
     * @brief Return the simulated millisecond clock value.
     */
    uint32_t getMillis() override {
        return _millis;
    }

private:
    MockMode _mode;
    float    _dist_mm;
    int      _count;
    uint32_t _millis;
};

}  // namespace autochair
