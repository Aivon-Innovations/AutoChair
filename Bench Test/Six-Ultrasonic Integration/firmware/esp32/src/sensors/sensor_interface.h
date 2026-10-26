/**
 * @file sensor_interface.h
 * @brief AutoChair ESP32 — Abstract IO interface for ultrasonic sensor hardware
 *
 * Project:  Aivon Innovations — AutoChair
 * Platform: ESP32-WROOM-32D
 * Branch:   alvira-dev
 * Status:   IN DEVELOPMENT
 *
 * Defines the pure-virtual UltrasonicIO interface used by HCSR04Driver.
 * Concrete implementations:
 *   HardwareUltrasonicIO — wraps pulseIn/digitalWrite/millis (ESP32 build only)
 *   MockUltrasonicIO     — injected by native tests (test/mock_ultrasonic_io.h)
 *
 * This abstraction allows the driver to be fully testable on a host without
 * any Arduino hardware or GPIO dependencies.
 */

#pragma once

#include <cstdint>

namespace autochair {

/**
 * @brief Abstract hardware IO interface for one HC-SR04 sensor channel.
 *
 * The driver sends a trigger pulse and waits for an echo via this interface.
 * The interface owns the complete timing sequence so that mocks can return
 * arbitrary pulse durations without any real GPIO access.
 */
class UltrasonicIO {
public:
    virtual ~UltrasonicIO() = default;

    /**
     * @brief Issue a 10-µs trigger pulse and measure the resulting echo.
     *
     * On hardware this maps to:
     *   digitalWrite(trig_pin, LOW);  delayMicroseconds(4);
     *   digitalWrite(trig_pin, HIGH); delayMicroseconds(10);
     *   digitalWrite(trig_pin, LOW);
     *   return pulseIn(echo_pin, HIGH, timeout_us);
     *
     * @param trig_pin   GPIO number for the TRIG output.
     * @param echo_pin   GPIO number for the ECHO input.
     * @param timeout_us Maximum wait time in µs; return 0 on timeout.
     * @return Echo pulse duration in µs, or 0 if no echo within timeout.
     */
    virtual uint32_t measurePulseUs(uint8_t trig_pin,
                                     uint8_t echo_pin,
                                     uint32_t timeout_us) = 0;

    /**
     * @brief Return current time in milliseconds.
     *
     * On hardware: millis(). In tests: a controllable counter.
     */
    virtual uint32_t getMillis() = 0;
};

}  // namespace autochair
