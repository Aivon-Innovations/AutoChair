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
#include "../src/communication/command_handler.h"
#include "../src/hardware/wheelchair_interface/wheelchair_interface.h"
#include "../src/sensors/ultrasonic/ultrasonic_io.h"
#include "../src/sensors/ultrasonic/hcsr04_driver.h"
#include "../src/sensors/ultrasonic/ultrasonic_manager.h"
#include "../src/sensors/imu/imu_bus.h"
#include "../src/sensors/imu/mpu_driver.h"
#include "../src/sensors/encoder/encoder_hal.h"
#include "../src/sensors/encoder/encoder_driver.h"
#include "../src/sensors/encoder/encoder_manager.h"

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

/**
 * Test: E-stop assertion forces SAFE and remains latched until recovery.
 */
void test_estop_forces_safe_and_latches() {
    advanceToReady();
    TEST_ASSERT_TRUE(sm.isEnabled());

    // 1. Assert E-stop
    safety.notifyEstopChanged(true);
    TEST_ASSERT_EQUAL(SafetyState::SAFE, safety.getSafetyState());
    TEST_ASSERT_EQUAL(SystemState::SAFE_STOP, sm.getState());
    TEST_ASSERT_FALSE(sm.isEnabled());
    TEST_ASSERT_TRUE(safety.isEstopActive());

    // 2. Release physical button — latch MUST persist
    safety.notifyEstopChanged(false);
    TEST_ASSERT_FALSE(safety.isEstopActive());  // Input unasserted
    TEST_ASSERT_EQUAL(SafetyState::SAFE, safety.getSafetyState());  // Still SAFE (latched!)
    TEST_ASSERT_FALSE(sm.isEnabled());

    // 3. Recovery while released should succeed
    NackReason rec = safety.requestRecovery();
    TEST_ASSERT_EQUAL(NackReason::NONE, rec);
    TEST_ASSERT_EQUAL(SafetyState::CLEAR, safety.getSafetyState());

    // 4. System must NOT auto-re-enable (must transition to IDLE via RESET_REQUESTED)
    TEST_ASSERT_FALSE(sm.isEnabled());
    sm.processEvent(SystemEvent::RESET_REQUESTED);
    TEST_ASSERT_EQUAL(SystemState::IDLE, sm.getState());
}

/**
 * Test: E-stop recovery is rejected while physically active.
 */
void test_estop_recovery_rejected_while_physically_active() {
    advanceToReady();
    safety.notifyEstopChanged(true);
    TEST_ASSERT_TRUE(safety.isEstopActive());

    // Attempt recovery while button is still pressed
    NackReason rec = safety.requestRecovery();
    TEST_ASSERT_EQUAL(NackReason::SAFETY_RESTRICTED, rec);
    TEST_ASSERT_EQUAL(SafetyState::SAFE, safety.getSafetyState());
}

/**
 * Test: Command handler rejects ENABLE when safety is SAFE.
 */
void test_command_handler_rejects_enable_when_safe() {
    CommandHandler ch(sm, safety);
    advanceToIdle();
    sm.requestModeChange(OperatingMode::MANUAL, "test");

    // Force SAFE via E-stop -> state transitions to SAFE_STOP
    safety.notifyEstopChanged(true);
    TEST_ASSERT_EQUAL(SystemState::SAFE_STOP, sm.getState());
    TEST_ASSERT_FALSE(sm.canEnable());

    protocol::CommandPayload cmd{};
    cmd.command = CommandId::ENABLE;
    protocol::ResponsePayload resp = ch.handleCommand(cmd, 1);

    // In SAFE_STOP, ENABLE is not permitted (INVALID_STATE)
    TEST_ASSERT_EQUAL(ResponseStatus::NACK, resp.status);
    TEST_ASSERT_EQUAL(NackReason::INVALID_STATE, resp.nack);
    TEST_ASSERT_FALSE(sm.isEnabled());
}

/**
 * Test: Wheelchair interface simulation is strictly software-only.
 */
void test_wheelchair_interface_simulation_only() {
    WheelchairInterfaceSimulation wc;
    TEST_ASSERT_EQUAL(WheelchairInterfaceMode::SIMULATION, wc.getMode());
    TEST_ASSERT_EQUAL(WheelchairInterfaceStatus::UNAVAILABLE, wc.getStatus());

    TEST_ASSERT_TRUE(wc.initialize());
    TEST_ASSERT_EQUAL(WheelchairInterfaceStatus::READY, wc.getStatus());

    TEST_ASSERT_TRUE(wc.enable());
    TEST_ASSERT_EQUAL(WheelchairInterfaceStatus::ACTIVE, wc.getStatus());

    TEST_ASSERT_TRUE(wc.stop());
    TEST_ASSERT_EQUAL(WheelchairInterfaceStatus::STOPPING, wc.getStatus());

    TEST_ASSERT_TRUE(wc.disable());
    TEST_ASSERT_EQUAL(WheelchairInterfaceStatus::DISABLED, wc.getStatus());
}

// =============================================================================
// HC-SR04 Ultrasonic Driver Unit Tests (Simulation / Host-only)
// =============================================================================

/**
 * Test 1: Driver begins in UNINITIALIZED state when pins are placeholder (0).
 */
void test_hcsr04_uninitialized_and_placeholder_pins() {
    HCSR04Config cfg{};
    cfg.sensor_id = 0;
    cfg.trig_pin  = 0;  // Placeholder pin
    cfg.echo_pin  = 0;  // Placeholder pin

    HCSR04Driver driver(cfg);
    TEST_ASSERT_EQUAL(SensorStatus::UNINITIALIZED, driver.getStatus());
    TEST_ASSERT_FALSE(driver.isHealthy());
    TEST_ASSERT_EQUAL_FLOAT(-1.0f, driver.getReading().distance_mm);

    // begin() with placeholder pin 0 returns false and keeps UNINITIALIZED
    TEST_ASSERT_FALSE(driver.begin());
    TEST_ASSERT_EQUAL(SensorStatus::UNINITIALIZED, driver.getStatus());

    // update() on uninitialized sensor must remain safe (-1.0 mm, UNINITIALIZED)
    driver.update();
    TEST_ASSERT_EQUAL(SensorStatus::UNINITIALIZED, driver.getStatus());
    TEST_ASSERT_EQUAL_FLOAT(-1.0f, driver.getReading().distance_mm);
}

/**
 * Test 2: Valid distance measurement calculation with mock IO.
 */
void test_hcsr04_valid_distance_measurement() {
    MockUltrasonicIO mockIO(MockUltrasonicIO::MockMode::FIXED_PULSE);
    // 1166 us * 0.1715 mm/us = 199.969 mm (~200 mm)
    mockIO.setPulseUs(1166);

    HCSR04Config cfg{};
    cfg.sensor_id = 1;
    cfg.trig_pin  = 4;
    cfg.echo_pin  = 5;

    HCSR04Driver driver(cfg, &mockIO);
    TEST_ASSERT_TRUE(driver.begin());
    TEST_ASSERT_EQUAL(SensorStatus::INITIALIZING, driver.getStatus());

    driver.update();
    TEST_ASSERT_EQUAL(SensorStatus::OK, driver.getStatus());
    TEST_ASSERT_TRUE(driver.isHealthy());

    const auto& reading = driver.getReading();
    TEST_ASSERT_EQUAL(1, reading.sensor_id);
    TEST_ASSERT_EQUAL(SensorStatus::OK, reading.status);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 200.0f, reading.distance_mm);

    const auto& health = driver.getHealth();
    TEST_ASSERT_EQUAL(0, health.consecutive_failures);
    TEST_ASSERT_EQUAL(1, health.successful_measurements);
    TEST_ASSERT_EQUAL(1, health.total_measurements);
}

