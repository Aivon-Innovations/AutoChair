/**
 * @file encoder_driver.h
 * @brief AutoChair ESP32 — Wheel Encoder Concrete Driver
 *
 * Project:  Aivon Innovations — AutoChair
 * Platform: ESP32-WROOM-32D
 * Status:   IN DEVELOPMENT — SOFTWARE DRIVER (BENCH / SIMULATION SUPPORTED)
 *
 * Implements the concrete wheel encoder driver according to ENCODER_INTERFACE.md
 * and SENSOR_INTERFACE.md.
 *
 * Key features:
 *   - ISensor base implementation.
 *   - Configurable quadrature decoding mode:
 *       * X1: 600 counts/rev (1 count per waveform cycle)
 *       * X2: 1200 counts/rev (2 counts per waveform cycle)
 *       * X4: 2400 counts/rev (4 counts per waveform cycle)
 *   - Configurable direction polarity (no silent inversion).
 *   - Signed raw pulse counting with atomic HAL acquisition.
 *   - Direction detection (FORWARD, REVERSE, STATIONARY).
 *   - Z-channel index event tracking.
 *   - Hardware isolation via IEncoderHAL (supporting ESP32 interrupts and host mocks).
 *
 * SAFETY BOUNDARY (MANDATORY):
 *   - Placeholder pin assignments (pin 0) safely inhibit GPIO access.
 *   - Distance/velocity conversions are deferred until physical calibration.
 *   - No encoder logic directly actuates motors or brakes.
 */

#pragma once

#include "../sensor_interface.h"
#include "encoder_hal.h"
#include "autochair_types.h"
#include "autochair_config.h"

#include <cstdint>
#include <cstddef>

namespace autochair {

struct EncoderConfig {
    uint8_t             sensor_id         = 0;
    const char*         label             = "LEFT";
    uint8_t             pin_a             = 0;  ///< Channel A GPIO (PLACEHOLDER).
    uint8_t             pin_b             = 0;  ///< Channel B GPIO (PLACEHOLDER).
    uint8_t             pin_z             = 0;  ///< Channel Z index GPIO (PLACEHOLDER, 0 = disabled).
    uint16_t            ppr               = 600; ///< Waveform cycles per revolution (manufacturer spec).
    EncoderDecodingMode decoding_mode     = EncoderDecodingMode::X4;
    bool                reverse_direction = false; ///< Polarity inversion parameter.
};

struct EncoderHealth {
    uint32_t     last_pulse_ms       = 0;
    uint32_t     total_pulses        = 0;
    uint32_t     index_events        = 0;
    uint32_t     invalid_transitions = 0;
    SensorStatus status              = SensorStatus::UNINITIALIZED;
};

class EncoderDriver : public ISensor {
public:
    explicit EncoderDriver(const EncoderConfig& config, IEncoderHAL* hal = nullptr);

    // -------------------------------------------------------------------------
    // ISensor Interface
    // -------------------------------------------------------------------------

    bool begin() override;
    void update() override;
    void update(uint32_t timestampMs);
    bool isHealthy() const override;
    SensorStatus getStatus() const override;

    // -------------------------------------------------------------------------
    // Accessors
    // -------------------------------------------------------------------------

    const EncoderReading& getReading() const { return _reading; }
    const EncoderHealth&  getHealth()  const { return _health;  }
    const EncoderConfig&  getConfig()  const { return _config;  }
    const char*           getLabel()   const { return _config.label; }

    int64_t  getCount()      const { return _reading.count; }
    uint32_t getIndexCount() const { return _health.index_events; }

    void setHAL(IEncoderHAL* hal);
    void resetCount();
    void reset();

    /**
     * @brief Expected counts per revolution for the configured decoding mode.
     */
    uint32_t getCountsPerRevolution() const {
        return static_cast<uint32_t>(_config.ppr) * static_cast<uint32_t>(_config.decoding_mode);
    }

private:
    EncoderConfig      _config;
    IEncoderHAL*       _hal;
    HardwareEncoderHAL _defaultHardwareHAL;
    EncoderReading     _reading{};
    EncoderHealth      _health{};
    int64_t            _lastHalCount = 0;
    bool               _initialized  = false;
};

}  // namespace autochair
