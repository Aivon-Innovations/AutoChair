/**
 * @file mpu_driver.cpp
 * @brief AutoChair ESP32 — MPU6050 / MPU6500 Concrete IMU Driver implementation
 *
 * Project:  Aivon Innovations — AutoChair
 * Platform: ESP32-WROOM-32D
 * Status:   IN DEVELOPMENT — SOFTWARE DRIVER
 */

#include "mpu_driver.h"
#include "diagnostics/logger.h"

#ifdef ENV_NATIVE
#  include <chrono>
static uint32_t currentMillis() {
    using namespace std::chrono;
    return static_cast<uint32_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count()
    );
}
#else
#  include <Arduino.h>
static inline uint32_t currentMillis() {
    return millis();
}
#endif

namespace autochair {

MPUDriver::MPUDriver(const MPUConfig& config, IIMUBus* bus)
    : _config(config),
      _bus(bus ? bus : &_defaultI2CBus),
      _defaultI2CBus(config.i2c_address)
{
    _reading.sensor_id    = _config.sensor_id;
    _reading.timestamp_ms = 0;
    _reading.status       = SensorStatus::UNINITIALIZED;
    _reading.accel_x      = 0.0f;
    _reading.accel_y      = 0.0f;
    _reading.accel_z      = 0.0f;
    _reading.gyro_x       = 0.0f;
    _reading.gyro_y       = 0.0f;
    _reading.gyro_z       = 0.0f;

    _health.status        = SensorStatus::UNINITIALIZED;
}

void MPUDriver::setBus(IIMUBus* bus) {
    _bus = (bus != nullptr) ? bus : &_defaultI2CBus;
}

void MPUDriver::reset() {
    _health = IMUHealth{};
    _health.status = _initialized ? SensorStatus::INITIALIZING : SensorStatus::UNINITIALIZED;

    _reading.status = _health.status;
    _reading.accel_x = 0.0f;
    _reading.accel_y = 0.0f;
    _reading.accel_z = 0.0f;
    _reading.gyro_x  = 0.0f;
    _reading.gyro_y  = 0.0f;
    _reading.gyro_z  = 0.0f;
    _reading.timestamp_ms = currentMillis();
}

float MPUDriver::getAccelScaleFactor(AccelScale scale) {
    switch (scale) {
        case AccelScale::SCALE_2G:  return 16384.0f;
        case AccelScale::SCALE_4G:  return 8192.0f;
        case AccelScale::SCALE_8G:  return 4096.0f;
        case AccelScale::SCALE_16G: return 2048.0f;
        default:                    return 8192.0f;
    }
}

float MPUDriver::getGyroScaleFactor(GyroScale scale) {
    switch (scale) {
        case GyroScale::SCALE_250DPS:  return 131.0f;
        case GyroScale::SCALE_500DPS:  return 65.5f;
        case GyroScale::SCALE_1000DPS: return 32.8f;
        case GyroScale::SCALE_2000DPS: return 16.4f;
        default:                       return 65.5f;
    }
}

bool MPUDriver::begin() {
    Logger::info("MPUDriver", "Initializing IMU driver");

    if (!_bus->begin()) {
        Logger::warning("MPUDriver", "Bus initialization failed (hardware/wiring unverified)");
        _initialized = false;
        _health.status = SensorStatus::UNINITIALIZED;
        _reading.status = SensorStatus::UNINITIALIZED;
        return false;
    }

    // Step 1: Read WHO_AM_I register (0x75)
    uint8_t whoAmI = 0;
    if (!_bus->readRegister(REG_WHO_AM_I, whoAmI)) {
        Logger::warning("MPUDriver", "Failed to read WHO_AM_I register");
        _initialized = false;
        _health.status = SensorStatus::UNINITIALIZED;
        _reading.status = SensorStatus::UNINITIALIZED;
        return false;
    }

    // Identify device type
    if (whoAmI == WHO_AM_I_MPU6050) {
        _detectedType = IMUDeviceType::MPU6050;
        Logger::info("MPUDriver", "Detected device: MPU6050 (WHO_AM_I = 0x68)");
    } else if (whoAmI == WHO_AM_I_MPU6500) {
        _detectedType = IMUDeviceType::MPU6500;
        Logger::info("MPUDriver", "Detected device: MPU6500 (WHO_AM_I = 0x70)");
    } else {
        Logger::warning("MPUDriver", "Unrecognized WHO_AM_I response");
        _detectedType = IMUDeviceType::UNKNOWN;
        if (_config.expected_type != IMUDeviceType::UNKNOWN) {
            _initialized = false;
            _health.status = SensorStatus::UNINITIALIZED;
            _reading.status = SensorStatus::UNINITIALIZED;
            return false;
        }
    }

    // If a specific type was strictly expected, verify match
    if (_config.expected_type != IMUDeviceType::UNKNOWN &&
        _config.expected_type != _detectedType) {
        Logger::warning("MPUDriver", "Device type mismatch with expected configuration");
        _initialized = false;
        _health.status = SensorStatus::UNINITIALIZED;
        _reading.status = SensorStatus::UNINITIALIZED;
        return false;
    }

    // Step 2: Wake up device (clear SLEEP bit in PWR_MGMT_1)
    if (!_bus->writeRegister(REG_PWR_MGMT_1, 0x00)) {
        Logger::warning("MPUDriver", "Failed to clear sleep mode in PWR_MGMT_1");
        _initialized = false;
        _health.status = SensorStatus::UNINITIALIZED;
        _reading.status = SensorStatus::UNINITIALIZED;
        return false;
    }

    // Step 3: Configure Accelerometer Range
    uint8_t accelConfigVal = static_cast<uint8_t>(_config.accel_scale) << 3;
    if (!_bus->writeRegister(REG_ACCEL_CONFIG, accelConfigVal)) {
        Logger::warning("MPUDriver", "Failed to write ACCEL_CONFIG");
        _initialized = false;
        _health.status = SensorStatus::UNINITIALIZED;
        _reading.status = SensorStatus::UNINITIALIZED;
        return false;
    }

    // Step 4: Configure Gyroscope Range
    uint8_t gyroConfigVal = static_cast<uint8_t>(_config.gyro_scale) << 3;
    if (!_bus->writeRegister(REG_GYRO_CONFIG, gyroConfigVal)) {
        Logger::warning("MPUDriver", "Failed to write GYRO_CONFIG");
        _initialized = false;
        _health.status = SensorStatus::UNINITIALIZED;
        _reading.status = SensorStatus::UNINITIALIZED;
        return false;
    }

    _initialized = true;
    _health.status = SensorStatus::INITIALIZING;
    _reading.status = SensorStatus::INITIALIZING;
    _reading.timestamp_ms = currentMillis();

    Logger::info("MPUDriver", "IMU driver initialized successfully");
    return true;
}

void MPUDriver::update() {
    _reading.timestamp_ms = currentMillis();
    _health.total_measurements++;

    if (!_initialized) {
        _reading.status = SensorStatus::UNINITIALIZED;
        _health.status  = SensorStatus::UNINITIALIZED;
        return;
    }

    // Burst read 14 bytes: 6 accel, 2 temp, 6 gyro starting from REG_ACCEL_XOUT_H (0x3B)
    uint8_t rawBuf[14] = {0};
    if (!_bus->readBytes(REG_ACCEL_XOUT_H, rawBuf, sizeof(rawBuf))) {
        handleFailure(SensorStatus::TIMEOUT);
        return;
    }

    // Parse big-endian 16-bit signed integers
    int16_t rawAx = static_cast<int16_t>((rawBuf[0] << 8) | rawBuf[1]);
    int16_t rawAy = static_cast<int16_t>((rawBuf[2] << 8) | rawBuf[3]);
    int16_t rawAz = static_cast<int16_t>((rawBuf[4] << 8) | rawBuf[5]);

    int16_t rawGx = static_cast<int16_t>((rawBuf[8]  << 8) | rawBuf[9]);
    int16_t rawGy = static_cast<int16_t>((rawBuf[10] << 8) | rawBuf[11]);
    int16_t rawGz = static_cast<int16_t>((rawBuf[12] << 8) | rawBuf[13]);

    // Convert raw register data to physical engineering units
    float accelScale = getAccelScaleFactor(_config.accel_scale);
    float gyroScale  = getGyroScaleFactor(_config.gyro_scale);

    float ax = static_cast<float>(rawAx) / accelScale;
    float ay = static_cast<float>(rawAy) / accelScale;
    float az = static_cast<float>(rawAz) / accelScale;

    float gx = static_cast<float>(rawGx) / gyroScale;
    float gy = static_cast<float>(rawGy) / gyroScale;
    float gz = static_cast<float>(rawGz) / gyroScale;

    handleSuccess(ax, ay, az, gx, gy, gz);
}

bool MPUDriver::isHealthy() const {
    return _initialized &&
           (_health.status == SensorStatus::OK) &&
           (_health.consecutive_failures < _config.max_consecutive_failures);
}

SensorStatus MPUDriver::getStatus() const {
    return _health.status;
}

void MPUDriver::handleFailure(SensorStatus failureStatus) {
    _health.consecutive_failures++;

    if (_health.consecutive_failures >= _config.max_consecutive_failures) {
        _health.status = SensorStatus::FAULT;
        _reading.status = SensorStatus::FAULT;
    } else {
        _health.status = failureStatus;
        _reading.status = failureStatus;
    }
}

void MPUDriver::handleSuccess(float ax, float ay, float az, float gx, float gy, float gz) {
    _health.last_success_ms = currentMillis();
    _health.consecutive_failures = 0;
    _health.successful_measurements++;
    _health.status = SensorStatus::OK;

    _reading.status  = SensorStatus::OK;
    _reading.accel_x = ax;
    _reading.accel_y = ay;
    _reading.accel_z = az;
    _reading.gyro_x  = gx;
    _reading.gyro_y  = gy;
    _reading.gyro_z  = gz;
}

}  // namespace autochair