/**
 * Test 3: Minimum range boundary validation (20 mm).
 */
void test_hcsr04_minimum_range_boundary() {
    MockUltrasonicIO mockIO;
    HCSR04Config cfg{};
    cfg.sensor_id       = 0;
    cfg.trig_pin        = 4;
    cfg.echo_pin        = 5;
    cfg.min_distance_mm = 20.0f;
    cfg.max_distance_mm = 4000.0f;

    HCSR04Driver driver(cfg, &mockIO);
    TEST_ASSERT_TRUE(driver.begin());

    // 1. Valid reading right at min boundary: 20 mm -> pulse = 20 / 0.1715 = 117 us (20.06 mm)
    mockIO.setPulseUs(117);
    driver.update();
    TEST_ASSERT_EQUAL(SensorStatus::OK, driver.getStatus());
    TEST_ASSERT_TRUE(driver.isHealthy());
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 20.0f, driver.getReading().distance_mm);

    // 2. Below min boundary: 15 mm -> pulse = 15 / 0.1715 = 87 us (14.92 mm)
    mockIO.setPulseUs(87);
    driver.update();
    TEST_ASSERT_EQUAL(SensorStatus::INVALID, driver.getStatus());
    TEST_ASSERT_FALSE(driver.isHealthy());
    TEST_ASSERT_EQUAL_FLOAT(-1.0f, driver.getReading().distance_mm);
}

/**
 * Test 4: Maximum range boundary validation (4000 mm).
 */
void test_hcsr04_maximum_range_boundary() {
    MockUltrasonicIO mockIO;
    HCSR04Config cfg{};
    cfg.sensor_id       = 0;
    cfg.trig_pin        = 4;
    cfg.echo_pin        = 5;
    cfg.min_distance_mm = 20.0f;
    cfg.max_distance_mm = 4000.0f;

    HCSR04Driver driver(cfg, &mockIO);
    TEST_ASSERT_TRUE(driver.begin());

    // 1. Valid reading right at max boundary: 3999 mm -> pulse = 3999 / 0.1715 = 23317 us
    mockIO.setPulseUs(23317);
    driver.update();
    TEST_ASSERT_EQUAL(SensorStatus::OK, driver.getStatus());
    TEST_ASSERT_TRUE(driver.isHealthy());
    TEST_ASSERT_FLOAT_WITHIN(5.0f, 3999.0f, driver.getReading().distance_mm);

    // 2. Above max boundary: 4500 mm -> pulse = 4500 / 0.1715 = 26239 us
    mockIO.setPulseUs(26239);
    driver.update();
    TEST_ASSERT_EQUAL(SensorStatus::INVALID, driver.getStatus());
    TEST_ASSERT_FALSE(driver.isHealthy());
    TEST_ASSERT_EQUAL_FLOAT(-1.0f, driver.getReading().distance_mm);
}

/**
 * Test 5: Timeout / No echo pulse handling.
 */
void test_hcsr04_timeout_no_echo() {
    MockUltrasonicIO mockIO(MockUltrasonicIO::MockMode::TIMEOUT);
    HCSR04Config cfg{};
    cfg.sensor_id = 0;
    cfg.trig_pin  = 4;
    cfg.echo_pin  = 5;

    HCSR04Driver driver(cfg, &mockIO);
    TEST_ASSERT_TRUE(driver.begin());

    driver.update();
    TEST_ASSERT_EQUAL(SensorStatus::TIMEOUT, driver.getStatus());
    TEST_ASSERT_FALSE(driver.isHealthy());
    TEST_ASSERT_EQUAL_FLOAT(-1.0f, driver.getReading().distance_mm);
    TEST_ASSERT_EQUAL(1, driver.getHealth().consecutive_failures);
}

/**
 * Test 6: Pulse exceeding maximum timeout limit (> 30000 us).
 */
void test_hcsr04_pulse_exceeds_timeout() {
    MockUltrasonicIO mockIO;
    HCSR04Config cfg{};
    cfg.sensor_id  = 0;
    cfg.trig_pin   = 4;
    cfg.echo_pin   = 5;
    cfg.timeout_us = 30000;

    HCSR04Driver driver(cfg, &mockIO);
    TEST_ASSERT_TRUE(driver.begin());

    // Set pulse longer than timeout limit
    mockIO.setPulseUs(35000);
    driver.update();
    TEST_ASSERT_EQUAL(SensorStatus::TIMEOUT, driver.getStatus());
    TEST_ASSERT_FALSE(driver.isHealthy());
    TEST_ASSERT_EQUAL_FLOAT(-1.0f, driver.getReading().distance_mm);
}

/**
 * Test 7: Repeated measurements with sequence of pulse values.
 */
void test_hcsr04_repeated_measurements() {
    MockUltrasonicIO mockIO;
    const uint32_t pulses[] = {
        583,   // ~100 mm
        1749,  // ~300 mm
        2915   // ~500 mm
    };
    mockIO.setSequence(pulses, 3);

    HCSR04Config cfg{};
    cfg.sensor_id = 2;
    cfg.trig_pin  = 4;
    cfg.echo_pin  = 5;

    HCSR04Driver driver(cfg, &mockIO);
    TEST_ASSERT_TRUE(driver.begin());

    // Measurement 1: 100 mm
    driver.update();
    TEST_ASSERT_EQUAL(SensorStatus::OK, driver.getStatus());
    TEST_ASSERT_FLOAT_WITHIN(2.0f, 100.0f, driver.getReading().distance_mm);

    // Measurement 2: 300 mm
    driver.update();
    TEST_ASSERT_EQUAL(SensorStatus::OK, driver.getStatus());
    TEST_ASSERT_FLOAT_WITHIN(2.0f, 300.0f, driver.getReading().distance_mm);

    // Measurement 3: 500 mm
    driver.update();
    TEST_ASSERT_EQUAL(SensorStatus::OK, driver.getStatus());
    TEST_ASSERT_FLOAT_WITHIN(2.0f, 500.0f, driver.getReading().distance_mm);

    TEST_ASSERT_EQUAL(3, driver.getHealth().total_measurements);
    TEST_ASSERT_EQUAL(3, driver.getHealth().successful_measurements);
    TEST_ASSERT_EQUAL(0, driver.getHealth().consecutive_failures);
}

/**
 * Test 8: Consecutive failures escalate to FAULT, and valid reading recovers.
 */
void test_hcsr04_fault_escalation_and_recovery() {
    MockUltrasonicIO mockIO(MockUltrasonicIO::MockMode::TIMEOUT);
    HCSR04Config cfg{};
    cfg.sensor_id                = 0;
    cfg.trig_pin                 = 4;
    cfg.echo_pin                 = 5;
    cfg.max_consecutive_failures = 3;  // Escalate after 3 failures

    HCSR04Driver driver(cfg, &mockIO);
    TEST_ASSERT_TRUE(driver.begin());

    // Failure 1 -> TIMEOUT
    driver.update();
    TEST_ASSERT_EQUAL(SensorStatus::TIMEOUT, driver.getStatus());
    TEST_ASSERT_EQUAL(1, driver.getHealth().consecutive_failures);

    // Failure 2 -> TIMEOUT
    driver.update();
    TEST_ASSERT_EQUAL(SensorStatus::TIMEOUT, driver.getStatus());
    TEST_ASSERT_EQUAL(2, driver.getHealth().consecutive_failures);

    // Failure 3 -> Escalate to FAULT
    driver.update();
    TEST_ASSERT_EQUAL(SensorStatus::FAULT, driver.getStatus());
    TEST_ASSERT_EQUAL(3, driver.getHealth().consecutive_failures);
    TEST_ASSERT_FALSE(driver.isHealthy());

    // Recovery: mock provides valid pulse (1166 us -> ~200 mm)
    mockIO.setMode(MockUltrasonicIO::MockMode::FIXED_PULSE);
    mockIO.setPulseUs(1166);
    driver.update();

    TEST_ASSERT_EQUAL(SensorStatus::OK, driver.getStatus());
    TEST_ASSERT_TRUE(driver.isHealthy());
    TEST_ASSERT_EQUAL(0, driver.getHealth().consecutive_failures);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 200.0f, driver.getReading().distance_mm);
}

