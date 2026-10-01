/**
 * @file safety_manager.h
 * @brief AutoChair ESP32 — Safety Manager
 *
 * Project:  Aivon Innovations — AutoChair
 * Platform: ESP32-WROOM-32D
 * Status:   IN DEVELOPMENT
 *
 * The Safety Manager is the central software component responsible for
 * evaluating all safety-relevant conditions and publishing a SafetyState
 * decision to the StateMachine.  (SAFETY_ARCHITECTURE.md §4)
 *
 * Safety inputs evaluated:
 *   - Emergency-stop status            (§6.1)
 *   - Raspberry Pi heartbeat           (§6.2)
 *   - Sensor health (future)           (§6.3)
 *   - Active faults
 *   - Watchdog status
 *
 * Safety outputs:
 *   - SafetyState (CLEAR / SAFE)
 *   - Fault requests
 *   - Operational authorization decision
 *
 * Safety boundary (mandatory):
 *   SAFE is a software state only. It does NOT prove that the physical
 *   wheelchair has stopped. Physical stopping requires a verified hardware
 *   interface that does not yet exist in V1.
 *
 * Invariants enforced:
 *   Invariant 1: SAFE and operational authorization cannot coexist.
 *   Invariant 3: E-stop release alone never enables the system.
 *   Invariant 4: Communication recovery never resumes a previous operation.
 *   Invariant 6: Pi cannot override an ESP32 safety restriction.
 */

#pragma once

#include "autochair_types.h"
#include "autochair_config.h"
#include <cstdint>

namespace autochair {

// Forward declaration
class StateMachine;

/** Callback invoked whenever the SafetyState changes. */
using SafetyEventCallback = void (*)(SafetyState newState, FaultCode reason);

/**
 * @brief Central Safety Manager.
 *
 * Call update() periodically from the main safety-monitoring task.
 * Individual subsystems call the relevant notify*() methods when their
 * state changes; the Safety Manager re-evaluates and may invoke
 * StateMachine::onSafetyStateChanged().
 */
class SafetyManager {
public:
    explicit SafetyManager(StateMachine& sm);

    // -------------------------------------------------------------------------
    // Initialization
    // -------------------------------------------------------------------------

    void begin();

    // -------------------------------------------------------------------------
    // Periodic evaluation (call at SAFETY_MONITOR_INTERVAL_MS cadence)
    // -------------------------------------------------------------------------

    void update();

    // -------------------------------------------------------------------------
    // Safety inputs  — called by subsystem monitors
    // -------------------------------------------------------------------------

    /**
     * @brief Notify a change in E-stop hardware input.
     *
     * SAFETY_ARCHITECTURE.md §6.1
     * An asserted E-stop immediately sets SafetyState = SAFE (latched).
     * Clearing requires explicit recovery after the input is released.
     */
    void notifyEstopChanged(bool asserted);

    /**
     * @brief Notify that a valid heartbeat was received from the Pi.
     *
     * Resets the heartbeat watchdog timestamp.
     * SAFETY_ARCHITECTURE.md §6.2
     */
    void notifyHeartbeatReceived();

    /**
     * @brief Notify a fault from any subsystem.
     *
     * The Safety Manager evaluates severity and may trigger SAFE.
     */
    void notifyFault(const FaultRecord& fault);

    // -------------------------------------------------------------------------
    // Safety queries
    // -------------------------------------------------------------------------

    SafetyState getSafetyState()    const { return _safetyState; }
    bool        isEstopActive()     const { return _estopActive; }
    bool        isHeartbeatOk()     const { return _heartbeatOk; }
    bool        isOperationAllowed() const;

    // -------------------------------------------------------------------------
    // Recovery  (SAFETY_ARCHITECTURE.md §20)
    // -------------------------------------------------------------------------

    /**
     * @brief Attempt safety recovery.
     *
     * Returns NackReason::NONE if recovery is accepted.
     * Recovery from SAFE_STOP requires: E-stop released + heartbeat OK +
     * no blocking faults + explicit acknowledgement.
     */
    NackReason requestRecovery();

    // -------------------------------------------------------------------------
    // Callbacks
    // -------------------------------------------------------------------------

    void setSafetyEventCallback(SafetyEventCallback cb) { _callback = cb; }

private:
    StateMachine&      _sm;
    SafetyState        _safetyState  = SafetyState::CLEAR;

    // E-stop state (SAFETY_ARCHITECTURE.md §10 — latched until recovery)
    bool               _estopActive  = false;
    bool               _estopLatched = false;  ///< Stays true until explicit recovery.

    // Heartbeat tracking
    bool               _heartbeatOk       = false;
    uint32_t           _lastHeartbeatMs   = 0;

    // Operational authorization
    bool               _operationAllowed  = false;

    SafetyEventCallback _callback = nullptr;

    // -------------------------------------------------------------------------
    // Internal helpers
    // -------------------------------------------------------------------------

    void evaluate();
    void setSafetyState(SafetyState newState, FaultCode reason);
    bool isHeartbeatTimedOut() const;
};

}  // namespace autochair
