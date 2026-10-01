/**
 * @file command_handler.h
 * @brief AutoChair ESP32 — Command handler
 *
 * Project:  Aivon Innovations — AutoChair
 * Platform: ESP32-WROOM-32D
 * Status:   IN DEVELOPMENT
 *
 * Receives parsed commands from the transport layer, validates them
 * against current state and safety conditions, and dispatches to
 * the appropriate subsystem.  (ESP32_PI_PROTOCOL.md §11)
 *
 * Processing pipeline:
 *   Command → Message Validation → Command Handler
 *           → State Validation → Safety Validation → Subsystem
 *
 * An invalid command must never reach a hardware subsystem.
 */

#pragma once

#include "protocol_types.h"
#include "../core/state_machine.h"
#include "../safety/safety_manager.h"

namespace autochair {

/**
 * @brief Processes a validated command and produces a response.
 *
 * The transport layer calls handleCommand() after protocol parsing.
 * The handler checks state and safety before dispatching.
 */
class CommandHandler {
public:
    CommandHandler(StateMachine& sm, SafetyManager& safety);

    /**
     * @brief Handle an incoming command.
     *
     * @param cmd     Parsed command payload.
     * @param msgId   Protocol message ID (echoed in response).
     * @return        Response payload to send back to the Pi.
     */
    protocol::ResponsePayload handleCommand(const protocol::CommandPayload& cmd,
                                            uint16_t msgId);

    /**
     * @brief Build a GET_STATUS response payload.
     */
    protocol::SystemStatusPayload buildStatusPayload() const;

    /**
     * @brief Build a capabilities response.
     */
    protocol::CapabilitiesPayload buildCapabilitiesPayload() const;

private:
    StateMachine&   _sm;
    SafetyManager&  _safety;

    // Individual command handlers
    protocol::ResponsePayload handlePing();
    protocol::ResponsePayload handleGetStatus();
    protocol::ResponsePayload handleSetMode(OperatingMode mode);
    protocol::ResponsePayload handleEnable();
    protocol::ResponsePayload handleDisable();
    protocol::ResponsePayload handleStop();
    protocol::ResponsePayload handleResetFault();

    // Helper: build a NACK response
    static protocol::ResponsePayload makeNack(CommandId cmd, NackReason reason);
    // Helper: build an OK response
    static protocol::ResponsePayload makeOk(CommandId cmd);
};

}  // namespace autochair