/**
 * Test 9: Safety Invariant — Invalid reading is NEVER represented as 0.0 mm.
 */
void test_hcsr04_never_reports_zero_for_invalid() {
    MockUltrasonicIO mockIO;
    HCSR04Config cfg{};
    cfg.sensor_id = 0;
    cfg.trig_pin  = 4;
    cfg.echo_pin  = 5;

    HCSR04Driver driver(cfg, &mockIO);
    TEST_ASSERT_TRUE(driver.begin());

    // 1. Timeout
    mockIO.setMode(MockUltrasonicIO::MockMode::TIMEOUT);
    driver.update();
    TEST_ASSERT_NOT_EQUAL(0.0f, driver.getReading().distance_mm);
    TEST_ASSERT_EQUAL_FLOAT(-1.0f, driver.getReading().distance_mm);

    // 2. Sub-minimum (< 20 mm)
    mockIO.setMode(MockUltrasonicIO::MockMode::FIXED_PULSE);
    mockIO.setPulseUs(50);  // ~8.5 mm
    driver.update();
    TEST_ASSERT_NOT_EQUAL(0.0f, driver.getReading().distance_mm);
    TEST_ASSERT_EQUAL_FLOAT(-1.0f, driver.getReading().distance_mm);

    // 3. Super-maximum (> 4000 mm)
    mockIO.setPulseUs(28000);  // ~4800 mm
    driver.update();
    TEST_ASSERT_NOT_EQUAL(0.0f, driver.getReading().distance_mm);
    TEST_ASSERT_EQUAL_FLOAT(-1.0f, driver.getReading().distance_mm);
}

/**
 * Test 10: UltrasonicManager aggregates sensors and finds minimum distance.
 */
void test_ultrasonic_manager_aggregation() {
    UltrasonicManager mgr;
    TEST_ASSERT_EQUAL(config::ULTRASONIC_COUNT, UltrasonicManager::getSensorCount());

    // Uninitialized sensors: getMinValidDistanceMm() returns -1.0f
    TEST_ASSERT_EQUAL_FLOAT(-1.0f, mgr.getMinValidDistanceMm());

    // Attach mock IO to drivers in manager
    MockUltrasonicIO mock0(MockUltrasonicIO::MockMode::FIXED_PULSE, 2915);  // ~500 mm
    MockUltrasonicIO mock1(MockUltrasonicIO::MockMode::FIXED_PULSE, 1166);  // ~200 mm (minimum)
    MockUltrasonicIO mock2(MockUltrasonicIO::MockMode::TIMEOUT);           // Timeout (-1.0 mm)

    mgr.getDriver(0).setIO(&mock0);
    mgr.getDriver(1).setIO(&mock1);
    mgr.getDriver(2).setIO(&mock2);

    // Initialize mock-enabled drivers
    mock0.initPins(4, 5);
    mgr.getDriver(0).begin();
    mgr.getDriver(1).begin();
    mgr.getDriver(2).begin();

    // Round-robin: call update 3 times to update drivers 0, 1, and 2
    mgr.update(); // polls sensor 0
    mgr.update(); // polls sensor 1
    mgr.update(); // polls sensor 2

    TEST_ASSERT_EQUAL(SensorStatus::OK, mgr.getReading(0).status);
    TEST_ASSERT_FLOAT_WITHIN(2.0f, 500.0f, mgr.getReading(0).distance_mm);

    TEST_ASSERT_EQUAL(SensorStatus::OK, mgr.getReading(1).status);
    TEST_ASSERT_FLOAT_WITHIN(2.0f, 200.0f, mgr.getReading(1).distance_mm);

    TEST_ASSERT_EQUAL(SensorStatus::TIMEOUT, mgr.getReading(2).status);
    TEST_ASSERT_EQUAL_FLOAT(-1.0f, mgr.getReading(2).distance_mm);

    // Min valid distance should be ~200 mm from sensor 1 (ignoring sensor 2's timeout)
    TEST_ASSERT_FLOAT_WITHIN(2.0f, 200.0f, mgr.getMinValidDistanceMm());
}

/**
 * Test 11: UltrasonicManager round-robin scheduling stepping across all 6 channels.
 */
void test_ultrasonic_manager_round_robin_scheduling() {
    UltrasonicManager mgr;
    TEST_ASSERT_EQUAL(0, mgr.getCurrentSensorIndex());

    MockUltrasonicIO mocks[6];
    for (uint8_t i = 0; i < 6; ++i) {
        mocks[i].setMode(MockUltrasonicIO::MockMode::FIXED_PULSE);
        mocks[i].setPulseUs(1166); // ~200 mm
        mgr.getDriver(i).setIO(&mocks[i]);
        mgr.getDriver(i).begin();
    }

    // Step 0 -> updates sensor 0, advances to 1
    mgr.update();
    TEST_ASSERT_EQUAL(1, mgr.getCurrentSensorIndex());
    TEST_ASSERT_EQUAL(SensorStatus::OK, mgr.getReading(0).status);

    // Step 1 -> updates sensor 1, advances to 2
    mgr.update();
    TEST_ASSERT_EQUAL(2, mgr.getCurrentSensorIndex());
    TEST_ASSERT_EQUAL(SensorStatus::OK, mgr.getReading(1).status);

    // Step 2 -> updates sensor 2, advances to 3
    mgr.update();
    TEST_ASSERT_EQUAL(3, mgr.getCurrentSensorIndex());
    TEST_ASSERT_EQUAL(SensorStatus::OK, mgr.getReading(2).status);

    // Step 3 -> updates sensor 3, advances to 4
    mgr.update();
    TEST_ASSERT_EQUAL(4, mgr.getCurrentSensorIndex());
    TEST_ASSERT_EQUAL(SensorStatus::OK, mgr.getReading(3).status);

    // Step 4 -> updates sensor 4, advances to 5
    mgr.update();
    TEST_ASSERT_EQUAL(5, mgr.getCurrentSensorIndex());
    TEST_ASSERT_EQUAL(SensorStatus::OK, mgr.getReading(4).status);

    // Step 5 -> updates sensor 5, wraps to 0
    mgr.update();
    TEST_ASSERT_EQUAL(0, mgr.getCurrentSensorIndex());
    TEST_ASSERT_EQUAL(SensorStatus::OK, mgr.getReading(5).status);
}

/**
 * Test 12: Six-sensor configuration represented and accessible.
 */
void test_ultrasonic_manager_six_sensor_configuration() {
    UltrasonicManager mgr;
    TEST_ASSERT_EQUAL(6, config::ULTRASONIC_COUNT);
    TEST_ASSERT_EQUAL(6, UltrasonicManager::getSensorCount());

    for (uint8_t i = 0; i < 6; ++i) {
        const auto& reading = mgr.getReading(i);
        TEST_ASSERT_EQUAL(i, reading.sensor_id);
        TEST_ASSERT_EQUAL(SensorStatus::UNINITIALIZED, reading.status);
        TEST_ASSERT_EQUAL_FLOAT(-1.0f, reading.distance_mm);
    }

    // Invalid index returns sentinel
    const auto& invalidReading = mgr.getReading(6);
    TEST_ASSERT_EQUAL(0xFF, invalidReading.sensor_id);
    TEST_ASSERT_EQUAL(SensorStatus::INVALID, invalidReading.status);
    TEST_ASSERT_EQUAL_FLOAT(-1.0f, invalidReading.distance_mm);
}

