/**
 * @file state_machine.cpp
 * @brief AutoChair ESP32 — System State Machine implementation
 *
 * Project:  Aivon Innovations — AutoChair
 * Platform: ESP32-WROOM-32D
 * Status:   IN DEVELOPMENT
 *
 * See state_machine.h for design rationale and safety boundaries.
 */

#include "state_machine.h"
#include "../diagnostics/logger.h"

#include <cstring>

// millis() is available in Arduino; for native tests a shim is provided.
#ifdef ENV_NATIVE
#  include <chrono>
static uint32_t millis() {
    using namespace std::chrono;
    return static_cast<uint32_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count()
    );
}
#else
#  include <Arduino.h>
#endif

namespace autochair {

// =============================================================================
// Permitted transitions table  (SYSTEM_STATE_MACHINE.md §6)
// =============================================================================
// Entry: { from, to }
// Only transitions listed here are valid.
// Safety-driven transitions (→ SAFE_STOP, → FAULT) are handled separately.

struct AllowedTransition {
    SystemState from;
    SystemState to;
};

static constexpr AllowedTransition ALLOWED[] = {
    { SystemState::BOOT,        SystemState::INITIALIZING },
    { SystemState::INITIALIZING,SystemState::SELF_TEST    },
    { SystemState::INITIALIZING,SystemState::FAULT        },
    { SystemState::SELF_TEST,   SystemState::IDLE         },
    { SystemState::SELF_TEST,   SystemState::FAULT        },
    { SystemState::IDLE,        SystemState::READY        },
    { SystemState::IDLE,        SystemState::SAFE_STOP    },
    { SystemState::IDLE,        SystemState::FAULT        },
    { SystemState::READY,       SystemState::ACTIVE       },
    { SystemState::READY,       SystemState::IDLE         },
    { SystemState::READY,       SystemState::SAFE_STOP    },
    { SystemState::READY,       SystemState::FAULT        },
    { SystemState::ACTIVE,      SystemState::READY        },
    { SystemState::ACTIVE,      SystemState::IDLE         },
    { SystemState::ACTIVE,      SystemState::SAFE_STOP    },
    { SystemState::ACTIVE,      SystemState::FAULT        },
    { SystemState::SAFE_STOP,   SystemState::IDLE         },
    { SystemState::SAFE_STOP,   SystemState::FAULT        },
    { SystemState::FAULT,       SystemState::IDLE         },
};

static constexpr size_t ALLOWED_COUNT = sizeof(ALLOWED) / sizeof(ALLOWED[0]);

// =============================================================================
// Command permission table  (SYSTEM_STATE_MACHINE.md §12)
// =============================================================================

static bool isCommandAllowedInState(CommandId cmd, SystemState state) {
    // PING and GET_STATUS always available when communication is up.
    if (cmd == CommandId::PING || cmd == CommandId::GET_STATUS) {
        return true;
    }

    // STOP: any state where command processing is available.
    if (cmd == CommandId::STOP) {
        return (state != SystemState::BOOT && state != SystemState::INITIALIZING);
    }

    // Sensor data: IDLE, READY, ACTIVE, SAFE_STOP.
    if (cmd == CommandId::GET_SENSOR_DATA ||
        cmd == CommandId::GET_ENCODER_DATA ||
        cmd == CommandId::GET_IMU_DATA) {
        return (state == SystemState::IDLE  ||
                state == SystemState::READY ||
                state == SystemState::ACTIVE ||
                state == SystemState::SAFE_STOP);
    }

    // SET_MODE: IDLE or READY only.
    if (cmd == CommandId::SET_MODE) {
        return (state == SystemState::IDLE || state == SystemState::READY);
    }

    // ENABLE: IDLE only.
    if (cmd == CommandId::ENABLE) {
        return (state == SystemState::IDLE);
    }

    // DISABLE: IDLE, READY, ACTIVE.
    if (cmd == CommandId::DISABLE) {
        return (state == SystemState::IDLE  ||
                state == SystemState::READY ||
                state == SystemState::ACTIVE);
    }

    // RESET_FAULT: FAULT or SAFE_STOP.
    if (cmd == CommandId::RESET_FAULT) {
        return (state == SystemState::FAULT ||
                state == SystemState::SAFE_STOP);
    }

    return false;
}

// =============================================================================
// StateMachine implementation
// =============================================================================

StateMachine::StateMachine() = default;

void StateMachine::begin() {
    _state   = SystemState::BOOT;
    _mode    = OperatingMode::NONE;
    _safety  = SafetyState::CLEAR;
    _enabled = false;
    _startMs = millis();
    Logger::info("StateMachine", "Initialized in BOOT state");
}

// -----------------------------------------------------------------------------
// processEvent
// -----------------------------------------------------------------------------

void StateMachine::processEvent(SystemEvent event, const char* reason) {
    const SystemState prev = _state;

    switch (event) {

    // --- Startup sequence ---
    case SystemEvent::STARTUP_COMPLETE:
        transitionTo(SystemState::INITIALIZING, event, reason ?: "Startup complete");
        break;

    case SystemEvent::INITIALIZATION_COMPLETE:
        transitionTo(SystemState::SELF_TEST, event, reason ?: "Init complete");
        break;

    case SystemEvent::INITIALIZATION_FAILED:
        revokeEnableAuthorization();
        transitionTo(SystemState::FAULT, event, reason ?: "Init failed");
        break;

    case SystemEvent::SELF_TEST_PASSED:
        transitionTo(SystemState::IDLE, event, reason ?: "Self-test passed");
        break;

    case SystemEvent::SELF_TEST_FAILED:
        revokeEnableAuthorization();
        transitionTo(SystemState::FAULT, event, reason ?: "Self-test failed");
        break;

    // --- Operational ---
    case SystemEvent::ENABLE_REQUESTED:
        if (!canEnable()) {
            Logger::warning("StateMachine", "ENABLE rejected: guard failed");
            break;
        }
        _enabled = true;
        transitionTo(SystemState::READY, event, reason ?: "Enable accepted");
        break;

    case SystemEvent::DISABLE_REQUESTED:
        revokeEnableAuthorization();
        if (_safety == SafetyState::CLEAR) {
            transitionTo(SystemState::IDLE, event, reason ?: "Disable");
        }
        // If safety is SAFE, remain in SAFE_STOP rather than returning to IDLE.
        break;

    case SystemEvent::OPERATION_STARTED:
        if (!canStartOperation()) {
            Logger::warning("StateMachine", "Operation start rejected");
            break;
        }
        transitionTo(SystemState::ACTIVE, event, reason ?: "Operation started");
        break;

    case SystemEvent::OPERATION_COMPLETED:
        if (_state == SystemState::ACTIVE) {
            transitionTo(SystemState::READY, event, reason ?: "Operation complete");
        }
        break;

    // --- Stop (high-priority) ---
    case SystemEvent::STOP_REQUESTED:
        revokeEnableAuthorization();
        if (_safety == SafetyState::SAFE) {
            transitionTo(SystemState::SAFE_STOP, event, reason ?: "Stop — safety active");
        } else {
            transitionTo(SystemState::IDLE, event, reason ?: "Stop");
        }
        break;

    // --- Safety ---
    case SystemEvent::SAFETY_TRIGGERED:
        // onSafetyStateChanged() is the primary entry point for safety events;
        // this event path handles cases where the safety trigger comes via the
        // main event queue rather than the direct safety callback.
        _safety = SafetyState::SAFE;
        revokeEnableAuthorization();
        if (_state == SystemState::READY  ||
            _state == SystemState::ACTIVE ||
            _state == SystemState::IDLE) {
            transitionTo(SystemState::SAFE_STOP, event, reason ?: "Safety triggered");
        }
        break;

    case SystemEvent::SAFETY_CLEARED:
        // Safety cleared does NOT automatically restore operational authorization.
        // (Invariant 4: communication reconnection never resumes a previous operation)
        _safety = SafetyState::CLEAR;
        Logger::info("StateMachine", "Safety cleared; manual recovery required");
        break;

    // --- Fault ---
    case SystemEvent::FAULT_DETECTED:
        revokeEnableAuthorization();
        transitionTo(SystemState::FAULT, event, reason ?: "Fault detected");
        break;

    // --- Recovery ---
    case SystemEvent::RESET_REQUESTED:
        if (_state != SystemState::FAULT && _state != SystemState::SAFE_STOP) {
            Logger::warning("StateMachine", "RESET_FAULT ignored in current state");
            break;
        }
        if (_safety == SafetyState::SAFE) {
            Logger::warning("StateMachine", "RESET_FAULT rejected: safety still SAFE");
            break;
        }
        // Recovery requires the caller to have confirmed the underlying condition
        // has cleared and passed required checks. Here we simply transition to IDLE.
        // The command handler is responsible for those checks before posting this event.
        transitionTo(SystemState::IDLE, event, reason ?: "Recovery complete");
        break;

    default:
        Logger::warning("StateMachine", "Unknown event received");
        break;
    }

    (void)prev;  // prev available for debugging if needed
}

// -----------------------------------------------------------------------------
// onSafetyStateChanged  (called directly by SafetyManager)
// -----------------------------------------------------------------------------

void StateMachine::onSafetyStateChanged(SafetyState newSafety) {
    if (newSafety == _safety) return;  // No change.

    _safety = newSafety;

    if (newSafety == SafetyState::SAFE) {
        revokeEnableAuthorization();
        if (_state == SystemState::READY  ||
            _state == SystemState::ACTIVE ||
            _state == SystemState::IDLE) {
            transitionTo(SystemState::SAFE_STOP, SystemEvent::SAFETY_TRIGGERED,
                         "Safety manager: SAFE");
        }
    }
    // CLEAR is recorded; operational authorization requires a new ENABLE.
}

// -----------------------------------------------------------------------------
// requestModeChange
// -----------------------------------------------------------------------------

NackReason StateMachine::requestModeChange(OperatingMode mode, const char* reason) {
    if (_state != SystemState::IDLE && _state != SystemState::READY) {
        return NackReason::INVALID_STATE;
    }
    if (_state == SystemState::ACTIVE) {
        return NackReason::INVALID_STATE;  // Must stop before changing mode.
    }
    if (_safety == SafetyState::SAFE) {
        return NackReason::SAFETY_RESTRICTED;
    }
    if (mode == OperatingMode::NONE) {
        _mode = OperatingMode::NONE;
        Logger::info("StateMachine", "Mode set to NONE");
        return NackReason::NONE;
    }
    // MANUAL and ASSISTED are accepted; movement authorization remains pending
    // verification of the wheelchair interface.
    _mode = mode;
    Logger::info("StateMachine", reason ?: "Mode changed");
    return NackReason::NONE;
}

// -----------------------------------------------------------------------------
// canEnable / canStartOperation
// -----------------------------------------------------------------------------

bool StateMachine::canEnable() const {
    if (_state != SystemState::IDLE)          return false;
    if (_safety == SafetyState::SAFE)         return false;
    if (_mode  == OperatingMode::NONE)        return false;
    return true;
}

bool StateMachine::canStartOperation() const {
    if (_state  != SystemState::READY)        return false;
    if (_safety == SafetyState::SAFE)         return false;
    if (!_enabled)                            return false;
    return true;
}

// -----------------------------------------------------------------------------
// isCommandPermitted
// -----------------------------------------------------------------------------

bool StateMachine::isCommandPermitted(CommandId cmd) const {
    return isCommandAllowedInState(cmd, _state);
}

// -----------------------------------------------------------------------------
// uptimeMs
// -----------------------------------------------------------------------------

uint32_t StateMachine::uptimeMs() const {
    return millis() - _startMs;
}

// -----------------------------------------------------------------------------
// Internal helpers
// -----------------------------------------------------------------------------

void StateMachine::revokeEnableAuthorization() {
    if (_enabled) {
        _enabled = false;
        Logger::info("StateMachine", "Enable authorization revoked");
    }
}

bool StateMachine::isTransitionAllowed(SystemState from, SystemState to) const {
    for (size_t i = 0; i < ALLOWED_COUNT; ++i) {
        if (ALLOWED[i].from == from && ALLOWED[i].to == to) return true;
    }
    return false;
}

bool StateMachine::transitionTo(SystemState target, SystemEvent trigger,
                                 const char* reason) {
    if (_state == target) return true;  // Already there.

    if (!isTransitionAllowed(_state, target)) {
        Logger::warning("StateMachine", "Transition rejected (not in allowed table)");
        return false;
    }

    const SystemState prev = _state;
    _state = target;
    notifyStateChange(prev, target, trigger, reason);
    return true;
}

void StateMachine::notifyStateChange(SystemState prev, SystemState curr,
                                      SystemEvent trigger, const char* reason) {
    Logger::info("StateMachine", "State transition");
    if (_callback) {
        StateChangeEvent ev{};
        ev.previous     = prev;
        ev.current      = curr;
        ev.trigger      = trigger;
        ev.timestamp_ms = millis();
        if (reason) {
            strncpy(ev.reason, reason, sizeof(ev.reason) - 1);
        }
        _callback(ev);
    }
}

}  // namespace autochair
