/**
 * @file encoder_stub.h
 * @brief AutoChair ESP32 — Encoder stub / placeholder
 *
 * Project:  Aivon Innovations — AutoChair
 * Platform: ESP32-WROOM-32D
 * Status:   IN DEVELOPMENT — HARDWARE STUB ONLY
 *
 * The actual encoder driver will be implemented in Phase 6 after:
 *   - Encoder supply voltage and output voltage verified.
 *   - A/B/Z signal levels verified for ESP32 GPIO compatibility.
 *   - Quadrature decoding mode selected (counts/rev may NOT be 4×PPR).
 *   - Physical mounting location established.
 *   (ENCODER_INTERFACE.md §5, §9)
 *
 * CRITICAL WARNINGS:
 *   - 600 PPR does NOT automatically mean 2400 counts/revolution.
 *     The decoding mode must be confirmed experimentally.
 *   - Motor-to-wheel gear ratio (≈23.3:1 observed) must NOT be used
 *     in calibration until encoder mounting is verified.
 *   - Direction convention (A leads B = forward) must NOT be assumed;
 *     verify physically.
 */

#pragma once

#include "../sensor_interface.h"
#include "../../diagnostics/logger.h"

namespace autochair {

/**
 * @brief Stub encoder — counts remain zero, status UNINITIALIZED.
 *
 * Replace with the real encoder driver in Phase 6.
 */
class EncoderStub : public ISensor {
public:
    explicit EncoderStub(uint8_t encoderId, const char* label) : _label(label) {
        _reading.sensor_id = encoderId;
        _reading.status    = SensorStatus::UNINITIALIZED;
    }

    bool begin() override {
        Logger::info("Encoder", "Stub — hardware not connected (Phase 6)");
        _reading.status = SensorStatus::UNINITIALIZED;
        return true;
    }

    void update() override {
        // No hardware.
    }

    bool isHealthy() const override {
        return false;
    }

    SensorStatus getStatus() const override {
        return _reading.status;
    }

    const EncoderReading& getReading() const { return _reading; }

private:
    EncoderReading _reading{};
    const char*    _label;
};

}  // namespace autochair