/**
 * Test 13: Full 6-sensor minimum distance aggregation ([500, 350, 120, 800, 250, 1000] mm).
 */
void test_ultrasonic_manager_full_6_sensor_min_aggregation() {
    UltrasonicManager mgr;

    // Pulse durations corresponding to target distances:
    // 500 mm -> 2915 us
    // 350 mm -> 2041 us
    // 120 mm -> 700 us  (Minimum)
    // 800 mm -> 4665 us
    // 250 mm -> 1458 us
    // 1000 mm -> 5831 us
    MockUltrasonicIO mock0(MockUltrasonicIO::MockMode::FIXED_PULSE, 2915);
    MockUltrasonicIO mock1(MockUltrasonicIO::MockMode::FIXED_PULSE, 2041);
    MockUltrasonicIO mock2(MockUltrasonicIO::MockMode::FIXED_PULSE, 700);
    MockUltrasonicIO mock3(MockUltrasonicIO::MockMode::FIXED_PULSE, 4665);
    MockUltrasonicIO mock4(MockUltrasonicIO::MockMode::FIXED_PULSE, 1458);
    MockUltrasonicIO mock5(MockUltrasonicIO::MockMode::FIXED_PULSE, 5831);

    mgr.getDriver(0).setIO(&mock0);
    mgr.getDriver(1).setIO(&mock1);
    mgr.getDriver(2).setIO(&mock2);
    mgr.getDriver(3).setIO(&mock3);
    mgr.getDriver(4).setIO(&mock4);
    mgr.getDriver(5).setIO(&mock5);

    for (uint8_t i = 0; i < 6; ++i) {
        mgr.getDriver(i).begin();
    }

    // Execute full round-robin cycle (6 updates)
    for (uint8_t i = 0; i < 6; ++i) {
        mgr.update();
    }

    TEST_ASSERT_FLOAT_WITHIN(2.0f, 500.0f, mgr.getReading(0).distance_mm);
    TEST_ASSERT_FLOAT_WITHIN(2.0f, 350.0f, mgr.getReading(1).distance_mm);
    TEST_ASSERT_FLOAT_WITHIN(2.0f, 120.0f, mgr.getReading(2).distance_mm);
    TEST_ASSERT_FLOAT_WITHIN(2.0f, 800.0f, mgr.getReading(3).distance_mm);
    TEST_ASSERT_FLOAT_WITHIN(2.0f, 250.0f, mgr.getReading(4).distance_mm);
    TEST_ASSERT_FLOAT_WITHIN(2.0f, 1000.0f, mgr.getReading(5).distance_mm);

    // Minimum distance among all 6 active channels must be 120 mm
    TEST_ASSERT_FLOAT_WITHIN(2.0f, 120.0f, mgr.getMinValidDistanceMm());
    TEST_ASSERT_TRUE(mgr.areAllHealthy());
}

/**
 * Test 14: Partial sensor fault tolerance (Sensors 2 and 4 fail).
 */
void test_ultrasonic_manager_partial_fault_tolerance() {
    UltrasonicManager mgr;

    MockUltrasonicIO mock0(MockUltrasonicIO::MockMode::FIXED_PULSE, 2915); // 500 mm (OK)
    MockUltrasonicIO mock1(MockUltrasonicIO::MockMode::FIXED_PULSE, 2041); // 350 mm (OK)
    MockUltrasonicIO mock2(MockUltrasonicIO::MockMode::TIMEOUT);          // Fault/Timeout
    MockUltrasonicIO mock3(MockUltrasonicIO::MockMode::FIXED_PULSE, 4665); // 800 mm (OK)
    MockUltrasonicIO mock4(MockUltrasonicIO::MockMode::TIMEOUT);          // Fault/Timeout
    MockUltrasonicIO mock5(MockUltrasonicIO::MockMode::FIXED_PULSE, 1458); // 250 mm (OK)

    mgr.getDriver(0).setIO(&mock0);
    mgr.getDriver(1).setIO(&mock1);
    mgr.getDriver(2).setIO(&mock2);
    mgr.getDriver(3).setIO(&mock3);
    mgr.getDriver(4).setIO(&mock4);
    mgr.getDriver(5).setIO(&mock5);

    for (uint8_t i = 0; i < 6; ++i) {
        mgr.getDriver(i).begin();
    }

    // Execute full round-robin cycle (6 updates)
    for (uint8_t i = 0; i < 6; ++i) {
        mgr.update();
    }

    // Healthy sensors remain usable and report OK status
    TEST_ASSERT_EQUAL(SensorStatus::OK, mgr.getReading(0).status);
    TEST_ASSERT_EQUAL(SensorStatus::OK, mgr.getReading(1).status);
    TEST_ASSERT_EQUAL(SensorStatus::OK, mgr.getReading(3).status);
    TEST_ASSERT_EQUAL(SensorStatus::OK, mgr.getReading(5).status);

    // Faulty sensors report TIMEOUT and -1.0 mm
    TEST_ASSERT_EQUAL(SensorStatus::TIMEOUT, mgr.getReading(2).status);
    TEST_ASSERT_EQUAL_FLOAT(-1.0f, mgr.getReading(2).distance_mm);
    TEST_ASSERT_EQUAL(SensorStatus::TIMEOUT, mgr.getReading(4).status);
    TEST_ASSERT_EQUAL_FLOAT(-1.0f, mgr.getReading(4).distance_mm);

    // Aggregate health correctly reports false due to sensors 2 and 4
    TEST_ASSERT_FALSE(mgr.areAllHealthy());

    // Minimum distance computes correctly over the healthy sensors (min among 500, 350, 800, 250 is 250 mm)
    TEST_ASSERT_FLOAT_WITHIN(2.0f, 250.0f, mgr.getMinValidDistanceMm());
}

/**
 * Test 15: Pin-zero guard safely inhibits unconfigured sensors.
 */
void test_ultrasonic_manager_pin_zero_guard() {
    HCSR04Config cfg{};
    cfg.sensor_id = 5;
    cfg.trig_pin  = 0; // Unconfigured / disabled
    cfg.echo_pin  = 0; // Unconfigured / disabled

    HCSR04Driver driver(cfg);
    TEST_ASSERT_FALSE(driver.begin());
    TEST_ASSERT_EQUAL(SensorStatus::UNINITIALIZED, driver.getStatus());
    TEST_ASSERT_FALSE(driver.isHealthy());

    driver.update();
    TEST_ASSERT_EQUAL(SensorStatus::UNINITIALIZED, driver.getStatus());
    TEST_ASSERT_EQUAL_FLOAT(-1.0f, driver.getReading().distance_mm);
}

// =============================================================================
// IMU (MPU6050 / MPU6500) Driver Unit Tests (Simulation / Host-only)
// =============================================================================

/**
 * Test 1: IMU driver in UNINITIALIZED state before begin() or on bus failure.
 */
void test_imu_uninitialized_and_bus_failure() {
    MockIMUBus mockBus;
    mockBus.setInitSuccess(false);

    MPUConfig cfg{};
    cfg.sensor_id = 0;

    MPUDriver imuDriver(cfg, &mockBus);
    TEST_ASSERT_EQUAL(SensorStatus::UNINITIALIZED, imuDriver.getStatus());
    TEST_ASSERT_FALSE(imuDriver.isHealthy());

    // begin() fails because bus init fails
    TEST_ASSERT_FALSE(imuDriver.begin());
    TEST_ASSERT_EQUAL(SensorStatus::UNINITIALIZED, imuDriver.getStatus());

    // update() on uninitialized driver stays UNINITIALIZED
    imuDriver.update();
    TEST_ASSERT_EQUAL(SensorStatus::UNINITIALIZED, imuDriver.getStatus());
}

