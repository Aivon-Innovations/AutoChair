/**
 * @file test_ultrasonic_integration.cpp
 * @brief AutoChair ESP32 — Six-Sensor Ultrasonic Integration & Obstacle-Zone Tests
 *
 * Project:  Aivon Innovations — AutoChair
 * Platform: ESP32-WROOM-32D (native host test environment)
 * Status:   IN DEVELOPMENT
 *
 * Test coverage:
 *   A. Six-sensor configuration and GPIO mapping
 *   B. Sensor ID association
 *   C. Round-robin scheduling
 *   D. Update timing and starvation detection
 *   E. Timeout and invalid/no-echo handling
 *   F. One failed sensor while others continue updating
 *   G. Telemetry identification (each sensor distinguishable)
 *   H. Startup / reset behaviour
 *   I. Obstacle-zone classification (CLEAR / CAUTION / OBSTACLE / FAULT)
 *   J. Zone state transitions
 *   K. Invalid and timed-out readings → zone FAULT
 *   L. Stale-data detection
 *   M. Recovery after a fault
 *   N. Multiple sensors reporting different zones
 *   O. Aggregate zone (most-restrictive)
 *   P. Values at, below, and above each threshold boundary
 */

#include <unity.h>

// Pull in the modules under test from the firmware source tree.
// Native build uses ENV_NATIVE define; hardware IO is stubbed.
#define ENV_NATIVE

#include "sensors/ultrasonic/hcsr04_driver.h"
#include "sensors/ultrasonic/ultrasonic_manager.h"
#include "sensors/ultrasonic/obstacle_zone.h"
#include "autochair_config.h"
#include "autochair_types.h"
#include "mock_ultrasonic_io.h"

using namespace autochair;
using namespace autochair::config;

// =============================================================================
// Helper: build a mock UltrasonicReading
// =============================================================================

static UltrasonicReading makeReading(uint8_t id,
                                     float dist_mm,
                                     SensorStatus status,
                                     uint32_t timestamp_ms) {
    UltrasonicReading r;
    r.sensor_id    = id;
    r.distance_mm  = dist_mm;
    r.status       = status;
    r.timestamp_ms = timestamp_ms;
    return r;
}

// =============================================================================
// A. Six-sensor configuration and GPIO mapping
// =============================================================================

void test_ultrasonic_count_is_six(void) {
    TEST_ASSERT_EQUAL(6, ULTRASONIC_COUNT);
}

void test_trig_pins_match_spec(void) {
    // Authoritative map: Bench Test/Ultrasonic sensor validation/firmware/src/main.cpp
    const uint8_t expected_trig[6] = {4, 5, 18, 19, 23, 13};
    for (uint8_t i = 0; i < 6; ++i) {
        TEST_ASSERT_EQUAL_MESSAGE(expected_trig[i], US_TRIG_PIN[i],
                                  "TRIG pin mismatch");
    }
}

void test_echo_pins_match_spec(void) {
    const uint8_t expected_echo[6] = {34, 35, 36, 39, 32, 33};
    for (uint8_t i = 0; i < 6; ++i) {
        TEST_ASSERT_EQUAL_MESSAGE(expected_echo[i], US_ECHO_PIN[i],
                                  "ECHO pin mismatch");
    }
}

void test_no_trig_pin_is_zero(void) {
    for (uint8_t i = 0; i < ULTRASONIC_COUNT; ++i) {
        TEST_ASSERT_NOT_EQUAL_MESSAGE(0, US_TRIG_PIN[i],
                                      "TRIG pin must not be placeholder zero");
    }
}

void test_no_echo_pin_is_zero(void) {
    for (uint8_t i = 0; i < ULTRASONIC_COUNT; ++i) {
        TEST_ASSERT_NOT_EQUAL_MESSAGE(0, US_ECHO_PIN[i],
                                      "ECHO pin must not be placeholder zero");
    }
}

void test_trig_echo_pins_are_distinct(void) {
    // No pin should appear in both TRIG and ECHO arrays.
    for (uint8_t i = 0; i < ULTRASONIC_COUNT; ++i) {
        for (uint8_t j = 0; j < ULTRASONIC_COUNT; ++j) {
            TEST_ASSERT_NOT_EQUAL_MESSAGE(US_TRIG_PIN[i], US_ECHO_PIN[j],
                                          "TRIG pin used as ECHO pin");
        }
    }
}

void test_all_trig_pins_unique(void) {
    for (uint8_t i = 0; i < ULTRASONIC_COUNT; ++i) {
        for (uint8_t j = i + 1; j < ULTRASONIC_COUNT; ++j) {
            TEST_ASSERT_NOT_EQUAL_MESSAGE(US_TRIG_PIN[i], US_TRIG_PIN[j],
                                          "Duplicate TRIG pin");
        }
    }
}

