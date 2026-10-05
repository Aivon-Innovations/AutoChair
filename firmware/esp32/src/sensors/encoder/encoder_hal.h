/**
 * @file encoder_hal.h
 * @brief AutoChair ESP32 — Encoder Hardware Abstraction Layer (HAL)
 *
 * Project:  Aivon Innovations — AutoChair
 * Platform: ESP32-WROOM-32D
 * Status:   IN DEVELOPMENT — SOFTWARE / INTERRUPT DRIVEN
 *
 * Provides the hardware abstraction layer for wheel encoder pulse acquisition
 * and quadrature decoding. Decouples the EncoderDriver from direct ESP32 GPIO/ISR
 * calls, enabling deterministic unit testing on host machines.
 *
 * VALIDATED BENCH SPECIFICATIONS (from electrical testing):
 *   - 600 waveform cycles per revolution (per channel).
 *   - 1200 total rising+falling edges per revolution (per channel).
 *   - 3 × 4.7 kΩ pull-ups to 3.3 V logic rail.
 *   - Clockwise (CW): A leads B.
 *   - Counter-Clockwise (CCW): B leads A.
 *   - Z: 1 index event per revolution.
 *
 * DECODING MODES:
 *   - X1: Count on 1 edge of Channel A -> 600 counts/rev.
 *   - X2: Count on both edges of Channel A -> 1200 counts/rev.
 *   - X4: Count on all 4 quadrature edges of A and B -> 2400 counts/rev.
 *
 * SAFETY BOUNDARY:
 *   - When placeholder pins (pin 0) are configured, no GPIO/ISR access occurs.
 *   - Encoder count is purely rotational pulse feedback; no physical motor control.
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include <atomic>

#ifndef ENV_NATIVE
#  include <Arduino.h>
#  ifdef DISABLED
#    undef DISABLED
#  endif
#endif

namespace autochair {

enum class EncoderDecodingMode : uint8_t {
    X1 = 1,  ///< 1 count per cycle (600 counts/rev for 600 PPR)
    X2 = 2,  ///< 2 counts per cycle (1200 counts/rev for 600 PPR)
    X4 = 4   ///< 4 counts per cycle (2400 counts/rev for 600 PPR)
};

/**
 * @brief Abstract interface for quadrature encoder hardware/simulation acquisition.
 */
class IEncoderHAL {
public:
    virtual ~IEncoderHAL() = default;

    /**
     * @brief Configure pins and interrupts for encoder acquisition.
     * @return true if initialized, false if pins are placeholder/invalid.
     */
    virtual bool init(uint8_t pinA, uint8_t pinB, uint8_t pinZ,
                      EncoderDecodingMode mode) = 0;

    /**
     * @brief Get the signed cumulative raw pulse count.
     */
    virtual int64_t getCount() const = 0;

    /**
     * @brief Reset cumulative count to zero.
     */
    virtual void resetCount() = 0;

    /**
     * @brief Get total Z channel index pulses observed.
     */
    virtual uint32_t getIndexCount() const = 0;

    /**
     * @brief Reset index event counter.
     */
    virtual void resetIndexCount() = 0;

    /**
     * @brief Get count of invalid quadrature state transitions (noise / glitch detection).
     */
    virtual uint32_t getInvalidTransitionCount() const = 0;

    /**
     * @brief Timestamp in ms of the most recent interrupt / pulse event.
     */
    virtual uint32_t getLastInterruptTimestampMs() const = 0;
};

// =============================================================================
// Hardware Encoder HAL Implementation (ESP32 Interrupts)
// =============================================================================

/**
 * @brief Production ESP32 interrupt-driven quadrature decoder.
 * Supports up to 2 instances (Left = 0, Right = 1).
 */
class HardwareEncoderHAL : public IEncoderHAL {
public:
    explicit HardwareEncoderHAL(uint8_t instanceId = 0);

    bool init(uint8_t pinA, uint8_t pinB, uint8_t pinZ,
              EncoderDecodingMode mode) override;

    int64_t getCount() const override;
    void resetCount() override;

    uint32_t getIndexCount() const override;
    void resetIndexCount() override;

    uint32_t getInvalidTransitionCount() const override;
    uint32_t getLastInterruptTimestampMs() const override;

    void handlePinChange();
    void handleIndex();

private:
    uint8_t             _instanceId;
    uint8_t             _pinA;
    uint8_t             _pinB;
    uint8_t             _pinZ;
    EncoderDecodingMode _mode;
    bool                _initialized;

    volatile int64_t    _count;
    volatile uint32_t   _indexCount;
    volatile uint32_t   _invalidTransitions;
    volatile uint32_t   _lastInterruptMs;
    volatile uint8_t    _prevState;

    static HardwareEncoderHAL* _instances[2];

#ifndef ENV_NATIVE
    static void IRAM_ATTR isrA0();
    static void IRAM_ATTR isrB0();
    static void IRAM_ATTR isrZ0();

