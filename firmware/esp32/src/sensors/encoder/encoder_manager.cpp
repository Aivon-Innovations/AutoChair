/**
 * @file encoder_manager.cpp
 * @brief AutoChair ESP32 — Dual Wheel Encoder Manager implementation
 *
 * Project:  Aivon Innovations — AutoChair
 * Platform: ESP32-WROOM-32D
 * Status:   IN DEVELOPMENT
 */

#include "encoder_manager.h"
#include "diagnostics/logger.h"

namespace autochair {

static EncoderConfig makeDefaultLeftConfig() {
    EncoderConfig cfg;
    cfg.sensor_id         = 0;
    cfg.label             = "LEFT";
    cfg.pin_a             = config::ENCODER_LEFT_A_PIN;
    cfg.pin_b             = config::ENCODER_LEFT_B_PIN;
    cfg.pin_z             = config::ENCODER_LEFT_Z_PIN;
    cfg.ppr               = config::ENCODER_PPR;
    cfg.decoding_mode     = EncoderDecodingMode::X4;
    cfg.reverse_direction = false;
    return cfg;
}

static EncoderConfig makeDefaultRightConfig() {
    EncoderConfig cfg;
    cfg.sensor_id         = 1;
    cfg.label             = "RIGHT";
    cfg.pin_a             = config::ENCODER_RIGHT_A_PIN;
    cfg.pin_b             = config::ENCODER_RIGHT_B_PIN;
    cfg.pin_z             = config::ENCODER_RIGHT_Z_PIN;
    cfg.ppr               = config::ENCODER_PPR;
    cfg.decoding_mode     = EncoderDecodingMode::X4;
    cfg.reverse_direction = false;
    return cfg;
}

EncoderManager::EncoderManager()
    : _leftDriver(makeDefaultLeftConfig()),
      _rightDriver(makeDefaultRightConfig())
{
    _wheelData.left  = _leftDriver.getReading();
    _wheelData.right = _rightDriver.getReading();
}

EncoderManager::EncoderManager(const EncoderConfig& leftConfig, const EncoderConfig& rightConfig)
    : _leftDriver(leftConfig),
      _rightDriver(rightConfig)
{
    _wheelData.left  = _leftDriver.getReading();
    _wheelData.right = _rightDriver.getReading();
}

bool EncoderManager::begin() {
    Logger::info("EncoderManager", "Initializing wheel encoder manager");

    bool leftOk  = _leftDriver.begin();
    bool rightOk = _rightDriver.begin();

    _wheelData.left  = _leftDriver.getReading();
    _wheelData.right = _rightDriver.getReading();

    return leftOk || rightOk;
}

void EncoderManager::update() {
    _leftDriver.update();
    _rightDriver.update();

    _wheelData.left  = _leftDriver.getReading();
    _wheelData.right = _rightDriver.getReading();
}

void EncoderManager::resetCounts() {
    _leftDriver.resetCount();
    _rightDriver.resetCount();
    _wheelData.left  = _leftDriver.getReading();
    _wheelData.right = _rightDriver.getReading();
}

bool EncoderManager::areAllHealthy() const {
    return _leftDriver.isHealthy() && _rightDriver.isHealthy();
}

}  // namespace autochair