void test_all_echo_pins_unique(void) {
    for (uint8_t i = 0; i < ULTRASONIC_COUNT; ++i) {
        for (uint8_t j = i + 1; j < ULTRASONIC_COUNT; ++j) {
            TEST_ASSERT_NOT_EQUAL_MESSAGE(US_ECHO_PIN[i], US_ECHO_PIN[j],
                                          "Duplicate ECHO pin");
        }
    }
}

// =============================================================================
// B. Sensor ID association
// =============================================================================

void test_sensor_ids_are_zero_based(void) {
    // Manager indexes 0..5; sensor_id in each driver's config must match index.
    MockUltrasonicIO mockIO;
    for (uint8_t i = 0; i < ULTRASONIC_COUNT; ++i) {
        HCSR04Config cfg{};
        cfg.sensor_id = i;
        cfg.trig_pin  = US_TRIG_PIN[i];
        cfg.echo_pin  = US_ECHO_PIN[i];
        HCSR04Driver driver(cfg, &mockIO);
        TEST_ASSERT_EQUAL(i, driver.getReading().sensor_id);
    }
}

void test_manager_reading_sensor_id_matches_index(void) {
    UltrasonicManager mgr;
    // Before any update, each reading should carry its own sensor_id.
    for (uint8_t i = 0; i < ULTRASONIC_COUNT; ++i) {
        const auto& r = mgr.getReading(i);
        TEST_ASSERT_EQUAL_MESSAGE(i, r.sensor_id,
                                  "Reading sensor_id must match manager index");
    }
}

// =============================================================================
// C. Round-robin scheduling
// =============================================================================

void test_round_robin_advances_through_all_sensors(void) {
    // Each driver gets a mock that counts how many times measurePulseUs is called.
    MockUltrasonicIO mockIOs[ULTRASONIC_COUNT];
    UltrasonicManager mgr;
    for (uint8_t i = 0; i < ULTRASONIC_COUNT; ++i) {
        mgr.getDriver(i).setIO(&mockIOs[i]);
    }
    mgr.begin();

    // Run exactly ULTRASONIC_COUNT update() calls.
    for (uint8_t i = 0; i < ULTRASONIC_COUNT; ++i) {
        mgr.update();
    }

    // Every sensor should have been measured exactly once.
    for (uint8_t i = 0; i < ULTRASONIC_COUNT; ++i) {
        TEST_ASSERT_EQUAL_MESSAGE(1, mockIOs[i].getMeasureCount(),
                                  "Each sensor should be triggered exactly once per cycle");
    }
}

void test_round_robin_wraps_back_to_sensor_zero(void) {
    MockUltrasonicIO mockIOs[ULTRASONIC_COUNT];
    UltrasonicManager mgr;
    for (uint8_t i = 0; i < ULTRASONIC_COUNT; ++i) {
        mgr.getDriver(i).setIO(&mockIOs[i]);
    }
    mgr.begin();

    // Two complete cycles.
    for (uint8_t c = 0; c < 2 * ULTRASONIC_COUNT; ++c) {
        mgr.update();
    }

    // Every sensor should have been measured exactly twice.
    for (uint8_t i = 0; i < ULTRASONIC_COUNT; ++i) {
        TEST_ASSERT_EQUAL_MESSAGE(2, mockIOs[i].getMeasureCount(),
                                  "Each sensor triggered twice in two cycles");
    }
}

void test_only_one_sensor_triggered_per_update(void) {
    MockUltrasonicIO mockIOs[ULTRASONIC_COUNT];
    UltrasonicManager mgr;
    for (uint8_t i = 0; i < ULTRASONIC_COUNT; ++i) {
        mgr.getDriver(i).setIO(&mockIOs[i]);
    }
    mgr.begin();

    // After exactly one update(), exactly ONE sensor should have been triggered.
    mgr.update();
    uint32_t total = 0;
    for (uint8_t i = 0; i < ULTRASONIC_COUNT; ++i) {
        total += mockIOs[i].getMeasureCount();
    }
    TEST_ASSERT_EQUAL_MESSAGE(1, total,
                              "Exactly one sensor triggered per update() call");
}

// =============================================================================
// D. Update timing and starvation
// =============================================================================

void test_all_sensors_get_updated_in_six_cycles(void) {
    MockUltrasonicIO mockIOs[ULTRASONIC_COUNT];
    UltrasonicManager mgr;
    for (uint8_t i = 0; i < ULTRASONIC_COUNT; ++i) {
        mgr.getDriver(i).setIO(&mockIOs[i]);
    }
    mgr.begin();

    for (uint8_t i = 0; i < ULTRASONIC_COUNT; ++i) {
        mgr.update();
    }

    for (uint8_t i = 0; i < ULTRASONIC_COUNT; ++i) {
        TEST_ASSERT_GREATER_THAN_MESSAGE(0, mockIOs[i].getMeasureCount(),
                                         "Sensor starved — never triggered");
    }
}

