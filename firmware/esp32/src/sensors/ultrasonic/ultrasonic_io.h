/**
 * @file ultrasonic_io.h
 * @brief AutoChair ESP32 — Ultrasonic Hardware Abstraction Layer (IO interface)
 *
 * Project:  Aivon Innovations — AutoChair
 * Platform: ESP32-WROOM-32D
 * Status:   IN DEVELOPMENT — SOFTWARE ABSTRACTION ONLY
 *
 * Provides the hardware abstraction layer (HAL) for ultrasonic pulse generation
 * and measurement. Decouples HCSR04Driver from direct Arduino/ESP32 GPIO calls,
 * enabling deterministic unit testing on host machines and safe simulation.
 *
 * SAFETY BOUNDARY:
 *   - No physical GPIO pins are activated when placeholder pins (pin 0) are configured.
 *   - HC-SR04 ECHO line is 5V logic. Level-shifting must be verified before physical wiring.
 */

#pragma once

#include <cstdint>
#include <cstddef>

#ifndef ENV_NATIVE
#  include <Arduino.h>
#  ifdef DISABLED
#    undef DISABLED
#  endif
#endif

namespace autochair {

/**
 * @brief Abstract interface for ultrasonic hardware pin operations and timing.
 */
class IUltrasonicIO {
public:
    virtual ~IUltrasonicIO() = default;

    /**
     * @brief Configure trigger and echo pin modes.
     * @param trigPin Output trigger pin.
     * @param echoPin Input echo pin.
     * @return true if pins are valid and configured, false if placeholder/invalid.
     */
    virtual bool initPins(uint8_t trigPin, uint8_t echoPin) = 0;

    /**
     * @brief Trigger the sensor and measure echo pulse duration in microseconds.
     *
     * @param trigPin Output trigger pin.
     * @param echoPin Input echo pin.
     * @param timeoutUs Maximum time to wait for echo pulse.
     * @return Measured pulse duration in microseconds, or 0 on timeout/error.
     */
    virtual uint32_t measurePulseUs(uint8_t trigPin, uint8_t echoPin, uint32_t timeoutUs) = 0;
};

// =============================================================================
// Hardware IO implementation (for ESP32 target)
// =============================================================================

/**
 * @brief Concrete hardware IO implementation for Arduino/ESP32.
 *
 * When pins are set to 0 (PLACEHOLDER), hardware operations are safely inhibited.
 */
class HardwareUltrasonicIO : public IUltrasonicIO {
public:
    bool initPins(uint8_t trigPin, uint8_t echoPin) override {
        // Safety guard: placeholder pin 0 is considered unverified.
        if (trigPin == 0 || echoPin == 0) {
            return false;
        }

#ifndef ENV_NATIVE
        pinMode(trigPin, OUTPUT);
        digitalWrite(trigPin, LOW);
        pinMode(echoPin, INPUT);
        return true;
#else
        return false;
#endif
    }

    uint32_t measurePulseUs(uint8_t trigPin, uint8_t echoPin, uint32_t timeoutUs) override {
        // Safety guard: do not access placeholder pins.
        if (trigPin == 0 || echoPin == 0) {
            return 0;
        }

#ifndef ENV_NATIVE
        // Standard HC-SR04 trigger sequence: 10 us HIGH pulse
        digitalWrite(trigPin, LOW);
        delayMicroseconds(2);
        digitalWrite(trigPin, HIGH);
        delayMicroseconds(10);
        digitalWrite(trigPin, LOW);

        // Measure HIGH duration on ECHO pin
        return static_cast<uint32_t>(pulseIn(echoPin, HIGH, timeoutUs));
#else
        (void)timeoutUs;
        return 0;
#endif
    }
};

// =============================================================================
// Mock / Simulation IO implementation (for host unit testing)
// =============================================================================

/**
 * @brief Mock IO implementation for deterministic testing and simulation.
 *
 * Allows test suites to simulate valid distances, out-of-range pulses, timeouts,
 * and hardware disconnection without physical sensors.
 */
class MockUltrasonicIO : public IUltrasonicIO {
public:
    enum class MockMode {
        FIXED_PULSE,        ///< Always return configured pulse duration.
        PULSE_SEQUENCE,     ///< Return durations from a predefined array.
        TIMEOUT,            ///< Return 0 (timeout / no echo).
        PIN_FAIL            ///< Fail initPins (simulate invalid pin config).
    };

    explicit MockUltrasonicIO(MockMode mode = MockMode::FIXED_PULSE, uint32_t pulseUs = 1166)
        : _mode(mode), _pulseUs(pulseUs) {}

    bool initPins(uint8_t trigPin, uint8_t echoPin) override {
        _lastTrigPin = trigPin;
        _lastEchoPin = echoPin;
        _initCount++;
        return (_mode != MockMode::PIN_FAIL);
    }

    uint32_t measurePulseUs(uint8_t trigPin, uint8_t echoPin, uint32_t timeoutUs) override {
        (void)trigPin;
        (void)echoPin;
        (void)timeoutUs;
        _measureCount++;

        switch (_mode) {
            case MockMode::FIXED_PULSE:
                return _pulseUs;

            case MockMode::PULSE_SEQUENCE:
                if (_sequenceLength > 0 && _sequenceIndex < _sequenceLength) {
                    return _sequence[_sequenceIndex++];
                }
                return _pulseUs;

            case MockMode::TIMEOUT:
            case MockMode::PIN_FAIL:
            default:
                return 0;
        }
    }

    // --- Configuration helpers for test cases ---
    void setMode(MockMode mode) { _mode = mode; }
    void setPulseUs(uint32_t pulseUs) { _pulseUs = pulseUs; }
    void setDistanceMm(float distanceMm) {
        // Round trip: distance = (pulse * 0.343) / 2 = pulse * 0.1715
        // pulse = distance / 0.1715
        _pulseUs = static_cast<uint32_t>(distanceMm / 0.1715f);
    }

    void setSequence(const uint32_t* sequence, size_t length) {
        _sequence = sequence;
        _sequenceLength = length;
        _sequenceIndex = 0;
        _mode = MockMode::PULSE_SEQUENCE;
    }

    uint32_t getMeasureCount() const { return _measureCount; }
    uint32_t getInitCount()    const { return _initCount; }
    uint8_t  getLastTrigPin()   const { return _lastTrigPin; }
    uint8_t  getLastEchoPin()   const { return _lastEchoPin; }

    void resetCounts() {
        _measureCount = 0;
        _initCount = 0;
        _sequenceIndex = 0;
    }

private:
    MockMode        _mode           = MockMode::FIXED_PULSE;
    uint32_t        _pulseUs        = 1166;  ///< Default ~200 mm
    const uint32_t* _sequence       = nullptr;
    size_t          _sequenceLength = 0;
    size_t          _sequenceIndex  = 0;
    uint32_t        _measureCount   = 0;
    uint32_t        _initCount      = 0;
    uint8_t         _lastTrigPin    = 0;
    uint8_t         _lastEchoPin    = 0;
};

}  // namespace autochair