/**
 * Test 2: MPU6050 auto-detection via WHO_AM_I (0x68) and initialization.
 */
void test_mpu6050_auto_detection_and_initialization() {
    MockIMUBus mockBus;
    mockBus.setWhoAmI(0x68);

    MPUConfig cfg{};
    cfg.sensor_id     = 0;
    cfg.expected_type = IMUDeviceType::UNKNOWN;  // Auto-detect
    cfg.accel_scale   = AccelScale::SCALE_4G;
    cfg.gyro_scale    = GyroScale::SCALE_500DPS;

    MPUDriver imuDriver(cfg, &mockBus);
    TEST_ASSERT_TRUE(imuDriver.begin());
    TEST_ASSERT_EQUAL(IMUDeviceType::MPU6050, imuDriver.getDetectedType());
    TEST_ASSERT_EQUAL(SensorStatus::INITIALIZING, imuDriver.getStatus());

    // Verify PWR_MGMT_1 was cleared (wake up from sleep)
    TEST_ASSERT_EQUAL(0x00, mockBus.getRegister(MockIMUBus::REG_PWR_MGMT_1));
    // Verify Accel config register (4G = 1 -> 1 << 3 = 0x08)
    TEST_ASSERT_EQUAL(0x08, mockBus.getRegister(MockIMUBus::REG_ACCEL_CONFIG));
    // Verify Gyro config register (500DPS = 1 -> 1 << 3 = 0x08)
    TEST_ASSERT_EQUAL(0x08, mockBus.getRegister(MockIMUBus::REG_GYRO_CONFIG));
}

/**
 * Test 3: MPU6500 auto-detection via WHO_AM_I (0x70) and initialization.
 */
void test_mpu6500_auto_detection_and_initialization() {
    MockIMUBus mockBus;
    mockBus.setWhoAmI(0x70);

    MPUConfig cfg{};
    cfg.sensor_id     = 0;
    cfg.expected_type = IMUDeviceType::UNKNOWN;  // Auto-detect

    MPUDriver imuDriver(cfg, &mockBus);
    TEST_ASSERT_TRUE(imuDriver.begin());
    TEST_ASSERT_EQUAL(IMUDeviceType::MPU6500, imuDriver.getDetectedType());
    TEST_ASSERT_EQUAL(SensorStatus::INITIALIZING, imuDriver.getStatus());
}

/**
 * Test 4: Unexpected WHO_AM_I response rejection.
 */
void test_imu_unexpected_who_am_i_rejection() {
    MockIMUBus mockBus;
    mockBus.setWhoAmI(0xFF);  // Invalid ID

    MPUConfig cfg{};
    cfg.sensor_id     = 0;
    cfg.expected_type = IMUDeviceType::MPU6050;  // Strictly expect MPU6050

    MPUDriver imuDriver(cfg, &mockBus);
    TEST_ASSERT_FALSE(imuDriver.begin());
    TEST_ASSERT_EQUAL(SensorStatus::UNINITIALIZED, imuDriver.getStatus());
}

/**
 * Test 5: Valid accelerometer and gyroscope measurement conversion to engineering units.
 */
void test_imu_valid_accel_gyro_readings() {
    MockIMUBus mockBus;
    mockBus.setWhoAmI(0x68);

    // Accel scale ±4g (8192 LSB/g) -> raw 8192 on Z = +1.0g (gravity)
    // Gyro scale ±500 deg/s (65.5 LSB/(deg/s)) -> raw 655 on X = +10.0 deg/s, -1310 on Y = -20.0 deg/s
    mockBus.setRawAccel(0, 0, 8192);
    mockBus.setRawGyro(655, -1310, 0);

    MPUConfig cfg{};
    cfg.sensor_id   = 0;
    cfg.accel_scale = AccelScale::SCALE_4G;
    cfg.gyro_scale  = GyroScale::SCALE_500DPS;

    MPUDriver imuDriver(cfg, &mockBus);
    TEST_ASSERT_TRUE(imuDriver.begin());

    imuDriver.update();
    TEST_ASSERT_EQUAL(SensorStatus::OK, imuDriver.getStatus());
    TEST_ASSERT_TRUE(imuDriver.isHealthy());

    const auto& reading = imuDriver.getReading();
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, reading.accel_x);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, reading.accel_y);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, reading.accel_z);

    TEST_ASSERT_FLOAT_WITHIN(0.1f, 10.0f, reading.gyro_x);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, -20.0f, reading.gyro_y);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, reading.gyro_z);

    const auto& health = imuDriver.getHealth();
    TEST_ASSERT_EQUAL(0, health.consecutive_failures);
    TEST_ASSERT_EQUAL(1, health.successful_measurements);
}

/**
 * Test 6: Communication failure handling during acquisition.
 */
void test_imu_communication_failure_handling() {
    MockIMUBus mockBus;
    mockBus.setWhoAmI(0x68);

    MPUConfig cfg{};
    cfg.sensor_id = 0;

    MPUDriver imuDriver(cfg, &mockBus);
    TEST_ASSERT_TRUE(imuDriver.begin());

    // Inject bus communication failure
    mockBus.setCommFailure(true);
    imuDriver.update();

    TEST_ASSERT_EQUAL(SensorStatus::TIMEOUT, imuDriver.getStatus());
    TEST_ASSERT_FALSE(imuDriver.isHealthy());
    TEST_ASSERT_EQUAL(1, imuDriver.getHealth().consecutive_failures);
}

/**
 * Test 7: Consecutive failures escalate to FAULT, and valid reading recovers.
 */
void test_imu_fault_escalation_and_recovery() {
    MockIMUBus mockBus;
    mockBus.setWhoAmI(0x68);

    MPUConfig cfg{};
    cfg.sensor_id                = 0;
    cfg.max_consecutive_failures = 3;

    MPUDriver imuDriver(cfg, &mockBus);
    TEST_ASSERT_TRUE(imuDriver.begin());

    mockBus.setCommFailure(true);

    // Failures 1 & 2 -> TIMEOUT
    imuDriver.update();
    TEST_ASSERT_EQUAL(SensorStatus::TIMEOUT, imuDriver.getStatus());
    imuDriver.update();
    TEST_ASSERT_EQUAL(SensorStatus::TIMEOUT, imuDriver.getStatus());

    // Failure 3 -> Escalate to FAULT
    imuDriver.update();
    TEST_ASSERT_EQUAL(SensorStatus::FAULT, imuDriver.getStatus());
    TEST_ASSERT_FALSE(imuDriver.isHealthy());
    TEST_ASSERT_EQUAL(3, imuDriver.getHealth().consecutive_failures);

    // Recovery
    mockBus.setCommFailure(false);
    mockBus.setRawAccel(0, 0, 8192);
    imuDriver.update();

    TEST_ASSERT_EQUAL(SensorStatus::OK, imuDriver.getStatus());
    TEST_ASSERT_TRUE(imuDriver.isHealthy());
    TEST_ASSERT_EQUAL(0, imuDriver.getHealth().consecutive_failures);
}

/**
 * Test 8: Reading timestamps update monotonically.
 */