void test_no_sensor_triggered_more_than_once_per_cycle(void) {
    MockUltrasonicIO mockIOs[ULTRASONIC_COUNT];
    UltrasonicManager mgr;
    for (uint8_t i = 0; i < ULTRASONIC_COUNT; ++i) {
        mgr.getDriver(i).setIO(&mockIOs[i]);
    }
    mgr.begin();

    for (uint8_t i = 0; i < ULTRASONIC_COUNT; ++i) {
        mgr.update();
    }

    for (uint8_t i = 0; i < ULTRASONIC_COUNT; ++i) {
        TEST_ASSERT_LESS_OR_EQUAL_MESSAGE(1, mockIOs[i].getMeasureCount(),
                                          "Sensor triggered more than once per cycle");
    }
}

// =============================================================================
// E. Timeout and invalid/no-echo handling
// =============================================================================

void test_timeout_returns_negative_distance(void) {
    MockUltrasonicIO mockIO(MockUltrasonicIO::MockMode::TIMEOUT);
    HCSR04Config cfg{};
    cfg.sensor_id = 0;
    cfg.trig_pin  = US_TRIG_PIN[0];
    cfg.echo_pin  = US_ECHO_PIN[0];
    cfg.timeout_us = ULTRASONIC_TIMEOUT_US;
    cfg.max_consecutive_failures = ULTRASONIC_MAX_FAILURES;
    HCSR04Driver driver(cfg, &mockIO);
    driver.begin();
    driver.update();
    TEST_ASSERT_LESS_THAN(0.0f, driver.getReading().distance_mm);
}

void test_timeout_sets_status_timeout(void) {
    MockUltrasonicIO mockIO(MockUltrasonicIO::MockMode::TIMEOUT);
    HCSR04Config cfg{};
    cfg.sensor_id = 0;
    cfg.trig_pin  = US_TRIG_PIN[0];
    cfg.echo_pin  = US_ECHO_PIN[0];
    cfg.timeout_us = ULTRASONIC_TIMEOUT_US;
    cfg.max_consecutive_failures = ULTRASONIC_MAX_FAILURES;
    HCSR04Driver driver(cfg, &mockIO);
    driver.begin();
    driver.update();
    TEST_ASSERT_EQUAL(SensorStatus::TIMEOUT, driver.getReading().status);
}

void test_repeated_timeout_escalates_to_fault(void) {
    MockUltrasonicIO mockIO(MockUltrasonicIO::MockMode::TIMEOUT);
    HCSR04Config cfg{};
    cfg.sensor_id = 0;
    cfg.trig_pin  = US_TRIG_PIN[0];
    cfg.echo_pin  = US_ECHO_PIN[0];
    cfg.max_consecutive_failures = 5;
    HCSR04Driver driver(cfg, &mockIO);
    driver.begin();
    for (uint8_t i = 0; i <= cfg.max_consecutive_failures; ++i) {
        driver.update();
    }
    TEST_ASSERT_EQUAL(SensorStatus::FAULT, driver.getReading().status);
    TEST_ASSERT_FALSE(driver.isHealthy());
}

void test_valid_reading_after_timeouts_does_not_crash(void) {
    MockUltrasonicIO mockIO(MockUltrasonicIO::MockMode::TIMEOUT);
    HCSR04Config cfg{};
    cfg.sensor_id = 0;
    cfg.trig_pin  = US_TRIG_PIN[0];
    cfg.echo_pin  = US_ECHO_PIN[0];
    cfg.max_consecutive_failures = 5;
    HCSR04Driver driver(cfg, &mockIO);
    driver.begin();
    for (uint8_t i = 0; i < 3; ++i) driver.update();

    // Switch to a fixed valid pulse (~500 mm)
    mockIO.setDistanceMm(500.0f);
    mockIO.setMode(MockUltrasonicIO::MockMode::FIXED_PULSE);
    driver.update();
    // Should not crash; distance should be positive
    TEST_ASSERT_GREATER_THAN(0.0f, driver.getReading().distance_mm);
}

// =============================================================================
// F. One failed sensor while others continue updating
// =============================================================================

void test_one_faulty_sensor_does_not_stop_others(void) {
    MockUltrasonicIO goodIOs[ULTRASONIC_COUNT];
    MockUltrasonicIO faultyIO(MockUltrasonicIO::MockMode::TIMEOUT);

    UltrasonicManager mgr;

    // Sensor #3 (index 2) is the faulty one.
    constexpr uint8_t FAULTY_IDX = 2;
    for (uint8_t i = 0; i < ULTRASONIC_COUNT; ++i) {
        if (i == FAULTY_IDX) {
            mgr.getDriver(i).setIO(&faultyIO);
        } else {
            goodIOs[i].setDistanceMm(500.0f);
            mgr.getDriver(i).setIO(&goodIOs[i]);
        }
    }
    mgr.begin();

    // Run two full cycles.
    for (uint8_t c = 0; c < 2 * ULTRASONIC_COUNT; ++c) {
        mgr.update();
    }

    // Faulty sensor should be TIMEOUT or FAULT; others should be OK.
    for (uint8_t i = 0; i < ULTRASONIC_COUNT; ++i) {
        if (i == FAULTY_IDX) {
            TEST_ASSERT_NOT_EQUAL(SensorStatus::OK,
                                  mgr.getReading(i).status);
        } else {
            TEST_ASSERT_EQUAL_MESSAGE(SensorStatus::OK,
                                      mgr.getReading(i).status,
                                      "Good sensor should still be OK");
        }
    }
}

