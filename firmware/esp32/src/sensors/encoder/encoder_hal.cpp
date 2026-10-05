/**
 * @file encoder_hal.cpp
 * @brief AutoChair ESP32 — Encoder Hardware Abstraction Layer implementation
 *
 * Project:  Aivon Innovations — AutoChair
 * Platform: ESP32-WROOM-32D
 * Status:   IN DEVELOPMENT
 */

#include "encoder_hal.h"

namespace autochair {

HardwareEncoderHAL* HardwareEncoderHAL::_instances[2] = { nullptr, nullptr };

#ifndef ENV_NATIVE
void IRAM_ATTR HardwareEncoderHAL::isrA0() { if (_instances[0]) _instances[0]->handlePinChange(); }
void IRAM_ATTR HardwareEncoderHAL::isrB0() { if (_instances[0]) _instances[0]->handlePinChange(); }
void IRAM_ATTR HardwareEncoderHAL::isrZ0() { if (_instances[0]) _instances[0]->handleIndex(); }

void IRAM_ATTR HardwareEncoderHAL::isrA1() { if (_instances[1]) _instances[1]->handlePinChange(); }
void IRAM_ATTR HardwareEncoderHAL::isrB1() { if (_instances[1]) _instances[1]->handlePinChange(); }
void IRAM_ATTR HardwareEncoderHAL::isrZ1() { if (_instances[1]) _instances[1]->handleIndex(); }
#endif

HardwareEncoderHAL::HardwareEncoderHAL(uint8_t instanceId)
    : _instanceId(instanceId), _pinA(0), _pinB(0), _pinZ(0),
      _mode(EncoderDecodingMode::X4), _initialized(false),
      _count(0), _indexCount(0), _invalidTransitions(0),
      _lastInterruptMs(0), _prevState(0)
{
    if (_instanceId < 2) {
        _instances[_instanceId] = this;
    }
}

bool HardwareEncoderHAL::init(uint8_t pinA, uint8_t pinB, uint8_t pinZ,
                              EncoderDecodingMode mode)
{
    _pinA = pinA;
    _pinB = pinB;
    _pinZ = pinZ;
    _mode = mode;

    // Safety guard: placeholder pin 0 is considered unverified
    if (_pinA == 0 || _pinB == 0) {
        _initialized = false;
        return false;
    }

#ifndef ENV_NATIVE
    pinMode(_pinA, INPUT_PULLUP);
    pinMode(_pinB, INPUT_PULLUP);

    // Read initial state
    uint8_t a = digitalRead(_pinA);
    uint8_t b = digitalRead(_pinB);
    _prevState = (a << 1) | b;

    // Attach interrupts based on instance ID
    if (_instanceId == 0) {
        attachInterrupt(digitalPinToInterrupt(_pinA), isrA0, CHANGE);
        attachInterrupt(digitalPinToInterrupt(_pinB), isrB0, CHANGE);
        if (_pinZ != 0) {
            pinMode(_pinZ, INPUT_PULLUP);
            attachInterrupt(digitalPinToInterrupt(_pinZ), isrZ0, RISING);
        }
    } else if (_instanceId == 1) {
        attachInterrupt(digitalPinToInterrupt(_pinA), isrA1, CHANGE);
        attachInterrupt(digitalPinToInterrupt(_pinB), isrB1, CHANGE);
        if (_pinZ != 0) {
            pinMode(_pinZ, INPUT_PULLUP);
            attachInterrupt(digitalPinToInterrupt(_pinZ), isrZ1, RISING);
        }
    }

    _initialized = true;
    return true;
#else
    return false;
#endif
}

int64_t HardwareEncoderHAL::getCount() const {
    return _count;
}

void HardwareEncoderHAL::resetCount() {
    _count = 0;
}

uint32_t HardwareEncoderHAL::getIndexCount() const {
    return _indexCount;
}

void HardwareEncoderHAL::resetIndexCount() {
    _indexCount = 0;
}

uint32_t HardwareEncoderHAL::getInvalidTransitionCount() const {
    return _invalidTransitions;
}

uint32_t HardwareEncoderHAL::getLastInterruptTimestampMs() const {
    return _lastInterruptMs;
}

void HardwareEncoderHAL::handlePinChange() {
#ifndef ENV_NATIVE
    uint8_t a = digitalRead(_pinA);
    uint8_t b = digitalRead(_pinB);
    uint8_t currState = (a << 1) | b;

    // Standard 4-bit quadrature transition lookup table
    // Index: (prevState << 2) | currState
    // Values: +1 (CW: A leads B), -1 (CCW: B leads A), 0 (invalid or same)
    static const int8_t QUAD_TABLE[16] = {
         0, -1,  1,  0,
         1,  0,  0, -1,
        -1,  0,  0,  1,
         0,  1, -1,  0
    };

    uint8_t tableIdx = static_cast<uint8_t>((_prevState << 2) | currState);
    int8_t step = QUAD_TABLE[tableIdx];

    if (step != 0) {
        if (_mode == EncoderDecodingMode::X4) {
            _count += step;
        } else if (_mode == EncoderDecodingMode::X2) {
            // Count only on A channel changes
            if ((_prevState ^ currState) & 0x02) {
                _count += step;
            }
        } else if (_mode == EncoderDecodingMode::X1) {
            // Count only on A channel rising edge (0 -> 1)
            if ((_prevState & 0x02) == 0 && (currState & 0x02) != 0) {
                _count += step;
            }
        }
        _lastInterruptMs = millis();
    } else if (_prevState != currState) {
        // Double transition / noise glitch
        _invalidTransitions++;
    }

    _prevState = currState;
#endif
}

void HardwareEncoderHAL::handleIndex() {
#ifndef ENV_NATIVE
    _indexCount++;
    _lastInterruptMs = millis();
#endif
}

}  // namespace autochair