void test_imu_timestamp_behavior() {
    MockIMUBus mockBus;
    mockBus.setWhoAmI(0x68);
    mockBus.setRawAccel(0, 0, 8192);

    MPUConfig cfg{};
    cfg.sensor_id = 0;

    MPUDriver imuDriver(cfg, &mockBus);
    TEST_ASSERT_TRUE(imuDriver.begin());

    imuDriver.update();
    uint32_t t1 = imuDriver.getReading().timestamp_ms;

    imuDriver.update();
    uint32_t t2 = imuDriver.getReading().timestamp_ms;

    TEST_ASSERT_TRUE(t2 >= t1);
}

// =============================================================================
// Wheel Encoder Driver Unit Tests (Simulation / Host-only)
// =============================================================================

/**
 * Test 1: Encoder uninitialized state and placeholder pin rejection.
 */
void test_encoder_uninitialized_and_placeholder_pins() {
    MockEncoderHAL mockHAL;
    EncoderConfig cfg{};
    cfg.sensor_id = 0;
    cfg.pin_a     = 0;  // Placeholder
    cfg.pin_b     = 0;  // Placeholder

    EncoderDriver driver(cfg, &mockHAL);
    TEST_ASSERT_EQUAL(SensorStatus::UNINITIALIZED, driver.getStatus());
    TEST_ASSERT_FALSE(driver.isHealthy());
    TEST_ASSERT_EQUAL(0, driver.getCount());

    // begin() fails safely with placeholder pins
    TEST_ASSERT_FALSE(driver.begin());
    TEST_ASSERT_EQUAL(SensorStatus::UNINITIALIZED, driver.getStatus());
    TEST_ASSERT_FALSE(driver.isHealthy());

    // update() maintains UNINITIALIZED status
    driver.update();
    TEST_ASSERT_EQUAL(SensorStatus::UNINITIALIZED, driver.getStatus());
    TEST_ASSERT_EQUAL(EncoderDirection::STATIONARY, driver.getReading().direction);
}

/**
 * Test 2: Encoder HAL init failure handling.
 */
void test_encoder_hal_init_failure() {
    MockEncoderHAL mockHAL;
    mockHAL.setInitSuccess(false);

    EncoderConfig cfg{};
    cfg.sensor_id = 0;
    cfg.pin_a     = 18;
    cfg.pin_b     = 19;

    EncoderDriver driver(cfg, &mockHAL);
    TEST_ASSERT_FALSE(driver.begin());
    TEST_ASSERT_EQUAL(SensorStatus::UNINITIALIZED, driver.getStatus());
    TEST_ASSERT_FALSE(driver.isHealthy());
}

/**
 * Test 3: Forward quadrature rotation (CW: A leads B).
 */
void test_encoder_forward_quadrature_cw() {
    MockEncoderHAL mockHAL(EncoderDecodingMode::X4);
    EncoderConfig cfg{};
    cfg.sensor_id     = 0;
    cfg.pin_a         = 18;
    cfg.pin_b         = 19;
    cfg.decoding_mode = EncoderDecodingMode::X4;

    EncoderDriver driver(cfg, &mockHAL);
    TEST_ASSERT_TRUE(driver.begin());

    // Rotate 100 quadrature steps forward (CW)
    mockHAL.stepQuad(+1, 100, 1000);
    driver.update();

    TEST_ASSERT_EQUAL(SensorStatus::OK, driver.getStatus());
    TEST_ASSERT_TRUE(driver.isHealthy());
    TEST_ASSERT_EQUAL(100, driver.getCount());
    TEST_ASSERT_EQUAL(EncoderDirection::FORWARD, driver.getReading().direction);
    TEST_ASSERT_EQUAL(100, driver.getReading().pulses_since_last_update);
    TEST_ASSERT_EQUAL(100, driver.getHealth().total_pulses);
}

/**
 * Test 4: Reverse quadrature rotation (CCW: B leads A).
 */
void test_encoder_reverse_quadrature_ccw() {
    MockEncoderHAL mockHAL(EncoderDecodingMode::X4);
    EncoderConfig cfg{};
    cfg.sensor_id     = 0;
    cfg.pin_a         = 18;
    cfg.pin_b         = 19;
    cfg.decoding_mode = EncoderDecodingMode::X4;

    EncoderDriver driver(cfg, &mockHAL);
    TEST_ASSERT_TRUE(driver.begin());

    // Rotate 150 quadrature steps reverse (CCW)
    mockHAL.stepQuad(-1, 150, 1000);
    driver.update();

    TEST_ASSERT_EQUAL(SensorStatus::OK, driver.getStatus());
    TEST_ASSERT_TRUE(driver.isHealthy());
    TEST_ASSERT_EQUAL(-150, driver.getCount());
    TEST_ASSERT_EQUAL(EncoderDirection::REVERSE, driver.getReading().direction);
    TEST_ASSERT_EQUAL(150, driver.getReading().pulses_since_last_update);
    TEST_ASSERT_EQUAL(150, driver.getHealth().total_pulses);
}

/**
 * Test 5: Exact counts per revolution for X1, X2, and X4 decoding modes (600 PPR).
 */
void test_encoder_decoding_modes_per_revolution() {
    // 600 waveform cycles / revolution validated experimentally.
    // X1 Mode -> 600 counts / rev (1 count per cycle)
    {
        MockEncoderHAL mockHAL(EncoderDecodingMode::X1);
        EncoderConfig cfg{};
        cfg.sensor_id     = 0;
        cfg.pin_a         = 18;
        cfg.pin_b         = 19;
        cfg.ppr           = 600;
        cfg.decoding_mode = EncoderDecodingMode::X1;

        EncoderDriver driver(cfg, &mockHAL);
        TEST_ASSERT_TRUE(driver.begin());
        TEST_ASSERT_EQUAL(600, driver.getCountsPerRevolution());

        // Rotate exactly 1 revolution (600 cycles = 2400 quad steps)
        mockHAL.rotateRevolutions(1.0f, +1, 500);
        driver.update();
        TEST_ASSERT_EQUAL(600, driver.getCount());
    }

    // X2 Mode -> 1200 counts / rev (2 counts per cycle)
    {
        MockEncoderHAL mockHAL(EncoderDecodingMode::X2);
        EncoderConfig cfg{};
        cfg.sensor_id     = 0;
        cfg.pin_a         = 18;
        cfg.pin_b         = 19;
        cfg.ppr           = 600;
        cfg.decoding_mode = EncoderDecodingMode::X2;

        EncoderDriver driver(cfg, &mockHAL);
        TEST_ASSERT_TRUE(driver.begin());
        TEST_ASSERT_EQUAL(1200, driver.getCountsPerRevolution());

        mockHAL.rotateRevolutions(1.0f, +1, 500);
        driver.update();
        TEST_ASSERT_EQUAL(1200, driver.getCount());
    }

    // X4 Mode -> 2400 counts / rev (4 counts per cycle)
    {
        MockEncoderHAL mockHAL(EncoderDecodingMode::X4);
        EncoderConfig cfg{};
        cfg.sensor_id     = 0;
        cfg.pin_a         = 18;
        cfg.pin_b         = 19;
        cfg.ppr           = 600;
        cfg.decoding_mode = EncoderDecodingMode::X4;

        EncoderDriver driver(cfg, &mockHAL);
        TEST_ASSERT_TRUE(driver.begin());
        TEST_ASSERT_EQUAL(2400, driver.getCountsPerRevolution());

        mockHAL.rotateRevolutions(1.0f, +1, 500);
        driver.update();
        TEST_ASSERT_EQUAL(2400, driver.getCount());
    }
}

/**
 * Test 6: Direction polarity parameter (no silent inversion).
 */
