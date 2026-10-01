/**
 * @file test_state_machine.cpp
 * @brief AutoChair ESP32 — Unit tests for StateMachine
 *
 * Project:  Aivon Innovations — AutoChair
 * Platform: Native (host machine / no hardware required)
 * Status:   IN DEVELOPMENT
 *
 * Tests the core state-machine invariants from SYSTEM_STATE_MACHINE.md:
 *
 *   Invariant 1: SAFE and operational authorization cannot coexist.
 *   Invariant 2: A blocking fault must prevent READY and ACTIVE.
 *   Invariant 3: Communication reconnection must not resume operation.
 *   Invariant 4: E-stop release alone must not enable the system.
 *
 * PlatformIO native test environment compiles and runs these tests on
 * the development machine without an ESP32 connected.
 *
 * Run with:
 *   pio test -e native
 */

#include <unity.h>

// Include core modules
#include "../src/core/state_machine.h"
#include "../src/safety/safety_manager.h"
#include "../src/diagnostics/logger.h"

using namespace autochair;

// Shared test fixtures
static StateMachine  sm;
static SafetyManager safety(sm);

// =============================================================================
// Test helpers
// =============================================================================

static void advanceToIdle() {
    sm.begin();
    sm.processEvent(SystemEvent::STARTUP_COMPLETE);
    sm.processEvent(SystemEvent::INITIALIZATION_COMPLETE);
    sm.processEvent(SystemEvent::SELF_TEST_PASSED);
}

static void advanceToReady() {
    advanceToIdle();
    sm.requestModeChange(OperatingMode::MANUAL, "test");
    sm.processEvent(SystemEvent::ENABLE_REQUESTED);
}

// =============================================================================
// setUp / tearDown
// =============================================================================

void setUp() {
    // Re-initialize before each test.
    sm.begin();
    safety.begin();
}

void tearDown() {}

// =============================================================================
// Tests
// =============================================================================

/**
 * Test: Boot state is correct on initialization.
 */
void test_initial_state_is_boot() {
    TEST_ASSERT_EQUAL(SystemState::BOOT, sm.getState());
    TEST_ASSERT_EQUAL(SafetyState::CLEAR, sm.getSafety());
    TEST_ASSERT_EQUAL(OperatingMode::NONE, sm.getMode());
    TEST_ASSERT_FALSE(sm.isEnabled());
}

/**
 * Test: Normal startup sequence reaches IDLE.
 */
void test_startup_sequence_reaches_idle() {
    sm.processEvent(SystemEvent::STARTUP_COMPLETE);
    TEST_ASSERT_EQUAL(SystemState::INITIALIZING, sm.getState());

    sm.processEvent(SystemEvent::INITIALIZATION_COMPLETE);
    TEST_ASSERT_EQUAL(SystemState::SELF_TEST, sm.getState());

    sm.processEvent(SystemEvent::SELF_TEST_PASSED);
    TEST_ASSERT_EQUAL(SystemState::IDLE, sm.getState());
}

/**
 * Test: ENABLE without mode set is rejected.
 */
void test_enable_without_mode_rejected() {
    advanceToIdle();
    // Mode is NONE — canEnable() should fail.
    sm.processEvent(SystemEvent::ENABLE_REQUESTED);
    TEST_ASSERT_EQUAL(SystemState::IDLE, sm.getState());
    TEST_ASSERT_FALSE(sm.isEnabled());
}

/**
 * Test: ENABLE with mode set reaches READY.
 */
void test_enable_with_mode_reaches_ready() {
    advanceToIdle();
    NackReason r = sm.requestModeChange(OperatingMode::MANUAL, "test");
    TEST_ASSERT_EQUAL(NackReason::NONE, r);

    sm.processEvent(SystemEvent::ENABLE_REQUESTED);
    TEST_ASSERT_EQUAL(SystemState::READY, sm.getState());
    TEST_ASSERT_TRUE(sm.isEnabled());
}

/**
 * Test: Invariant 1 — safety = SAFE revokes enabled authorization.
 */
void test_invariant1_safety_revokes_authorization() {
    advanceToReady();
    TEST_ASSERT_TRUE(sm.isEnabled());

    // Trigger safety event (as Safety Manager would).
    sm.onSafetyStateChanged(SafetyState::SAFE);

    // Authorization must be revoked.
    TEST_ASSERT_FALSE(sm.isEnabled());
    // State must be SAFE_STOP (was READY).
    TEST_ASSERT_EQUAL(SystemState::SAFE_STOP, sm.getState());
}

