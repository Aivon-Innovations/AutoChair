/**
 * @file state_machine.h
 * @brief AutoChair ESP32 — Centralized System State Machine
 *
 * Project:  Aivon Innovations — AutoChair
 * Platform: ESP32-WROOM-32D
 * Status:   IN DEVELOPMENT
 *
 * Implements the three-dimensional state model defined in SYSTEM_STATE_MACHINE.md:
 *   - SystemState  : firmware lifecycle
 *   - OperatingMode: selected mode
 *   - SafetyState  : safety layer decision
 *
 * Key invariants enforced here:
 *   - Only processEvent() changes SystemState (§13, Invariant 6).
 *   - SafetyState = SAFE revokes operational authorization (Invariant 1).
 *   - Reboot never restores previous enable authorization (Invariant 3).
 *   - FAULT never auto-transitions to READY/ACTIVE (§4.8).
 *
 * Safety boundary: state transitions are software-only.
 * Physical wheelchair stopping is NOT implied by SAFE_STOP.
 */

#pragma once

#include "autochair_types.h"
#include <cstdint>

namespace autochair {

// Forward declaration
struct StateChangeEvent {
    SystemState  previous;
    SystemState  current;
    SystemEvent  trigger;
    uint32_t     timestamp_ms;
    char         reason[48];
};

/**
 * @brief Callback invoked whenever the system state changes.
 * Implement in the application to react to state transitions (e.g. logging).
 */
using StateChangeCallback = void (*)(const StateChangeEvent&);

/**
 * @brief Central state machine for the AutoChair ESP32 firmware.
 *
 * Design rules:
 *  - No public setter for SystemState.
 *  - All transitions go through processEvent().
 *  - Safety events are handled before operational requests.
 *  - The machine does not directly communicate with hardware.
 */
class StateMachine {
public:
    StateMachine();

    // -------------------------------------------------------------------------
    // Initialization
    // -------------------------------------------------------------------------

    /** Must be called once at firmware startup (in BOOT state). */
    void begin();

    // -------------------------------------------------------------------------
    // Event processing  (SYSTEM_STATE_MACHINE.md §13)
    // -------------------------------------------------------------------------

    /**
     * @brief Process an event, potentially causing a state transition.
     *
     * Safety and fault events are evaluated before ordinary operational events.
     * An invalid or rejected transition is silently discarded (no exception).
     *
     * @param event  The event to process.
     * @param reason Optional human-readable reason (for logging/telemetry).
     */
    void processEvent(SystemEvent event, const char* reason = nullptr);

    // -------------------------------------------------------------------------
    // Queries
    // -------------------------------------------------------------------------

    SystemState   getState()  const { return _state; }
    OperatingMode getMode()   const { return _mode;  }
    SafetyState   getSafety() const { return _safety; }

    bool isEnabled()           const { return _enabled; }
    bool canEnable()           const;
    bool canStartOperation()   const;

    /** True if the current state + safety permit accepting this command. */
    bool isCommandPermitted(CommandId cmd) const;

    uint32_t uptimeMs() const;

    // -------------------------------------------------------------------------
    // Safety integration  (called by SafetyManager)
    // -------------------------------------------------------------------------

    /**
     * @brief Called by SafetyManager whenever the safety state changes.
     *
     * If safety transitions to SAFE while operational, this method triggers
     * a SAFE_STOP transition automatically.
     */
    void onSafetyStateChanged(SafetyState newSafety);

    // -------------------------------------------------------------------------
    // Mode management  (SYSTEM_STATE_MACHINE.md §5)
    // -------------------------------------------------------------------------

    /**
     * @brief Request an operating-mode change.
     *
     * Accepted only in IDLE or READY, with no active operation and no blocking
     * safety condition.
     *
     * @param mode   Requested mode.
     * @param reason Human-readable reason.
     * @return NackReason::NONE if accepted; otherwise a NACK reason.
     */
    NackReason requestModeChange(OperatingMode mode, const char* reason = nullptr);

    // -------------------------------------------------------------------------
    // Callbacks
    // -------------------------------------------------------------------------

    void setStateChangeCallback(StateChangeCallback cb) { _callback = cb; }

private:
    // State
    SystemState   _state  = SystemState::BOOT;
    OperatingMode _mode   = OperatingMode::NONE;
    SafetyState   _safety = SafetyState::CLEAR;
    bool          _enabled = false;

    uint32_t _startMs = 0;

    StateChangeCallback _callback = nullptr;

    // -------------------------------------------------------------------------
    // Internal transition helpers
    // -------------------------------------------------------------------------

    /** Attempt to move to the target state.  Returns true on success. */
    bool transitionTo(SystemState target, SystemEvent trigger, const char* reason);

    /** Revoke operational authorization and clear enabled flag. */
    void revokeEnableAuthorization();

    /** Publish a StateChangeEvent to the registered callback. */
    void notifyStateChange(SystemState prev, SystemState curr,
                           SystemEvent trigger, const char* reason);

    /** Guard: is this transition defined and permitted? */
    bool isTransitionAllowed(SystemState from, SystemState to) const;
};

}  // namespace autochair
