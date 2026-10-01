/**
 * @file command_handler.cpp
 * @brief AutoChair ESP32 — Command handler implementation
 */

#include "command_handler.h"
#include "../diagnostics/logger.h"

namespace autochair {

CommandHandler::CommandHandler(StateMachine& sm, SafetyManager& safety)
    : _sm(sm), _safety(safety) {}

// =============================================================================
// Main dispatch
// =============================================================================

protocol::ResponsePayload CommandHandler::handleCommand(
    const protocol::CommandPayload& cmd, uint16_t /*msgId*/)
{
    // Step 1 — Is the command recognized?
    if (cmd.command == CommandId::UNKNOWN) {
        Logger::warning("CommandHandler", "Unknown command received");
        return makeNack(cmd.command, NackReason::INVALID_COMMAND);
    }

    // Step 2 — Is the command permitted in the current state?
    if (!_sm.isCommandPermitted(cmd.command)) {
        Logger::warning("CommandHandler", "Command not permitted in current state");
        return makeNack(cmd.command, NackReason::INVALID_STATE);
    }

    // Step 3 — Dispatch to specific handler.
    switch (cmd.command) {
        case CommandId::PING:        return handlePing();
        case CommandId::GET_STATUS:  return handleGetStatus();
        case CommandId::SET_MODE:    return handleSetMode(cmd.mode);
        case CommandId::ENABLE:      return handleEnable();
        case CommandId::DISABLE:     return handleDisable();
        case CommandId::STOP:        return handleStop();
        case CommandId::RESET_FAULT: return handleResetFault();

        case CommandId::GET_SENSOR_DATA:
        case CommandId::GET_ENCODER_DATA:
        case CommandId::GET_IMU_DATA:
            // Phase 4–6: sensor data commands return stub data for now.
            Logger::info("CommandHandler", "Sensor data command — stubs active");
            return makeOk(cmd.command);

        default:
            return makeNack(cmd.command, NackReason::NOT_SUPPORTED);
    }
}

// =============================================================================
// Individual handlers
// =============================================================================

protocol::ResponsePayload CommandHandler::handlePing() {
    return makeOk(CommandId::PING);
}

protocol::ResponsePayload CommandHandler::handleGetStatus() {
    return makeOk(CommandId::GET_STATUS);
}

protocol::ResponsePayload CommandHandler::handleSetMode(OperatingMode mode) {
    NackReason result = _sm.requestModeChange(mode, "SET_MODE command");
    if (result != NackReason::NONE) {
        return makeNack(CommandId::SET_MODE, result);
    }
    return makeOk(CommandId::SET_MODE);
}

protocol::ResponsePayload CommandHandler::handleEnable() {
    if (_safety.getSafetyState() == SafetyState::SAFE) {
        Logger::warning("CommandHandler", "ENABLE rejected: safety SAFE");
        return makeNack(CommandId::ENABLE, NackReason::SAFETY_RESTRICTED);
    }
    if (!_sm.canEnable()) {
        Logger::warning("CommandHandler", "ENABLE rejected: guard failed");
        return makeNack(CommandId::ENABLE, NackReason::NOT_READY);
    }
    _sm.processEvent(SystemEvent::ENABLE_REQUESTED, "Pi command");
    return makeOk(CommandId::ENABLE);
}

protocol::ResponsePayload CommandHandler::handleDisable() {
    _sm.processEvent(SystemEvent::DISABLE_REQUESTED, "Pi command");
    return makeOk(CommandId::DISABLE);
}

protocol::ResponsePayload CommandHandler::handleStop() {
    // STOP is high-priority; no state guard (it's always allowed).
    _sm.processEvent(SystemEvent::STOP_REQUESTED, "Pi STOP command");
    return makeOk(CommandId::STOP);
}

protocol::ResponsePayload CommandHandler::handleResetFault() {
    if (_safety.getSafetyState() == SafetyState::SAFE) {
        NackReason result = _safety.requestRecovery();
        if (result != NackReason::NONE) {
            return makeNack(CommandId::RESET_FAULT, result);
        }
    }
    _sm.processEvent(SystemEvent::RESET_REQUESTED, "Pi RESET_FAULT");
    return makeOk(CommandId::RESET_FAULT);
}

// =============================================================================
// Payload builders
// =============================================================================

protocol::SystemStatusPayload CommandHandler::buildStatusPayload() const {
    protocol::SystemStatusPayload p{};
    p.system_state    = _sm.getState();
    p.operating_mode  = _sm.getMode();
    p.safety_state    = _sm.getSafety();
    p.enabled         = _sm.isEnabled();
    p.uptime_ms       = _sm.uptimeMs();
    p.active_faults   = 0;  // Fault registry not yet implemented.
    p.wheelchair_mode = WheelchairInterfaceMode::SIMULATION;
    return p;
}

protocol::CapabilitiesPayload CommandHandler::buildCapabilitiesPayload() const {
    protocol::CapabilitiesPayload c{};
    // Sensors and wheelchair control remain false until Phases 4–6 are complete.
    c.has_ultrasonic     = false;
    c.has_imu            = false;
    c.has_encoder        = false;
    c.wheelchair_control = false;
    return c;
}

// =============================================================================
// Static helpers
// =============================================================================

protocol::ResponsePayload CommandHandler::makeNack(CommandId cmd, NackReason reason) {
    protocol::ResponsePayload r{};
    r.command = cmd;
    r.status  = ResponseStatus::NACK;
    r.nack    = reason;
    return r;
}

protocol::ResponsePayload CommandHandler::makeOk(CommandId cmd) {
    protocol::ResponsePayload r{};
    r.command = cmd;
    r.status  = ResponseStatus::OK;
    r.nack    = NackReason::NONE;
    return r;
}

}  // namespace autochair