    static void IRAM_ATTR isrA1();
    static void IRAM_ATTR isrB1();
    static void IRAM_ATTR isrZ1();
#endif
};

// =============================================================================
// Mock / Simulation Encoder HAL Implementation (for host unit testing)
// =============================================================================

/**
 * @brief Mock Encoder HAL for deterministic simulation and unit testing.
 *
 * Allows test suites to step quadrature signals forward/backward, trigger index events,
 * and simulate noise/glitches without hardware.
 */
class MockEncoderHAL : public IEncoderHAL {
public:
    explicit MockEncoderHAL(EncoderDecodingMode mode = EncoderDecodingMode::X4)
        : _mode(mode), _count(0), _indexCount(0), _invalidTransitions(0),
          _lastInterruptMs(0), _quadState(0), _initSuccess(true), _initialized(false) {}

    bool init(uint8_t pinA, uint8_t pinB, uint8_t pinZ,
              EncoderDecodingMode mode) override
    {
        (void)pinZ;
        _mode = mode;
        if (pinA == 0 || pinB == 0) {
            _initialized = false;
            return false;
        }
        _initialized = _initSuccess;
        return _initialized;
    }

    int64_t getCount() const override {
        return _count;
    }

    void resetCount() override {
        _count = 0;
    }

    uint32_t getIndexCount() const override {
        return _indexCount;
    }

    void resetIndexCount() override {
        _indexCount = 0;
    }

    uint32_t getInvalidTransitionCount() const override {
        return _invalidTransitions;
    }

    uint32_t getLastInterruptTimestampMs() const override {
        return _lastInterruptMs;
    }

    // --- Simulation control helpers ---

    /**
     * @brief Step the encoder in forward (+1) or reverse (-1) quadrature direction.
     * Follows the Gray code sequence:
     * Forward (CW: A leads B):  00 -> 10 -> 11 -> 01 -> 00
     * Reverse (CCW: B leads A): 00 -> 01 -> 11 -> 10 -> 00
     */
    void stepQuad(int8_t direction, size_t steps = 1, uint32_t timestampMs = 0) {
        static const uint8_t CW_SEQ[4]  = { 0, 2, 3, 1 }; // 00, 10, 11, 01
        static const uint8_t CCW_SEQ[4] = { 0, 1, 3, 2 }; // 00, 01, 11, 10

        for (size_t s = 0; s < steps; ++s) {
            uint8_t prev = _quadState;
            if (direction > 0) {
                // Forward (CW)
                _quadState = (_quadState + 1) % 4;
            } else if (direction < 0) {
                // Reverse (CCW)
                _quadState = (_quadState + 3) % 4;
            }

            // Apply decoding mode multiplier
            if (_mode == EncoderDecodingMode::X4) {
                _count += (direction > 0) ? 1 : -1;
            } else if (_mode == EncoderDecodingMode::X2) {
                // X2: count every 2 steps
                if ((s % 2) == 1) {
                    _count += (direction > 0) ? 1 : -1;
                }
            } else if (_mode == EncoderDecodingMode::X1) {
                // X1: count every 4 steps (1 full cycle)
                if ((s % 4) == 3) {
                    _count += (direction > 0) ? 1 : -1;
                }
            }

            _lastInterruptMs = timestampMs;
            (void)prev;
        }
    }

    /**
     * @brief Simulate rotation of exact number of full revolutions.
     * In X4 mode: 600 cycles * 4 = 2400 counts per rev.
     * In X2 mode: 600 cycles * 2 = 1200 counts per rev.
     * In X1 mode: 600 cycles * 1 = 600 counts per rev.
     */
    void rotateRevolutions(float revs, int8_t direction = 1, uint32_t timestampMs = 0) {
        size_t totalSteps = static_cast<size_t>(revs * 600.0f * 4.0f);
        stepQuad(direction, totalSteps, timestampMs);

        // Add 1 index event per full revolution completed
        uint32_t fullRevs = static_cast<uint32_t>(revs > 0 ? revs : -revs);
        _indexCount += fullRevs;
    }

    void triggerIndex(uint32_t timestampMs = 0) {
        _indexCount++;
        _lastInterruptMs = timestampMs;
    }

    void injectInvalidTransition() {
        _invalidTransitions++;
    }

    void setCount(int64_t count) { _count = count; }
    void setInitSuccess(bool success) { _initSuccess = success; }
    void setMode(EncoderDecodingMode mode) { _mode = mode; }

private:
    EncoderDecodingMode _mode;
    int64_t             _count;
    uint32_t            _indexCount;
    uint32_t            _invalidTransitions;
    uint32_t            _lastInterruptMs;
    uint8_t             _quadState;
    bool                _initSuccess;
    bool                _initialized;
};

}  // namespace autochair
