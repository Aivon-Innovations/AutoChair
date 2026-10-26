/**
 * @file hardware_ultrasonic_io.h
 * @brief AutoChair ESP32 — Concrete hardware IO for HC-SR04 (ESP32 target only)
 *
 * Project:  Aivon Innovations — AutoChair
 * Platform: ESP32-WROOM-32D
 * Branch:   alvira-dev
 * Status:   IN DEVELOPMENT
 *
 * This file is only included in the ESP32 (arduino framework) build.
 * It wraps the standard Arduino pulseIn / digitalWrite / millis functions.
 *
 * NOT included in native test builds (ENV_NATIVE). Tests use MockUltrasonicIO.
 */

#pragma once

#ifndef ENV_NATIVE

#include "sensors/sensor_interface.h"
#include <Arduino.h>

namespace autochair {

/**
 * @brief Hardware UltrasonicIO implementation for the ESP32 Arduino target.
 *
 * Issues the 10-µs TRIG pulse and measures the resulting ECHO pulse using
 * the standard Arduino pulseIn() function.
 */
class HardwareUltrasonicIO : public UltrasonicIO {
public:
    /**
     * @brief Trigger sensor and measure echo pulse duration.
     *
     * Sequence:
     *   1. Drive TRIG LOW for 4 µs (settle)
     *   2. Drive TRIG HIGH for 10 µs (trigger burst)
     *   3. Drive TRIG LOW
     *   4. Wait for ECHO HIGH pulse (up to timeout_us µs)
     *
     * @return Pulse duration in µs, or 0 on timeout.
     */
    uint32_t measurePulseUs(uint8_t trig_pin,
                             uint8_t echo_pin,
                             uint32_t timeout_us) override {
        digitalWrite(trig_pin, LOW);
        delayMicroseconds(4);
        digitalWrite(trig_pin, HIGH);
        delayMicroseconds(10);
        digitalWrite(trig_pin, LOW);
        return static_cast<uint32_t>(pulseIn(echo_pin, HIGH, timeout_us));
    }

    /**
     * @brief Return Arduino millisecond counter.
     */
    uint32_t getMillis() override {
        return millis();
    }
};

}  // namespace autochair

#endif  // !ENV_NATIVE