void test_encoder_direction_polarity_configuration() {
    MockEncoderHAL mockHAL(EncoderDecodingMode::X4);
    EncoderConfig cfg{};
    cfg.sensor_id         = 0;
    cfg.pin_a             = 18;
    cfg.pin_b             = 19;
    cfg.decoding_mode     = EncoderDecodingMode::X4;
    cfg.reverse_direction = true;  // Invert polarity

    EncoderDriver driver(cfg, &mockHAL);
    TEST_ASSERT_TRUE(driver.begin());

    // Hardware steps forward (+100)
    mockHAL.stepQuad(+1, 100, 1000);
    driver.update();

    // Driver reports inverted sign (-100) and REVERSE direction
    TEST_ASSERT_EQUAL(-100, driver.getCount());
    TEST_ASSERT_EQUAL(EncoderDirection::REVERSE, driver.getReading().direction);
}

/**
 * Test 7: Z Channel Index event tracking.
 */
void test_encoder_z_index_tracking() {
    MockEncoderHAL mockHAL(EncoderDecodingMode::X4);
    EncoderConfig cfg{};
    cfg.sensor_id     = 0;
    cfg.pin_a         = 18;
    cfg.pin_b         = 19;
    cfg.pin_z         = 23;
    cfg.decoding_mode = EncoderDecodingMode::X4;

    EncoderDriver driver(cfg, &mockHAL);
    TEST_ASSERT_TRUE(driver.begin());

    // Rotate 3 full marked revolutions (should generate 3 index events)
    mockHAL.rotateRevolutions(3.0f, +1, 1500);
    driver.update();

    TEST_ASSERT_EQUAL(3 * 2400, driver.getCount());
    TEST_ASSERT_EQUAL(3, driver.getIndexCount());
    TEST_ASSERT_EQUAL(3, driver.getHealth().index_events);
}

/**
 * Test 8: Stationary / No-motion behavior.
 */
void test_encoder_stationary_no_motion() {
    MockEncoderHAL mockHAL(EncoderDecodingMode::X4);
    EncoderConfig cfg{};
    cfg.sensor_id = 0;
    cfg.pin_a     = 18;
    cfg.pin_b     = 19;

    EncoderDriver driver(cfg, &mockHAL);
    TEST_ASSERT_TRUE(driver.begin());

    // Step 50 pulses
    mockHAL.stepQuad(+1, 50, 500);
    driver.update();
    TEST_ASSERT_EQUAL(50, driver.getCount());
    TEST_ASSERT_EQUAL(EncoderDirection::FORWARD, driver.getReading().direction);

    // No further pulses (stationary)
    driver.update();
    TEST_ASSERT_EQUAL(50, driver.getCount());
    TEST_ASSERT_EQUAL(0, driver.getReading().pulses_since_last_update);
    TEST_ASSERT_EQUAL(EncoderDirection::STATIONARY, driver.getReading().direction);
}

/**
 * Test 9: Invalid quadrature transition glitch tracking.
 */
void test_encoder_invalid_quadrature_glitch_tracking() {
    MockEncoderHAL mockHAL(EncoderDecodingMode::X4);
    EncoderConfig cfg{};
    cfg.sensor_id = 0;
    cfg.pin_a     = 18;
    cfg.pin_b     = 19;

    EncoderDriver driver(cfg, &mockHAL);
    TEST_ASSERT_TRUE(driver.begin());

    mockHAL.injectInvalidTransition();
    mockHAL.injectInvalidTransition();
    driver.update();

    TEST_ASSERT_EQUAL(2, driver.getHealth().invalid_transitions);
    TEST_ASSERT_EQUAL(SensorStatus::OK, driver.getStatus());
}

/**
 * Test 10: Count and index reset functionality.
 */
void test_encoder_reset_functionality() {
    MockEncoderHAL mockHAL(EncoderDecodingMode::X4);
    EncoderConfig cfg{};
    cfg.sensor_id = 0;
    cfg.pin_a     = 18;
    cfg.pin_b     = 19;

    EncoderDriver driver(cfg, &mockHAL);
    TEST_ASSERT_TRUE(driver.begin());

    mockHAL.stepQuad(+1, 300, 1000);
    mockHAL.triggerIndex(1000);
    driver.update();

    TEST_ASSERT_EQUAL(300, driver.getCount());
    TEST_ASSERT_EQUAL(1, driver.getIndexCount());

    driver.resetCount();
    TEST_ASSERT_EQUAL(0, driver.getCount());
    TEST_ASSERT_EQUAL(0, driver.getIndexCount());
    TEST_ASSERT_EQUAL(EncoderDirection::STATIONARY, driver.getReading().direction);
}

/**
 * Test 11: Dual wheel independence in EncoderManager.
 */
void test_encoder_manager_dual_wheel_independence() {
    MockEncoderHAL mockLeft(EncoderDecodingMode::X4);
    MockEncoderHAL mockRight(EncoderDecodingMode::X4);

    EncoderConfig leftCfg{};
    leftCfg.sensor_id = 0;
    leftCfg.label     = "LEFT";
    leftCfg.pin_a     = 18;
    leftCfg.pin_b     = 19;

    EncoderConfig rightCfg{};
    rightCfg.sensor_id = 1;
    rightCfg.label     = "RIGHT";
    rightCfg.pin_a     = 21;
    rightCfg.pin_b     = 22;

    EncoderManager mgr(leftCfg, rightCfg);
    mgr.getLeftDriver().setHAL(&mockLeft);
    mgr.getRightDriver().setHAL(&mockRight);

    TEST_ASSERT_TRUE(mgr.begin());

    // Turn Left wheel forward 100 steps, Right wheel reverse 200 steps
    mockLeft.stepQuad(+1, 100, 100);
    mockRight.stepQuad(-1, 200, 100);

    mgr.update();

    const auto& wheelData = mgr.getWheelData();
    TEST_ASSERT_EQUAL(100, wheelData.left.count);
    TEST_ASSERT_EQUAL(EncoderDirection::FORWARD, wheelData.left.direction);

    TEST_ASSERT_EQUAL(-200, wheelData.right.count);
    TEST_ASSERT_EQUAL(EncoderDirection::REVERSE, wheelData.right.direction);

    TEST_ASSERT_TRUE(mgr.areAllHealthy());
}

/**
 * Test 12: Encoder RPM calculation — stationary is zero.
 */
void test_encoder_rpm_stationary_is_zero() {
    MockEncoderHAL mockHAL(EncoderDecodingMode::X4);
    EncoderConfig cfg{};
    cfg.sensor_id = 0;
    cfg.pin_a     = 18;
    cfg.pin_b     = 19;
    cfg.ppr       = 600;
    cfg.decoding_mode = EncoderDecodingMode::X4;

    EncoderDriver driver(cfg, &mockHAL);
    TEST_ASSERT_TRUE(driver.begin());

    driver.update(1000);
    driver.update(1050);

    TEST_ASSERT_EQUAL_FLOAT(0.0f, driver.getReading().rpm);
    TEST_ASSERT_EQUAL(EncoderDirection::STATIONARY, driver.getReading().direction);
}

/**
 * Test 13: Encoder RPM calculation — positive delta (CW / forward).
 */
void test_encoder_rpm_positive_forward() {
    MockEncoderHAL mockHAL(EncoderDecodingMode::X4);
    EncoderConfig cfg{};
    cfg.sensor_id = 0;
    cfg.pin_a     = 18;
    cfg.pin_b     = 19;
    cfg.ppr       = 600;
    cfg.decoding_mode = EncoderDecodingMode::X4;

    EncoderDriver driver(cfg, &mockHAL);
    TEST_ASSERT_TRUE(driver.begin());

    driver.update(1000);

    // Step 200 counts in 50 ms -> (200 / 2400) * (60000 / 50) = 100.0 RPM
    mockHAL.stepQuad(+1, 200, 1050);
    driver.update(1050);

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 100.0f, driver.getReading().rpm);
    TEST_ASSERT_EQUAL(EncoderDirection::FORWARD, driver.getReading().direction);
}