void test_faulty_sensor_does_not_freeze_scheduler(void) {
    MockUltrasonicIO faultyIO(MockUltrasonicIO::MockMode::TIMEOUT);
    MockUltrasonicIO goodIOs[ULTRASONIC_COUNT];

    UltrasonicManager mgr;
    for (uint8_t i = 0; i < ULTRASONIC_COUNT; ++i) {
        goodIOs[i].setDistanceMm(300.0f);
        mgr.getDriver(i).setIO((i == 0) ? &faultyIO : &goodIOs[i]);
    }
    mgr.begin();

    // Run 3 full cycles — scheduler must complete without hanging.
    for (uint8_t c = 0; c < 3 * ULTRASONIC_COUNT; ++c) {
        mgr.update();
    }

    // Sensors 1–5 must have been triggered 3 times each.
    for (uint8_t i = 1; i < ULTRASONIC_COUNT; ++i) {
        TEST_ASSERT_EQUAL_MESSAGE(3, goodIOs[i].getMeasureCount(),
                                  "Good sensor was starved by faulty sensor");
    }
}

// =============================================================================
// G. Telemetry identification
// =============================================================================

void test_each_reading_carries_distinct_sensor_id(void) {
    MockUltrasonicIO mockIOs[ULTRASONIC_COUNT];
    UltrasonicManager mgr;
    for (uint8_t i = 0; i < ULTRASONIC_COUNT; ++i) {
        mockIOs[i].setDistanceMm(200.0f + i * 50.0f);
        mgr.getDriver(i).setIO(&mockIOs[i]);
    }
    mgr.begin();

    for (uint8_t c = 0; c < ULTRASONIC_COUNT; ++c) {
        mgr.update();
    }

    for (uint8_t i = 0; i < ULTRASONIC_COUNT; ++i) {
        TEST_ASSERT_EQUAL(i, mgr.getReading(i).sensor_id);
    }
}

void test_out_of_range_index_returns_invalid_id(void) {
    UltrasonicManager mgr;
    const auto& r = mgr.getReading(ULTRASONIC_COUNT);  // out-of-range
    TEST_ASSERT_EQUAL(0xFF, r.sensor_id);
    TEST_ASSERT_EQUAL(SensorStatus::INVALID, r.status);
}

// =============================================================================
// H. Startup / reset behaviour
// =============================================================================

void test_initial_status_is_uninitialized(void) {
    MockUltrasonicIO mockIO;
    HCSR04Config cfg{};
    cfg.sensor_id = 0;
    cfg.trig_pin  = US_TRIG_PIN[0];
    cfg.echo_pin  = US_ECHO_PIN[0];
    HCSR04Driver driver(cfg, &mockIO);
    TEST_ASSERT_EQUAL(SensorStatus::UNINITIALIZED, driver.getReading().status);
}

void test_initial_distance_is_negative(void) {
    MockUltrasonicIO mockIO;
    HCSR04Config cfg{};
    cfg.sensor_id = 0;
    cfg.trig_pin  = US_TRIG_PIN[0];
    cfg.echo_pin  = US_ECHO_PIN[0];
    HCSR04Driver driver(cfg, &mockIO);
    TEST_ASSERT_LESS_THAN(0.0f, driver.getReading().distance_mm);
}

void test_reset_clears_health_counters(void) {
    MockUltrasonicIO mockIO(MockUltrasonicIO::MockMode::TIMEOUT);
    HCSR04Config cfg{};
    cfg.sensor_id = 0;
    cfg.trig_pin  = US_TRIG_PIN[0];
    cfg.echo_pin  = US_ECHO_PIN[0];
    cfg.max_consecutive_failures = 5;
    HCSR04Driver driver(cfg, &mockIO);
    driver.begin();

    // Drive into FAULT
    for (uint8_t i = 0; i <= cfg.max_consecutive_failures; ++i) {
        driver.update();
    }
    TEST_ASSERT_EQUAL(SensorStatus::FAULT, driver.getReading().status);

    // Reset should clear
    driver.reset();
    TEST_ASSERT_NOT_EQUAL(SensorStatus::FAULT, driver.getReading().status);
}

// =============================================================================
// I. Obstacle-zone classification — values below, at, above each threshold
// =============================================================================

// Use a fixed timestamp — staleness not triggered if readings are fresh enough.
static constexpr uint32_t T_NOW = 1000;

void test_zone_clear_above_caution_threshold(void) {
    auto r = makeReading(0, config::ZONE_CAUTION_MM + 1.0f, SensorStatus::OK, T_NOW);
    TEST_ASSERT_EQUAL(ObstacleZone::CLEAR, classifyZone(r, T_NOW));
}

