/**
 * @file ultrasonic_stub.h
 * @brief AutoChair ESP32 — Ultrasonic sensor stub / placeholder
 *
 * Project:  Aivon Innovations — AutoChair
 * Platform: ESP32-WROOM-32D
 * Status:   IN DEVELOPMENT — HARDWARE STUB ONLY
 *
 * This file provides a non-driving stub that satisfies the ISensor interface
 * for compilation and early software testing.
 *
 * HARDWARE NOT YET CONNECTED.
 *
 * The actual HC-SR04 driver will be implemented in Phase 4 after:
 *   - GPIO pin assignments are confirmed.
 *   - Level-shifting/protection for the 5V ECHO line is verified.
 *   - Timing parameters are validated on bench.
 *   (SENSOR_INTERFACE.md §8)
 *
 * SAFETY NOTE:
 *   A stub that always returns UNINITIALIZED is safe — it will never
 *   incorrectly report a clear path.
 */

#pragma once

#include "../sensor_interface.h"
#include "../../diagnostics/logger.h"

namespace autochair {

/**
 * @brief Stub ultrasonic sensor — reports UNINITIALIZED.
 *
 * Replace this class with the real HC-SR04 driver in Phase 4.
 * The ISensor interface remains identical.
 */
class UltrasonicStub : public ISensor {
public:
    explicit UltrasonicStub(uint8_t sensorId) {
        _reading.sensor_id = sensorId;
        _reading.status    = SensorStatus::UNINITIALIZED;
        _reading.distance_mm = -1.0f;
    }

    bool begin() override {
        Logger::info("Ultrasonic", "Stub — hardware not connected (Phase 4)");
        _reading.status = SensorStatus::UNINITIALIZED;
        return true;  // Stub always succeeds; status clearly shows UNINITIALIZED.
    }

    void update() override {
        // No hardware; reading remains UNINITIALIZED.
        // Safety Manager must not treat UNINITIALIZED as a valid "no obstacle" reading.
    }

    bool isHealthy() const override {
        return false;  // Stub is never healthy.
    }

    SensorStatus getStatus() const override {
        return _reading.status;
    }

    const UltrasonicReading& getReading() const { return _reading; }

private:
    UltrasonicReading _reading{};
};

}  // namespace autochair
