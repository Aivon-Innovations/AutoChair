/**
 * @file protocol_types.h
 * @brief AutoChair ESP32 — Pi↔ESP32 protocol data structures
 *
 * Project:  Aivon Innovations — AutoChair
 * Platform: ESP32-WROOM-32D
 * Status:   IN DEVELOPMENT
 *
 * Defines the logical message types for the AutoChair Pi↔ESP32 protocol.
 * (ESP32_PI_PROTOCOL.md)
 *
 * This file defines APPLICATION-LAYER data structures only.
 * Transport framing, serialization, and integrity checking will be
 * implemented in the communication module (Phase 9).
 *
 * Protocol version: 1  (ESP32_PI_PROTOCOL.md §5)
 *
 * V1 protocol deliberately does NOT define:
 *   - SET_MOTOR_PWM
 *   - SET_MOTOR_SPEED
 *   - SET_MOTOR_DIRECTION
 * Physical motor commands require a verified wheelchair interface. (§43)
 */

#pragma once

#include "autochair_types.h"
#include <cstdint>

namespace autochair {
namespace protocol {

constexpr uint16_t PROTOCOL_VERSION_V1 = 1;

// =============================================================================
// Message categories  (ESP32_PI_PROTOCOL.md §6)
// =============================================================================

enum class MessageType : uint8_t {
    COMMAND,
    RESPONSE,
    TELEMETRY,
    EVENT,
    HEARTBEAT,
    HEARTBEAT_ACK,
    DIAGNOSTIC
};

// =============================================================================
// Telemetry types  (ESP32_PI_PROTOCOL.md §25)
// =============================================================================

enum class TelemetryType : uint8_t {
    SYSTEM,
    SAFETY,
    ULTRASONIC,
    IMU,
    ENCODER,
    FAULT,
    HEALTH
};

// =============================================================================
// Event types  (ESP32_PI_PROTOCOL.md §31)
// =============================================================================

enum class EventType : uint8_t {
    STATE_CHANGED,
    SAFETY_TRIGGERED,
    SAFETY_CLEARED,
    FAULT_DETECTED,
    FAULT_CLEARED,
    ESTOP_ASSERTED,
    ESTOP_RELEASED,
    HEARTBEAT_TIMEOUT,
    SENSOR_FAILURE,
    SELF_TEST_COMPLETE
};

// =============================================================================
// Message envelope  (ESP32_PI_PROTOCOL.md §8)
// =============================================================================

/**
 * @brief Logical envelope for every protocol message.
 *
 * Binary framing, CRC, and serialization are added by the transport layer.
 * The final wire format is defined during Phase 9.
 */
struct MessageEnvelope {
    uint16_t    version     = PROTOCOL_VERSION_V1;
    MessageType type        = MessageType::COMMAND;
    uint16_t    message_id  = 0;
    uint32_t    timestamp   = 0;  ///< millis() at sender side.
};

// =============================================================================
// System status payload  (ESP32_PI_PROTOCOL.md §14)
// =============================================================================

struct SystemStatusPayload {
    SystemState   system_state   = SystemState::BOOT;
    OperatingMode operating_mode = OperatingMode::NONE;
    SafetyState   safety_state   = SafetyState::CLEAR;
    bool          enabled        = false;
    uint8_t       active_faults  = 0;   ///< Count of active fault records.
    uint32_t      uptime_ms      = 0;
    uint16_t      protocol_version = PROTOCOL_VERSION_V1;
    char          firmware_version[12] = "0.1.0";
    WheelchairInterfaceMode wheelchair_mode = WheelchairInterfaceMode::SIMULATION;
};

// =============================================================================
// Heartbeat payload  (ESP32_PI_PROTOCOL.md §32)
// =============================================================================

struct HeartbeatPayload {
    uint32_t sequence = 0;
};

struct HeartbeatAckPayload {
    uint32_t    sequence     = 0;
    SystemState system_state = SystemState::IDLE;
    SafetyState safety_state = SafetyState::CLEAR;
};

// =============================================================================
// Command payload  (ESP32_PI_PROTOCOL.md §12)
// =============================================================================

struct CommandPayload {
    CommandId     command = CommandId::UNKNOWN;
    OperatingMode mode    = OperatingMode::NONE;  ///< Used by SET_MODE only.
};

// =============================================================================
// Response payload  (ESP32_PI_PROTOCOL.md §23)
// =============================================================================

struct ResponsePayload {
    CommandId      command = CommandId::UNKNOWN;
    ResponseStatus status  = ResponseStatus::NACK;
    NackReason     nack    = NackReason::NONE;
};

// =============================================================================
// State change event  (ESP32_PI_PROTOCOL.md §31)
// =============================================================================

struct StateChangedEvent {
    SystemState previous_state = SystemState::BOOT;
    SystemState new_state      = SystemState::BOOT;
    SystemEvent trigger        = SystemEvent::STARTUP_COMPLETE;
    char        reason[48]     = {};
};

// =============================================================================
// Safety event  (ESP32_PI_PROTOCOL.md §27)
// =============================================================================

struct SafetyTriggeredEvent {
    FaultCode reason = FaultCode::NONE;
};

// =============================================================================
// Capabilities response  (ESP32_PI_PROTOCOL.md §46)
// =============================================================================

struct CapabilitiesPayload {
    uint16_t protocol_version = PROTOCOL_VERSION_V1;
    char     firmware_version[12] = "0.1.0";
    char     device[24] = "AUTOCHAIR_ESP32";
    bool     has_ultrasonic      = false;  ///< False until Phase 4.
    bool     has_imu             = false;  ///< False until Phase 5.
    bool     has_encoder         = false;  ///< False until Phase 6.
    bool     wheelchair_control  = false;  ///< Always false until interface verified.
};

}  // namespace protocol
}  // namespace autochair
