/**
 * @file imu_stub.h
 * @brief AutoChair ESP32 — IMU sensor stub / placeholder
 *
 * Project:  Aivon Innovations — AutoChair
 * Platform: ESP32-WROOM-32D
 * Status:   IN DEVELOPMENT — HARDWARE STUB ONLY
 *
 * The actual MPU6500/MPU6050 driver will be implemented in Phase 5 after:
 *   - Bus type (I2C/SPI) and address are confirmed from actual wiring.
 *   - Device detection is verified on bench.
 *   - Measurement range configuration is selected.
 *   (IMU_INTERFACE.md §4, §5)
 *
 * ASSUMPTION WARNINGS (must be resolved before Phase 5):
 *   - Do NOT assume I2C address 0x68 without verifying with the actual board.
 *   - Do NOT assume the bus type from the sensor model number alone.
 *   - Units (m/s², deg/s, etc.) must be documented by the real driver.
 */

#pragma once

#include "../sensor_interface.h"
#include "../../diagnostics/logger.h"

namespace autochair {

/**
 * @brief Stub IMU — reports UNINITIALIZED, zeroed readings.
 *
 * Replace with the real MPU6500/MPU6050 driver in Phase 5.
 */
class IMUStub : public ISensor {
public:
    bool begin() override {
        Logger::info("IMU", "Stub — hardware not connected (Phase 5)");
        _reading.status = SensorStatus::UNINITIALIZED;
        return true;
    }

    void update() override {
        // No hardware; reading stays UNINITIALIZED.
    }

    bool isHealthy() const override {
        return false;
    }

    SensorStatus getStatus() const override {
        return _reading.status;
    }

    const IMUReading& getReading() const { return _reading; }

private:
    IMUReading _reading{};
};

}  // namespace autochair