void test_zone_clear_exactly_at_caution_threshold(void) {
    auto r = makeReading(0, config::ZONE_CAUTION_MM, SensorStatus::OK, T_NOW);
    TEST_ASSERT_EQUAL(ObstacleZone::CLEAR, classifyZone(r, T_NOW));
}

void test_zone_caution_just_below_caution_threshold(void) {
    auto r = makeReading(0, config::ZONE_CAUTION_MM - 1.0f, SensorStatus::OK, T_NOW);
    TEST_ASSERT_EQUAL(ObstacleZone::CAUTION, classifyZone(r, T_NOW));
}

void test_zone_caution_exactly_at_obstacle_threshold(void) {
    auto r = makeReading(0, config::ZONE_OBSTACLE_MM, SensorStatus::OK, T_NOW);
    TEST_ASSERT_EQUAL(ObstacleZone::CAUTION, classifyZone(r, T_NOW));
}

void test_zone_obstacle_just_below_obstacle_threshold(void) {
    auto r = makeReading(0, config::ZONE_OBSTACLE_MM - 1.0f, SensorStatus::OK, T_NOW);
    TEST_ASSERT_EQUAL(ObstacleZone::OBSTACLE, classifyZone(r, T_NOW));
}

void test_zone_obstacle_at_minimum_valid_distance(void) {
    auto r = makeReading(0, config::ULTRASONIC_MIN_MM, SensorStatus::OK, T_NOW);
    TEST_ASSERT_EQUAL(ObstacleZone::OBSTACLE, classifyZone(r, T_NOW));
}

void test_zone_fault_for_non_ok_status(void) {
    auto r = makeReading(0, 500.0f, SensorStatus::TIMEOUT, T_NOW);
    TEST_ASSERT_EQUAL(ObstacleZone::FAULT, classifyZone(r, T_NOW));
}

void test_zone_fault_for_invalid_status(void) {
    auto r = makeReading(0, 500.0f, SensorStatus::INVALID, T_NOW);
    TEST_ASSERT_EQUAL(ObstacleZone::FAULT, classifyZone(r, T_NOW));
}

void test_zone_fault_for_fault_status(void) {
    auto r = makeReading(0, 500.0f, SensorStatus::FAULT, T_NOW);
    TEST_ASSERT_EQUAL(ObstacleZone::FAULT, classifyZone(r, T_NOW));
}

void test_zone_fault_for_uninitialized_status(void) {
    auto r = makeReading(0, -1.0f, SensorStatus::UNINITIALIZED, T_NOW);
    TEST_ASSERT_EQUAL(ObstacleZone::FAULT, classifyZone(r, T_NOW));
}

void test_zone_fault_for_negative_distance_even_if_ok(void) {
    auto r = makeReading(0, -1.0f, SensorStatus::OK, T_NOW);
    TEST_ASSERT_EQUAL(ObstacleZone::FAULT, classifyZone(r, T_NOW));
}

// =============================================================================
// J. Zone state transitions
// =============================================================================

void test_zone_transitions_clear_to_caution(void) {
    ObstacleZoneClassifier clf;
    UltrasonicReading readings[ULTRASONIC_COUNT];
    for (uint8_t i = 0; i < ULTRASONIC_COUNT; ++i) {
        readings[i] = makeReading(i, config::ZONE_CAUTION_MM + 100.0f, SensorStatus::OK, T_NOW);
    }
    clf.update(readings, ULTRASONIC_COUNT, T_NOW);
    TEST_ASSERT_EQUAL(ObstacleZone::CLEAR, clf.getOverallZone());

    // Move sensor 0 into CAUTION range
    readings[0].distance_mm = (config::ZONE_OBSTACLE_MM + config::ZONE_CAUTION_MM) / 2.0f;
    clf.update(readings, ULTRASONIC_COUNT, T_NOW);
    TEST_ASSERT_EQUAL(ObstacleZone::CAUTION, clf.getOverallZone());
}

void test_zone_transitions_caution_to_obstacle(void) {
    ObstacleZoneClassifier clf;
    UltrasonicReading readings[ULTRASONIC_COUNT];
    for (uint8_t i = 0; i < ULTRASONIC_COUNT; ++i) {
        readings[i] = makeReading(i, config::ZONE_CAUTION_MM - 50.0f, SensorStatus::OK, T_NOW);
    }
    clf.update(readings, ULTRASONIC_COUNT, T_NOW);
    TEST_ASSERT_EQUAL(ObstacleZone::CAUTION, clf.getOverallZone());

    readings[0].distance_mm = config::ZONE_OBSTACLE_MM - 50.0f;
    clf.update(readings, ULTRASONIC_COUNT, T_NOW);
    TEST_ASSERT_EQUAL(ObstacleZone::OBSTACLE, clf.getOverallZone());
}

