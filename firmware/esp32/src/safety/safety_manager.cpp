/**
 * @file safety_manager.cpp
 * @brief AutoChair ESP32 — Safety Manager implementation
 *
 * See safety_manager.h for design rationale and safety boundaries.
 */

#include "safety_manager.h"
#include "../core/state_machine.h"
#include "../diagnostics/logger.h"

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

SafetyManager::SafetyManager(StateMachine& sm) : _sm(sm) {}

void SafetyManager::begin() {
    _safetyState     = SafetyState::CLEAR;
    _estopActive     = false;
    _estopLatched    = false;
    _heartbeatOk     = false;
    _lastHeartbeatMs = 0;
    _operationAllowed= false;
    Logger::info("SafetyManager", "Safety manager initialized");
}

// =============================================================================
// Periodic update
// =============================================================================

void SafetyManager::update() {
    evaluate();
}

// =============================================================================
// Safety input notifications
// =============================================================================

void SafetyManager::notifyEstopChanged(bool asserted) {
    if (asserted && !_estopLatched) {
        _estopActive  = true;
        _estopLatched = true;
        Logger::fault("SafetyManager",
                      "E-STOP ASSERTED — safety latched. "
                      "NOTE: software SAFE_STOP only; physical braking unverified.");
        setSafetyState(SafetyState::SAFE, FaultCode::ESTOP_ACTIVE);
    } else if (!asserted) {
        _estopActive = false;
        // _estopLatched remains true until requestRecovery() is called.
        Logger::info("SafetyManager",
                     "E-stop input released; latch persists until recovery");
    }
}

void SafetyManager::notifyHeartbeatReceived() {
    _lastHeartbeatMs = millis();
    if (!_heartbeatOk) {
        _heartbeatOk = true;
        Logger::info("SafetyManager", "Heartbeat restored");
    }
}

void SafetyManager::notifyFault(const FaultRecord& fault) {
    if (fault.severity == FaultSeverity::CRITICAL ||
        fault.severity == FaultSeverity::SAFETY_RESTRICTION) {
        Logger::fault("SafetyManager", "Critical fault received — triggering SAFE");
        setSafetyState(SafetyState::SAFE, fault.code);
    } else if (fault.severity == FaultSeverity::WARNING) {
        Logger::warning("SafetyManager", "Warning fault received");
    }
}

// =============================================================================
// Safety queries
// =============================================================================

bool SafetyManager::isOperationAllowed() const {
    return (_safetyState == SafetyState::CLEAR) && _operationAllowed;
}

// =============================================================================
// Recovery  (SAFETY_ARCHITECTURE.md §20)
// =============================================================================

NackReason SafetyManager::requestRecovery() {
    // All conditions must be satisfied before recovery is accepted.
    if (_estopActive) {
        Logger::warning("SafetyManager", "Recovery rejected: E-stop still active");
        return NackReason::SAFETY_RESTRICTED;
    }
    if (isHeartbeatTimedOut()) {
        Logger::warning("SafetyManager", "Recovery rejected: heartbeat timed out");
        return NackReason::SAFETY_RESTRICTED;
    }
    if (_estopLatched) {
        // E-stop has been released but latch must be explicitly cleared.
        _estopLatched = false;
        Logger::info("SafetyManager", "E-stop latch cleared");
    }

    // Re-evaluate after clearing latched conditions.
    evaluate();

    if (_safetyState == SafetyState::SAFE) {
        Logger::warning("SafetyManager",
                        "Recovery failed: safety still SAFE after evaluation");
        return NackReason::SAFETY_RESTRICTED;
    }

    Logger::info("SafetyManager", "Safety recovery accepted — returning to CLEAR");
    return NackReason::NONE;
}

// =============================================================================
// Internal helpers
// =============================================================================

void SafetyManager::evaluate() {
    // Check heartbeat timeout while operational.
    SystemState st = _sm.getState();
    if ((st == SystemState::READY || st == SystemState::ACTIVE) &&
        isHeartbeatTimedOut()) {
        Logger::fault("SafetyManager",
                      "Heartbeat timeout — triggering SAFE. "
                      "Communication recovery will not automatically resume operation.");
        _heartbeatOk = false;
        setSafetyState(SafetyState::SAFE, FaultCode::HEARTBEAT_TIMEOUT);
        return;
    }

    // If E-stop latch is active, remain SAFE.
    if (_estopLatched) {
        if (_safetyState != SafetyState::SAFE) {
            setSafetyState(SafetyState::SAFE, FaultCode::ESTOP_ACTIVE);
        }
        return;
    }

    // If we reach here with no active conditions, clear safety.
    if (_safetyState == SafetyState::SAFE &&
        !_estopLatched &&
        !isHeartbeatTimedOut()) {
        // Safety can only transition to CLEAR after all conditions clear AND
        // after explicit recovery. This path is reached only after requestRecovery().
        // Do nothing here — requestRecovery() calls evaluate() after clearing.
    }
}

void SafetyManager::setSafetyState(SafetyState newState, FaultCode reason) {
    if (newState == _safetyState) return;

    const SafetyState prev = _safetyState;
    _safetyState = newState;

    if (newState == SafetyState::SAFE) {
        _operationAllowed = false;
    }

    // Notify the state machine.
    _sm.onSafetyStateChanged(newState);

    // Notify external callback.
    if (_callback) {
        _callback(newState, reason);
    }

    (void)prev;
}

bool SafetyManager::isHeartbeatTimedOut() const {
    if (_lastHeartbeatMs == 0) {
        // No heartbeat ever received — not yet timed out (allow startup window).
        return false;
    }
    return (millis() - _lastHeartbeatMs) > autochair::config::HEARTBEAT_TIMEOUT_MS;
}

}  // namespace autochair
