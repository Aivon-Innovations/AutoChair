/**
 * @file mpu_driver.h
 * @brief AutoChair ESP32 — MPU6050 / MPU6500 Concrete IMU Driver
 *
 * Project:  Aivon Innovations — AutoChair
 * Platform: ESP32-WROOM-32D
 * Status:   IN DEVELOPMENT — SOFTWARE DRIVER (BENCH / SIMULATION SUPPORTED)
 *
 * Implements the concrete IMU driver for MPU6050 and MPU6500 sensors
 * according to IMU_INTERFACE.md and SENSOR_INTERFACE.md.
 *
 * Key features:
 *   - Common abstraction supporting both MPU6500 and MPU6050 devices.
 *   - Auto-detection via WHO_AM_I register (0x68 for MPU6050, 0x70 for MPU6500).
 *   - Hardware isolation via IIMUBus HAL.
 *   - Documented engineering units:
 *       * Accelerometer: linear acceleration in g (1g = 9.80665 m/s^2)
 *       * Gyroscope: angular velocity in degrees per second (deg/s)
 *   - Explicit separation of raw register acquisition, calibration, and orientation estimation.
 *   - Sensor health tracking and communication failure escalation.
 *
 * SAFETY BOUNDARY (MANDATORY):
 *   - No physical bus communication or pin driving is done until hardware is confirmed.
 *   - No unvalidated tilt-based emergency braking or autonomous control is enabled.
 */

#pragma once

#include "../sensor_interface.h"
#include "imu_bus.h"
#include "autochair_types.h"
#include "autochair_config.h"

#include <cstdint>
#include <cstddef>

namespace autochair {

enum class IMUDeviceType : uint8_t {
    UNKNOWN,
    MPU6050,
    MPU6500
};

enum class AccelScale : uint8_t {
    SCALE_2G  = 0,   ///< ±2g  (16384 LSB/g)
    SCALE_4G  = 1,   ///< ±4g  (8192 LSB/g)
    SCALE_8G  = 2,   ///< ±8g  (4096 LSB/g)
    SCALE_16G = 3    ///< ±16g (2048 LSB/g)
};

enum class GyroScale : uint8_t {
    SCALE_250DPS  = 0,  ///< ±250 deg/s  (131.0 LSB/(deg/s))
    SCALE_500DPS  = 1,  ///< ±500 deg/s  (65.5 LSB/(deg/s))
    SCALE_1000DPS = 2,  ///< ±1000 deg/s (32.8 LSB/(deg/s))
    SCALE_2000DPS = 3   ///< ±2000 deg/s (16.4 LSB/(deg/s))
};

struct MPUConfig {
    uint8_t       sensor_id                = 0;
    IMUDeviceType expected_type            = IMUDeviceType::UNKNOWN; ///< UNKNOWN = auto-detect.
    uint8_t       i2c_address              = 0x68;                   ///< Default I2C address (PLACEHOLDER).
    AccelScale    accel_scale              = AccelScale::SCALE_4G;
    GyroScale     gyro_scale               = GyroScale::SCALE_500DPS;
    uint16_t      max_consecutive_failures = 5;
};

struct IMUHealth {
    uint32_t     last_success_ms      = 0;
    uint16_t     consecutive_failures = 0;
    uint32_t     total_measurements   = 0;
    uint32_t     successful_measurements = 0;
    SensorStatus status               = SensorStatus::UNINITIALIZED;
};

class MPUDriver : public ISensor {
public:
    // Register definitions
    static constexpr uint8_t REG_WHO_AM_I     = 0x75;
    static constexpr uint8_t REG_PWR_MGMT_1   = 0x6B;
    static constexpr uint8_t REG_ACCEL_CONFIG = 0x1C;
    static constexpr uint8_t REG_GYRO_CONFIG  = 0x1B;
    static constexpr uint8_t REG_ACCEL_XOUT_H = 0x3B;

    // Expected WHO_AM_I responses
    static constexpr uint8_t WHO_AM_I_MPU6050 = 0x68;
    static constexpr uint8_t WHO_AM_I_MPU6500 = 0x70;

    explicit MPUDriver(const MPUConfig& config, IIMUBus* bus = nullptr);

    // -------------------------------------------------------------------------
    // ISensor Interface
    // -------------------------------------------------------------------------

    bool begin() override;
    void update() override;
    bool isHealthy() const override;
    SensorStatus getStatus() const override;

    // -------------------------------------------------------------------------
    // Accessors
    // -------------------------------------------------------------------------

    const IMUReading& getReading() const { return _reading; }
    const IMUHealth&  getHealth()  const { return _health;  }
    const MPUConfig&  getConfig()  const { return _config;  }
    IMUDeviceType     getDetectedType() const { return _detectedType; }

    void setBus(IIMUBus* bus);
    void reset();

    // -------------------------------------------------------------------------
    // Conversion helpers (scale factor to physical units)
    // -------------------------------------------------------------------------

    static float getAccelScaleFactor(AccelScale scale);
    static float getGyroScaleFactor(GyroScale scale);

private:
    MPUConfig         _config;
    IIMUBus*          _bus;
    HardwareI2CIMUBus _defaultI2CBus;
    IMUReading        _reading{};
    IMUHealth         _health{};
    IMUDeviceType     _detectedType = IMUDeviceType::UNKNOWN;
    bool              _initialized  = false;

    void handleFailure(SensorStatus failureStatus);
    void handleSuccess(float ax, float ay, float az, float gx, float gy, float gz);
};

}  // namespace autochair