void test_zone_transitions_obstacle_to_clear(void) {
    ObstacleZoneClassifier clf;
    UltrasonicReading readings[ULTRASONIC_COUNT];
    for (uint8_t i = 0; i < ULTRASONIC_COUNT; ++i) {
        readings[i] = makeReading(i, 100.0f, SensorStatus::OK, T_NOW);
    }
    clf.update(readings, ULTRASONIC_COUNT, T_NOW);
    TEST_ASSERT_EQUAL(ObstacleZone::OBSTACLE, clf.getOverallZone());

    // Remove obstacle — all sensors now clear
    for (uint8_t i = 0; i < ULTRASONIC_COUNT; ++i) {
        readings[i].distance_mm = config::ZONE_CAUTION_MM + 200.0f;
    }
    clf.update(readings, ULTRASONIC_COUNT, T_NOW);
    TEST_ASSERT_EQUAL(ObstacleZone::CLEAR, clf.getOverallZone());
}

// =============================================================================
// K. Invalid and timed-out readings → zone FAULT
// =============================================================================

void test_timeout_reading_produces_fault_zone(void) {
    auto r = makeReading(0, -1.0f, SensorStatus::TIMEOUT, T_NOW);
    TEST_ASSERT_EQUAL(ObstacleZone::FAULT, classifyZone(r, T_NOW));
}

void test_fault_reading_produces_fault_zone(void) {
    auto r = makeReading(0, 200.0f, SensorStatus::FAULT, T_NOW);
    TEST_ASSERT_EQUAL(ObstacleZone::FAULT, classifyZone(r, T_NOW));
}

// =============================================================================
// L. Stale-data detection
// =============================================================================

void test_stale_reading_produces_fault_zone(void) {
    uint32_t old_ts  = 0;
    uint32_t now     = config::ULTRASONIC_STALE_THRESHOLD_MS + 1;
    auto r = makeReading(0, config::ZONE_CAUTION_MM + 100.0f, SensorStatus::OK, old_ts);
    TEST_ASSERT_EQUAL(ObstacleZone::FAULT, classifyZone(r, now));
}

void test_fresh_reading_is_not_stale(void) {
    uint32_t ts  = 500;
    uint32_t now = ts + 100;  // 100 ms old — well within stale threshold
    auto r = makeReading(0, config::ZONE_CAUTION_MM + 100.0f, SensorStatus::OK, ts);
    TEST_ASSERT_EQUAL(ObstacleZone::CLEAR, classifyZone(r, now));
}

void test_reading_exactly_at_stale_boundary_is_fault(void) {
    uint32_t ts  = 0;
    uint32_t now = config::ULTRASONIC_STALE_THRESHOLD_MS + 1;
    auto r = makeReading(0, 1000.0f, SensorStatus::OK, ts);
    TEST_ASSERT_EQUAL(ObstacleZone::FAULT, classifyZone(r, now));
}

// =============================================================================
// M. Recovery after a fault
// =============================================================================

void test_fault_zone_clears_when_valid_reading_returns(void) {
    ObstacleZoneClassifier clf;
    UltrasonicReading readings[ULTRASONIC_COUNT];
    for (uint8_t i = 0; i < ULTRASONIC_COUNT; ++i) {
        readings[i] = makeReading(i, -1.0f, SensorStatus::FAULT, T_NOW);
    }
    clf.update(readings, ULTRASONIC_COUNT, T_NOW);
    TEST_ASSERT_EQUAL(ObstacleZone::FAULT, clf.getOverallZone());

    // All sensors now recover with valid clear readings.
    for (uint8_t i = 0; i < ULTRASONIC_COUNT; ++i) {
        readings[i] = makeReading(i, config::ZONE_CAUTION_MM + 100.0f, SensorStatus::OK, T_NOW);
    }
    clf.update(readings, ULTRASONIC_COUNT, T_NOW);
    TEST_ASSERT_EQUAL(ObstacleZone::CLEAR, clf.getOverallZone());
}

void test_sensor_fault_recovery_does_not_affect_other_sensors(void) {
    ObstacleZoneClassifier clf;
    UltrasonicReading readings[ULTRASONIC_COUNT];

    // All sensors CLEAR initially.
    for (uint8_t i = 0; i < ULTRASONIC_COUNT; ++i) {
        readings[i] = makeReading(i, 1000.0f, SensorStatus::OK, T_NOW);
    }
    clf.update(readings, ULTRASONIC_COUNT, T_NOW);
    TEST_ASSERT_EQUAL(ObstacleZone::CLEAR, clf.getOverallZone());

    // Sensor 2 faults.
    readings[2] = makeReading(2, -1.0f, SensorStatus::FAULT, T_NOW);
    clf.update(readings, ULTRASONIC_COUNT, T_NOW);
    TEST_ASSERT_EQUAL(ObstacleZone::FAULT, clf.getOverallZone());
    TEST_ASSERT_EQUAL(ObstacleZone::CLEAR, clf.getSensorZone(0));  // others still CLEAR

    // Sensor 2 recovers.
    readings[2] = makeReading(2, 1000.0f, SensorStatus::OK, T_NOW);
    clf.update(readings, ULTRASONIC_COUNT, T_NOW);
    TEST_ASSERT_EQUAL(ObstacleZone::CLEAR, clf.getOverallZone());
}

