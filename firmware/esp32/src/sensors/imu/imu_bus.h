/**
 * @file imu_bus.h
 * @brief AutoChair ESP32 — IMU Hardware Abstraction Layer (Bus interface)
 *
 * Project:  Aivon Innovations — AutoChair
 * Platform: ESP32-WROOM-32D
 * Status:   IN DEVELOPMENT — SOFTWARE ABSTRACTION
 *
 * Defines the abstract communication bus interface (I2C/SPI) for IMU communication.
 * Provides a mock implementation for host testing and an Arduino Wire (I2C) implementation.
 *
 * SAFETY BOUNDARY:
 *   - No physical I2C/SPI transactions are executed when bus is uninitialized or in test mode.
 *   - Pin assignments and I2C addresses remain PLACEHOLDERS until wiring verification.
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>

#ifndef ENV_NATIVE
#  include <Arduino.h>
#  include <Wire.h>
#  ifdef DISABLED
#    undef DISABLED
#  endif
#endif

namespace autochair {

/**
 * @brief Abstract interface for digital bus transactions with the IMU.
 */
class IIMUBus {
public:
    virtual ~IIMUBus() = default;

    /**
     * @brief Initialize the bus interface.
     * @return true if the bus is available and initialized.
     */
    virtual bool begin() = 0;

    /**
     * @brief Read a single 8-bit register from the device.
     */
    virtual bool readRegister(uint8_t reg, uint8_t& value) = 0;

    /**
     * @brief Write a single 8-bit register on the device.
     */
    virtual bool writeRegister(uint8_t reg, uint8_t value) = 0;

    /**
     * @brief Read multiple consecutive bytes starting from a register address.
     */
    virtual bool readBytes(uint8_t startReg, uint8_t* buffer, size_t length) = 0;
};

// =============================================================================
// Hardware I2C Bus Implementation (ESP32 target)
// =============================================================================

class HardwareI2CIMUBus : public IIMUBus {
public:
    explicit HardwareI2CIMUBus(uint8_t address = 0x68)
        : _address(address), _initialized(false) {}

    bool begin() override {
#ifndef ENV_NATIVE
        Wire.begin();
        _initialized = true;
        return true;
#else
        return false;
#endif
    }

    bool readRegister(uint8_t reg, uint8_t& value) override {
#ifndef ENV_NATIVE
        if (!_initialized) return false;
        Wire.beginTransmission(_address);
        Wire.write(reg);
        if (Wire.endTransmission(false) != 0) return false;

        if (Wire.requestFrom(_address, static_cast<uint8_t>(1)) != 1) return false;
        value = static_cast<uint8_t>(Wire.read());
        return true;
#else
        (void)reg; (void)value;
        return false;
#endif
    }

    bool writeRegister(uint8_t reg, uint8_t value) override {
#ifndef ENV_NATIVE
        if (!_initialized) return false;
        Wire.beginTransmission(_address);
        Wire.write(reg);
        Wire.write(value);
        return (Wire.endTransmission() == 0);
#else
        (void)reg; (void)value;
        return false;
#endif
    }

    bool readBytes(uint8_t startReg, uint8_t* buffer, size_t length) override {
#ifndef ENV_NATIVE
        if (!_initialized || buffer == nullptr || length == 0) return false;
        Wire.beginTransmission(_address);
        Wire.write(startReg);
        if (Wire.endTransmission(false) != 0) return false;

        uint8_t readCount = Wire.requestFrom(_address, static_cast<uint8_t>(length));
        if (readCount != length) return false;

        for (size_t i = 0; i < length; ++i) {
            buffer[i] = static_cast<uint8_t>(Wire.read());
        }
        return true;
#else
        (void)startReg; (void)buffer; (void)length;
        return false;
#endif
    }

    void setAddress(uint8_t address) { _address = address; }
    uint8_t getAddress() const { return _address; }

private:
    uint8_t _address;
    bool    _initialized;
};

// =============================================================================
// Mock IMU Bus Implementation (for host unit testing)
// =============================================================================

class MockIMUBus : public IIMUBus {
public:
    static constexpr uint8_t REG_WHO_AM_I     = 0x75;
    static constexpr uint8_t REG_PWR_MGMT_1   = 0x6B;
    static constexpr uint8_t REG_ACCEL_CONFIG = 0x1C;
    static constexpr uint8_t REG_GYRO_CONFIG  = 0x1B;
    static constexpr uint8_t REG_ACCEL_XOUT_H = 0x3B;

    MockIMUBus() {
        reset();
    }

    void reset() {
        std::memset(_registers, 0, sizeof(_registers));
        _registers[REG_WHO_AM_I] = 0x68;  // Default to MPU6050
        _registers[REG_PWR_MGMT_1] = 0x40; // Sleep mode active on reset
        _simulatedInitSuccess = true;
        _simulatedCommFailure = false;
        _readCount = 0;
        _writeCount = 0;
    }

    bool begin() override {
        return _simulatedInitSuccess && !_simulatedCommFailure;
    }

    bool readRegister(uint8_t reg, uint8_t& value) override {
        if (_simulatedCommFailure) return false;
        value = _registers[reg];
        _readCount++;
        return true;
    }

    bool writeRegister(uint8_t reg, uint8_t value) override {
        if (_simulatedCommFailure) return false;
        _registers[reg] = value;
        _writeCount++;
        return true;
    }

    bool readBytes(uint8_t startReg, uint8_t* buffer, size_t length) override {
        if (_simulatedCommFailure || buffer == nullptr) return false;
        for (size_t i = 0; i < length; ++i) {
            uint8_t r = static_cast<uint8_t>(startReg + i);
            buffer[i] = _registers[r];
        }
        _readCount += length;
        return true;
    }

    // --- Mock control helpers ---
    void setWhoAmI(uint8_t whoAmI) {
        _registers[REG_WHO_AM_I] = whoAmI;
    }

    void setRawAccel(int16_t x, int16_t y, int16_t z) {
        _registers[0x3B] = static_cast<uint8_t>((x >> 8) & 0xFF);
        _registers[0x3C] = static_cast<uint8_t>(x & 0xFF);
        _registers[0x3D] = static_cast<uint8_t>((y >> 8) & 0xFF);
        _registers[0x3E] = static_cast<uint8_t>(y & 0xFF);
        _registers[0x3F] = static_cast<uint8_t>((z >> 8) & 0xFF);
        _registers[0x40] = static_cast<uint8_t>(z & 0xFF);
    }

    void setRawGyro(int16_t x, int16_t y, int16_t z) {
        _registers[0x43] = static_cast<uint8_t>((x >> 8) & 0xFF);
        _registers[0x44] = static_cast<uint8_t>(x & 0xFF);
        _registers[0x45] = static_cast<uint8_t>((y >> 8) & 0xFF);
        _registers[0x46] = static_cast<uint8_t>(y & 0xFF);
        _registers[0x47] = static_cast<uint8_t>((z >> 8) & 0xFF);
        _registers[0x48] = static_cast<uint8_t>(z & 0xFF);
    }

    void setCommFailure(bool fail) { _simulatedCommFailure = fail; }
    void setInitSuccess(bool success) { _simulatedInitSuccess = success; }

    uint8_t getRegister(uint8_t reg) const { return _registers[reg]; }
    size_t getReadCount()  const { return _readCount;  }
    size_t getWriteCount() const { return _writeCount; }

private:
    uint8_t _registers[256];
    bool    _simulatedInitSuccess = true;
    bool    _simulatedCommFailure = false;
    size_t  _readCount            = 0;
    size_t  _writeCount           = 0;
};

}  // namespace autochair
