/**
 * @file autochair_types.h
 * @brief AutoChair ESP32 — Core type definitions
 *
 * Project:  Aivon Innovations — AutoChair
 * Platform: ESP32-WROOM-32D
 * Status:   IN DEVELOPMENT
 *
 * This header defines the fundamental enumerations and data structures
 * used across all AutoChair ESP32 firmware modules.
 *
 * Three state dimensions are kept SEPARATE as required by SYSTEM_STATE_MACHINE.md §2:
 *   - SystemState  : firmware lifecycle / operational readiness
 *   - OperatingMode: selected method of operation
 *   - SafetyState  : whether safety conditions permit continued operation
 *
 * These must NOT be collapsed into a single enumeration.
 *
 * Safety boundary: This firmware manages embedded software state only.
 * It does not claim to physically stop the wheelchair. Physical braking
 * and motor control require separately verified hardware interfaces.
 */

#pragma once

#include <cstdint>

namespace autochair {

// =============================================================================
// Firmware version
// =============================================================================

constexpr uint8_t  FIRMWARE_VERSION_MAJOR = 0;
constexpr uint8_t  FIRMWARE_VERSION_MINOR = 1;
constexpr uint8_t  FIRMWARE_VERSION_PATCH = 0;
constexpr uint16_t PROTOCOL_VERSION       = 1;

// =============================================================================
// System State  (SYSTEM_STATE_MACHINE.md §2.1)
// =============================================================================

/**
 * @brief Overall firmware lifecycle and operational readiness.
 *
 * Only the StateMachine class may transition between these states.
 * External modules must request transitions via processEvent().
 */
enum class SystemState : uint8_t {
    BOOT,           ///< ESP32 has powered on; minimum runtime established.
    INITIALIZING,   ///< Modules initialising. No external commands accepted.
    SELF_TEST,      ///< Required subsystems being verified.
    IDLE,           ///< Initialized but not enabled.
    READY,          ///< Enable authorization accepted; mode selected.
    ACTIVE,         ///< An authorized operation is underway.
    SAFE_STOP,      ///< Safety restriction active; operation suspended.
    FAULT           ///< Blocking failure; cannot proceed normally.
};

// =============================================================================
// Operating Mode  (SYSTEM_STATE_MACHINE.md §2.2)
// =============================================================================

/**
 * @brief Selected method of operation.
 *
 * Mode selection does NOT authorize physical wheelchair movement.
 * MANUAL and ASSISTED are architectural placeholders until verified interfaces exist.
 */
enum class OperatingMode : uint8_t {
    NONE,       ///< No operating mode selected.
    MANUAL,     ///< Manual-mode selection — wheelchair interface TBD.
    ASSISTED    ///< Assisted-mode selection — wheelchair interface TBD.
};

// =============================================================================
// Safety State  (SYSTEM_STATE_MACHINE.md §2.3 / SAFETY_ARCHITECTURE.md §5)
// =============================================================================

/**
 * @brief Whether safety conditions currently permit continued operation.
 *
 * CLEAR: all configured safety conditions satisfied.
 *        Does NOT mean physical wheelchair movement is authorized.
 * SAFE : at least one safety condition prevents normal operation.
 *        Operational authorization must be revoked immediately.
 *
 * Safety Invariant 1: SAFE and operational authorization cannot coexist.
 */
enum class SafetyState : uint8_t {
    CLEAR,  ///< All safety conditions satisfied.
    SAFE    ///< Safety restriction active.
};

// =============================================================================
// System Event  (SYSTEM_STATE_MACHINE.md §13)
// =============================================================================

/**
 * @brief Events that drive state-machine transitions.
 *
 * External modules post events; the StateMachine processes them
 * in priority order. Safety events take precedence over operational ones.
 */
enum class SystemEvent : uint8_t {
    STARTUP_COMPLETE,
    INITIALIZATION_COMPLETE,
    INITIALIZATION_FAILED,
    SELF_TEST_PASSED,
    SELF_TEST_FAILED,
    ENABLE_REQUESTED,
    DISABLE_REQUESTED,
    OPERATION_STARTED,
    OPERATION_COMPLETED,
    STOP_REQUESTED,
    SAFETY_TRIGGERED,
    SAFETY_CLEARED,
    FAULT_DETECTED,
    RESET_REQUESTED
};

// =============================================================================
// Fault codes  (SYSTEM_STATE_MACHINE.md §10 / SAFETY_ARCHITECTURE.md §12)
// =============================================================================

enum class FaultCode : uint8_t {
    NONE,
    SENSOR_TIMEOUT,
    SENSOR_INVALID,
    IMU_FAILURE,
    ENCODER_FAILURE,
    ESTOP_ACTIVE,
    HEARTBEAT_TIMEOUT,
    COMMUNICATION_FAILURE,
    WATCHDOG_FAILURE,
    INTERNAL_FAILURE
};

enum class FaultSeverity : uint8_t {
    INFO,
    WARNING,
    SAFETY_RESTRICTION,
    CRITICAL
};

/**
 * @brief A single fault record.
 * Each record contains the fault code, severity, source module, timestamp,
 * and current status (active / acknowledged).
 */
struct FaultRecord {
    FaultCode     code       = FaultCode::NONE;
    FaultSeverity severity   = FaultSeverity::INFO;
    uint32_t      timestamp  = 0;       ///< millis() at time of detection.
    bool          active     = false;
    bool          acknowledged = false;
    char          source[24] = {};      ///< Module name (null-terminated).
};

// =============================================================================
// Sensor status  (SENSOR_INTERFACE.md §5)
// =============================================================================

enum class SensorStatus : uint8_t {
    UNINITIALIZED,
    INITIALIZING,
    OK,
    WARNING,
    TIMEOUT,
    INVALID,
    DISCONNECTED,
    FAULT
};

// =============================================================================
// Command IDs  (ESP32_PI_PROTOCOL.md §12)
// =============================================================================

enum class CommandId : uint8_t {
    PING,
    GET_STATUS,
    GET_SENSOR_DATA,
    GET_ENCODER_DATA,
    GET_IMU_DATA,
    SET_MODE,
    ENABLE,
    DISABLE,
    RESET_FAULT,
    STOP,
    UNKNOWN = 0xFF
};

// =============================================================================
// Command response status  (ESP32_PI_PROTOCOL.md §23)
// =============================================================================

enum class ResponseStatus : uint8_t {
    OK,
    NACK,
    ERROR,
    BUSY
};

// =============================================================================
// NACK reasons  (ESP32_PI_PROTOCOL.md §24)
// =============================================================================

enum class NackReason : uint8_t {
    NONE,
    INVALID_COMMAND,
    INVALID_PAYLOAD,
    INVALID_STATE,
    SAFETY_RESTRICTED,
    FAULT_ACTIVE,
    NOT_READY,
    NOT_SUPPORTED,
    INVALID_MODE,
    INVALID_PARAMETER,
    BUSY
};

// =============================================================================
// Wheelchair interface mode  (WHEELCHAIR_INTERFACE.md §23)
// =============================================================================

/**
 * @brief Active implementation of the wheelchair interface.
 *
 * Default V1: SIMULATION.
 * PHYSICAL must not be enabled until the controller interface is verified.
 */
enum class WheelchairInterfaceMode : uint8_t {
    DISABLED,       ///< Interface inactive.
    SIMULATION,     ///< Software-only stub. Default V1.
    PHYSICAL        ///< Verified hardware interface — NOT V1.
};

enum class WheelchairInterfaceStatus : uint8_t {
    UNAVAILABLE,
    INITIALIZING,
    READY,
    ACTIVE,
    STOPPING,
    DISABLED,
    FAULT,
    SIMULATED
};

// =============================================================================
// Encoder direction  (ENCODER_INTERFACE.md §8)
// =============================================================================

/**
 * @brief Encoder direction convention.
 * Actual FORWARD/REVERSE mapping must be verified with physical hardware.
 */
enum class EncoderDirection : int8_t {
    FORWARD    =  1,
    REVERSE    = -1,
    STATIONARY =  0,
    UNKNOWN    = 127
};

// =============================================================================
// Log levels  (ESP32_CONTROLLER_PRD.md §21)
// =============================================================================

enum class LogLevel : uint8_t {
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    FAULT
};

// =============================================================================
// Diagnostic subsystem health  (ESP32_ARCHITECTURE.md §19)
// =============================================================================

enum class SubsystemHealth : uint8_t {
    INITIALIZING,
    READY,
    WARNING,
    FAULT,
    DISABLED
};

}  // namespace autochair