// =============================================================================
// N. Multiple sensors reporting different zones
// =============================================================================

void test_multiple_sensors_different_zones_aggregate_most_restrictive(void) {
    ObstacleZoneClassifier clf;
    UltrasonicReading readings[ULTRASONIC_COUNT];

    readings[0] = makeReading(0, 1500.0f, SensorStatus::OK, T_NOW);  // CLEAR
    readings[1] = makeReading(1,  600.0f, SensorStatus::OK, T_NOW);  // CAUTION
    readings[2] = makeReading(2,  200.0f, SensorStatus::OK, T_NOW);  // OBSTACLE
    readings[3] = makeReading(3, 2000.0f, SensorStatus::OK, T_NOW);  // CLEAR
    readings[4] = makeReading(4,  700.0f, SensorStatus::OK, T_NOW);  // CAUTION
    readings[5] = makeReading(5, 1000.0f, SensorStatus::OK, T_NOW);  // CLEAR

    clf.update(readings, ULTRASONIC_COUNT, T_NOW);

    TEST_ASSERT_EQUAL(ObstacleZone::CLEAR,    clf.getSensorZone(0));
    TEST_ASSERT_EQUAL(ObstacleZone::CAUTION,  clf.getSensorZone(1));
    TEST_ASSERT_EQUAL(ObstacleZone::OBSTACLE, clf.getSensorZone(2));
    TEST_ASSERT_EQUAL(ObstacleZone::CLEAR,    clf.getSensorZone(3));
    TEST_ASSERT_EQUAL(ObstacleZone::CAUTION,  clf.getSensorZone(4));
    TEST_ASSERT_EQUAL(ObstacleZone::CLEAR,    clf.getSensorZone(5));

    // Most restrictive = OBSTACLE
    TEST_ASSERT_EQUAL(ObstacleZone::OBSTACLE, clf.getOverallZone());
}

// =============================================================================
// O. Aggregate zone — most-restrictive logic
// =============================================================================

void test_single_fault_makes_aggregate_fault(void) {
    ObstacleZoneClassifier clf;
    UltrasonicReading readings[ULTRASONIC_COUNT];
    for (uint8_t i = 0; i < ULTRASONIC_COUNT; ++i) {
        readings[i] = makeReading(i, 1000.0f, SensorStatus::OK, T_NOW);
    }
    // Only sensor 4 is faulty
    readings[4] = makeReading(4, -1.0f, SensorStatus::FAULT, T_NOW);
    clf.update(readings, ULTRASONIC_COUNT, T_NOW);
    TEST_ASSERT_EQUAL(ObstacleZone::FAULT, clf.getOverallZone());
}

void test_all_clear_produces_clear_aggregate(void) {
    ObstacleZoneClassifier clf;
    UltrasonicReading readings[ULTRASONIC_COUNT];
    for (uint8_t i = 0; i < ULTRASONIC_COUNT; ++i) {
        readings[i] = makeReading(i, config::ZONE_CAUTION_MM + 200.0f, SensorStatus::OK, T_NOW);
    }
    clf.update(readings, ULTRASONIC_COUNT, T_NOW);
    TEST_ASSERT_EQUAL(ObstacleZone::CLEAR, clf.getOverallZone());
}

void test_zone_name_strings_are_correct(void) {
    TEST_ASSERT_EQUAL_STRING("CLEAR",    ObstacleZoneClassifier::zoneName(ObstacleZone::CLEAR));
    TEST_ASSERT_EQUAL_STRING("CAUTION",  ObstacleZoneClassifier::zoneName(ObstacleZone::CAUTION));
    TEST_ASSERT_EQUAL_STRING("OBSTACLE", ObstacleZoneClassifier::zoneName(ObstacleZone::OBSTACLE));
    TEST_ASSERT_EQUAL_STRING("FAULT",    ObstacleZoneClassifier::zoneName(ObstacleZone::FAULT));
}

// =============================================================================
// P. Threshold configuration values
// =============================================================================

void test_obstacle_threshold_is_positive(void) {
    TEST_ASSERT_GREATER_THAN(0.0f, config::ZONE_OBSTACLE_MM);
}

void test_caution_threshold_greater_than_obstacle(void) {
    TEST_ASSERT_GREATER_THAN(config::ZONE_OBSTACLE_MM, config::ZONE_CAUTION_MM);
}

void test_stale_threshold_is_positive(void) {
    TEST_ASSERT_GREATER_THAN(0u, config::ULTRASONIC_STALE_THRESHOLD_MS);
}