/**
 * Test 14: Encoder RPM calculation — negative delta (CCW / reverse).
 */
void test_encoder_rpm_negative_reverse() {
    MockEncoderHAL mockHAL(EncoderDecodingMode::X4);
    EncoderConfig cfg{};
    cfg.sensor_id = 0;
    cfg.pin_a     = 18;
    cfg.pin_b     = 19;
    cfg.ppr       = 600;
    cfg.decoding_mode = EncoderDecodingMode::X4;

    EncoderDriver driver(cfg, &mockHAL);
    TEST_ASSERT_TRUE(driver.begin());

    driver.update(1000);

    // Step -100 counts in 50 ms -> (-100 / 2400) * (60000 / 50) = -50.0 RPM
    mockHAL.stepQuad(-1, 100, 1050);
    driver.update(1050);

    TEST_ASSERT_FLOAT_WITHIN(0.01f, -50.0f, driver.getReading().rpm);
    TEST_ASSERT_EQUAL(EncoderDirection::REVERSE, driver.getReading().direction);
}

/**
 * Test 15: Encoder RPM calculation — zero elapsed time and uninitialized safety.
 */
void test_encoder_rpm_safety_edge_cases() {
    MockEncoderHAL mockHAL(EncoderDecodingMode::X4);
    EncoderConfig cfg{};
    cfg.sensor_id = 0;
    cfg.pin_a     = 18;
    cfg.pin_b     = 19;
    cfg.ppr       = 600;

    EncoderDriver driver(cfg, &mockHAL);
    TEST_ASSERT_TRUE(driver.begin());

    driver.update(1000);
    mockHAL.stepQuad(+1, 50, 1000);
    driver.update(1000); // 0 ms elapsed

    TEST_ASSERT_EQUAL_FLOAT(0.0f, driver.getReading().rpm);

    // Uninitialized driver with placeholder pins
    EncoderConfig uninitCfg{};
    uninitCfg.sensor_id = 1;
    uninitCfg.pin_a     = 0;
    uninitCfg.pin_b     = 0;

    EncoderDriver uninitDriver(uninitCfg, &mockHAL);
    TEST_ASSERT_FALSE(uninitDriver.begin());
    uninitDriver.update(1050);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, uninitDriver.getReading().rpm);
}

/**
 * Test 16: Encoder RPM calculation — decoding mode scaling (X1, X2, X4).
 */
void test_encoder_rpm_decoding_mode_scaling() {
    // X1 Mode: CPR = 600. 50 counts in 50 ms -> (50 / 600) * (60000 / 50) = 100.0 RPM
    {
        MockEncoderHAL mockHAL(EncoderDecodingMode::X1);
        EncoderConfig cfg{};
        cfg.sensor_id = 0;
        cfg.pin_a = 18; cfg.pin_b = 19;
        cfg.ppr = 600;
        cfg.decoding_mode = EncoderDecodingMode::X1;

        EncoderDriver driver(cfg, &mockHAL);
        TEST_ASSERT_TRUE(driver.begin());
        driver.update(1000);
        mockHAL.stepQuad(+1, 50 * 4, 1050);
        driver.update(1050);
        TEST_ASSERT_FLOAT_WITHIN(0.01f, 100.0f, driver.getReading().rpm);
    }

    // X2 Mode: CPR = 1200. 100 counts in 50 ms -> (100 / 1200) * (60000 / 50) = 100.0 RPM
    {
        MockEncoderHAL mockHAL(EncoderDecodingMode::X2);
        EncoderConfig cfg{};
        cfg.sensor_id = 0;
        cfg.pin_a = 18; cfg.pin_b = 19;
        cfg.ppr = 600;
        cfg.decoding_mode = EncoderDecodingMode::X2;

        EncoderDriver driver(cfg, &mockHAL);
        TEST_ASSERT_TRUE(driver.begin());
        driver.update(1000);
        mockHAL.stepQuad(+1, 100 * 2, 1050);
        driver.update(1050);
        TEST_ASSERT_FLOAT_WITHIN(0.01f, 100.0f, driver.getReading().rpm);
    }
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

    // State Machine & Core Invariant Tests (15 tests)
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
    RUN_TEST(test_estop_forces_safe_and_latches);
    RUN_TEST(test_estop_recovery_rejected_while_physically_active);
    RUN_TEST(test_command_handler_rejects_enable_when_safe);
    RUN_TEST(test_wheelchair_interface_simulation_only);

    // HC-SR04 Ultrasonic Driver & Manager Unit Tests (15 tests)
    RUN_TEST(test_hcsr04_uninitialized_and_placeholder_pins);
    RUN_TEST(test_hcsr04_valid_distance_measurement);
    RUN_TEST(test_hcsr04_minimum_range_boundary);
    RUN_TEST(test_hcsr04_maximum_range_boundary);
    RUN_TEST(test_hcsr04_timeout_no_echo);
    RUN_TEST(test_hcsr04_pulse_exceeds_timeout);
    RUN_TEST(test_hcsr04_repeated_measurements);
    RUN_TEST(test_hcsr04_fault_escalation_and_recovery);
    RUN_TEST(test_hcsr04_never_reports_zero_for_invalid);
    RUN_TEST(test_ultrasonic_manager_aggregation);
    RUN_TEST(test_ultrasonic_manager_round_robin_scheduling);
    RUN_TEST(test_ultrasonic_manager_six_sensor_configuration);
    RUN_TEST(test_ultrasonic_manager_full_6_sensor_min_aggregation);
    RUN_TEST(test_ultrasonic_manager_partial_fault_tolerance);
    RUN_TEST(test_ultrasonic_manager_pin_zero_guard);

    // IMU (MPU6050 / MPU6500) Driver Unit Tests (8 tests)
    RUN_TEST(test_imu_uninitialized_and_bus_failure);
    RUN_TEST(test_mpu6050_auto_detection_and_initialization);
    RUN_TEST(test_mpu6500_auto_detection_and_initialization);
    RUN_TEST(test_imu_unexpected_who_am_i_rejection);
    RUN_TEST(test_imu_valid_accel_gyro_readings);
    RUN_TEST(test_imu_communication_failure_handling);
    RUN_TEST(test_imu_fault_escalation_and_recovery);
    RUN_TEST(test_imu_timestamp_behavior);

    // Wheel Encoder Driver & Manager Unit Tests (16 tests)
    RUN_TEST(test_encoder_uninitialized_and_placeholder_pins);
    RUN_TEST(test_encoder_hal_init_failure);
    RUN_TEST(test_encoder_forward_quadrature_cw);
    RUN_TEST(test_encoder_reverse_quadrature_ccw);
    RUN_TEST(test_encoder_decoding_modes_per_revolution);
    RUN_TEST(test_encoder_direction_polarity_configuration);
    RUN_TEST(test_encoder_z_index_tracking);
    RUN_TEST(test_encoder_stationary_no_motion);
    RUN_TEST(test_encoder_invalid_quadrature_glitch_tracking);
    RUN_TEST(test_encoder_reset_functionality);
    RUN_TEST(test_encoder_manager_dual_wheel_independence);
    RUN_TEST(test_encoder_rpm_stationary_is_zero);
    RUN_TEST(test_encoder_rpm_positive_forward);
    RUN_TEST(test_encoder_rpm_negative_reverse);
    RUN_TEST(test_encoder_rpm_safety_edge_cases);
    RUN_TEST(test_encoder_rpm_decoding_mode_scaling);

    return UNITY_END();
}