/**
 * Test: Invariant 3 — safety clearing alone does NOT enable the system.
 */
void test_invariant3_safety_clear_does_not_reenable() {
    advanceToReady();
    sm.onSafetyStateChanged(SafetyState::SAFE);
    TEST_ASSERT_EQUAL(SystemState::SAFE_STOP, sm.getState());

    // Clear safety (heartbeat restored, etc.)
    sm.processEvent(SystemEvent::SAFETY_CLEARED);

    // System must NOT automatically return to READY or ACTIVE.
    TEST_ASSERT_NOT_EQUAL(SystemState::READY,  sm.getState());
    TEST_ASSERT_NOT_EQUAL(SystemState::ACTIVE, sm.getState());
    TEST_ASSERT_FALSE(sm.isEnabled());
}

/**
 * Test: FAULT state blocks READY transition.
 */
void test_invariant2_fault_blocks_ready() {
    advanceToIdle();
    sm.processEvent(SystemEvent::FAULT_DETECTED);
    TEST_ASSERT_EQUAL(SystemState::FAULT, sm.getState());

    // Attempt ENABLE while in FAULT — must be rejected.
    sm.processEvent(SystemEvent::ENABLE_REQUESTED);
    TEST_ASSERT_NOT_EQUAL(SystemState::READY, sm.getState());
}

/**
 * Test: STOP command from READY returns to IDLE.
 */
void test_stop_from_ready_returns_to_idle() {
    advanceToReady();
    sm.processEvent(SystemEvent::STOP_REQUESTED);
    TEST_ASSERT_EQUAL(SystemState::IDLE, sm.getState());
    TEST_ASSERT_FALSE(sm.isEnabled());
}

/**
 * Test: Disallowed transition is rejected.
 */
void test_disallowed_transition_rejected() {
    sm.processEvent(SystemEvent::STARTUP_COMPLETE);  // → INITIALIZING
    // Attempt to jump directly to READY — not in the allowed table.
    sm.processEvent(SystemEvent::ENABLE_REQUESTED);
    TEST_ASSERT_NOT_EQUAL(SystemState::READY, sm.getState());
}

/**
 * Test: PING command is always permitted.
 */
void test_ping_always_permitted() {
    // BOOT
    TEST_ASSERT_TRUE(sm.isCommandPermitted(CommandId::PING));

    sm.processEvent(SystemEvent::STARTUP_COMPLETE);  // INITIALIZING
    TEST_ASSERT_TRUE(sm.isCommandPermitted(CommandId::PING));

    sm.processEvent(SystemEvent::INITIALIZATION_COMPLETE);  // SELF_TEST
    TEST_ASSERT_TRUE(sm.isCommandPermitted(CommandId::PING));

    sm.processEvent(SystemEvent::SELF_TEST_PASSED);  // IDLE
    TEST_ASSERT_TRUE(sm.isCommandPermitted(CommandId::PING));
}

/**
 * Test: ENABLE command not permitted during INITIALIZING.
 */
void test_enable_not_permitted_during_init() {
    sm.processEvent(SystemEvent::STARTUP_COMPLETE);  // INITIALIZING
    TEST_ASSERT_FALSE(sm.isCommandPermitted(CommandId::ENABLE));
}

// =============================================================================
// Entry point
// =============================================================================

int main(int argc, char** argv) {
    (void)argc; (void)argv;

    // Start logger in native mode (stdout).
    Logger::begin();
    Logger::setLevel(LogLevel::WARNING);  // Reduce noise during tests.

    UNITY_BEGIN();

    RUN_TEST(test_initial_state_is_boot);
    RUN_TEST(test_startup_sequence_reaches_idle);
    RUN_TEST(test_enable_without_mode_rejected);
    RUN_TEST(test_enable_with_mode_reaches_ready);
    RUN_TEST(test_invariant1_safety_revokes_authorization);
    RUN_TEST(test_invariant3_safety_clear_does_not_reenable);
    RUN_TEST(test_invariant2_fault_blocks_ready);
    RUN_TEST(test_stop_from_ready_returns_to_idle);
    RUN_TEST(test_disallowed_transition_rejected);
    RUN_TEST(test_ping_always_permitted);
    RUN_TEST(test_enable_not_permitted_during_init);

    return UNITY_END();
}