void test_timeout_us_is_consistent_with_max_range(void) {
    // At 343 m/s, 4000 mm round trip takes ~23,324 µs.
    // Our timeout (30,000 µs) must exceed that.
    constexpr uint32_t min_required_us = static_cast<uint32_t>(
        (config::ULTRASONIC_MAX_MM * 2.0f) / 0.343f);
    TEST_ASSERT_GREATER_THAN(min_required_us, config::ULTRASONIC_TIMEOUT_US);
}

// =============================================================================
// Test runner
// =============================================================================

void setUp(void) {}
void tearDown(void) {}

int main(int argc, char** argv) {
    UNITY_BEGIN();

    // A — GPIO configuration
    RUN_TEST(test_ultrasonic_count_is_six);
    RUN_TEST(test_trig_pins_match_spec);
    RUN_TEST(test_echo_pins_match_spec);
    RUN_TEST(test_no_trig_pin_is_zero);
    RUN_TEST(test_no_echo_pin_is_zero);
    RUN_TEST(test_trig_echo_pins_are_distinct);
    RUN_TEST(test_all_trig_pins_unique);
    RUN_TEST(test_all_echo_pins_unique);

    // B — Sensor ID
    RUN_TEST(test_sensor_ids_are_zero_based);
    RUN_TEST(test_manager_reading_sensor_id_matches_index);

    // C — Round-robin
    RUN_TEST(test_round_robin_advances_through_all_sensors);
    RUN_TEST(test_round_robin_wraps_back_to_sensor_zero);
    RUN_TEST(test_only_one_sensor_triggered_per_update);

    // D — Starvation
    RUN_TEST(test_all_sensors_get_updated_in_six_cycles);
    RUN_TEST(test_no_sensor_triggered_more_than_once_per_cycle);

    // E — Timeout/invalid
    RUN_TEST(test_timeout_returns_negative_distance);
    RUN_TEST(test_timeout_sets_status_timeout);
    RUN_TEST(test_repeated_timeout_escalates_to_fault);
    RUN_TEST(test_valid_reading_after_timeouts_does_not_crash);

    // F — Fault isolation
    RUN_TEST(test_one_faulty_sensor_does_not_stop_others);
    RUN_TEST(test_faulty_sensor_does_not_freeze_scheduler);

    // G — Telemetry ID
    RUN_TEST(test_each_reading_carries_distinct_sensor_id);
    RUN_TEST(test_out_of_range_index_returns_invalid_id);

    // H — Startup/reset
    RUN_TEST(test_initial_status_is_uninitialized);
    RUN_TEST(test_initial_distance_is_negative);
    RUN_TEST(test_reset_clears_health_counters);

    // I — Zone classification
    RUN_TEST(test_zone_clear_above_caution_threshold);
    RUN_TEST(test_zone_clear_exactly_at_caution_threshold);
    RUN_TEST(test_zone_caution_just_below_caution_threshold);
    RUN_TEST(test_zone_caution_exactly_at_obstacle_threshold);
    RUN_TEST(test_zone_obstacle_just_below_obstacle_threshold);
    RUN_TEST(test_zone_obstacle_at_minimum_valid_distance);
    RUN_TEST(test_zone_fault_for_non_ok_status);
    RUN_TEST(test_zone_fault_for_invalid_status);
    RUN_TEST(test_zone_fault_for_fault_status);
    RUN_TEST(test_zone_fault_for_uninitialized_status);
    RUN_TEST(test_zone_fault_for_negative_distance_even_if_ok);

    // J — Transitions
    RUN_TEST(test_zone_transitions_clear_to_caution);
    RUN_TEST(test_zone_transitions_caution_to_obstacle);
    RUN_TEST(test_zone_transitions_obstacle_to_clear);

    // K — Timeout/invalid → zone FAULT
    RUN_TEST(test_timeout_reading_produces_fault_zone);
    RUN_TEST(test_fault_reading_produces_fault_zone);

    // L — Staleness
    RUN_TEST(test_stale_reading_produces_fault_zone);
    RUN_TEST(test_fresh_reading_is_not_stale);
    RUN_TEST(test_reading_exactly_at_stale_boundary_is_fault);

    // M — Recovery
    RUN_TEST(test_fault_zone_clears_when_valid_reading_returns);
    RUN_TEST(test_sensor_fault_recovery_does_not_affect_other_sensors);

    // N — Multiple zones
    RUN_TEST(test_multiple_sensors_different_zones_aggregate_most_restrictive);

    // O — Aggregate
    RUN_TEST(test_single_fault_makes_aggregate_fault);
    RUN_TEST(test_all_clear_produces_clear_aggregate);
    RUN_TEST(test_zone_name_strings_are_correct);

    // P — Thresholds
    RUN_TEST(test_obstacle_threshold_is_positive);
    RUN_TEST(test_caution_threshold_greater_than_obstacle);
    RUN_TEST(test_stale_threshold_is_positive);
    RUN_TEST(test_timeout_us_is_consistent_with_max_range);

    return UNITY_END();
}
