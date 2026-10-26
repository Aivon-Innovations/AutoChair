/**
 * @file autochair_config.h
 * @brief AutoChair ESP32 — Compile-time and runtime configuration constants
 *
 * Project:  Aivon Innovations — AutoChair
 * Platform: ESP32-WROOM-32D
 * Status:   IN DEVELOPMENT
 *
 * All timing values, pin assignments, and tunable parameters are gathered
 * here so they can be changed without hunting through driver files.
 *
 * GPIO pin assignments verified against:
 *   - Bench Test/Ultrasonic sensor validation/firmware/src/main.cpp
 *   - ULTRASONIC_PHASE_VALIDATION_REPORT.md
 *   - ULTRASONIC_SENSOR_TESTING_REPORT.md (Sensors #1–#6)
 *   - Physical bench wiring confirmed 2026-10-22
 *
 * OBSTACLE-ZONE THRESHOLDS:
 *   Labelled UNVALIDATED PROVISIONAL ENGINEERING PARAMETERS.
 *   Not calibrated for wheelchair stopping distance.
 *   Must be validated experimentally before any safety claim.
 */

#pragma once

#include <cstdint>
#include <cstddef>

namespace autochair {
namespace config {

// =============================================================================
// Firmware identity
// =============================================================================

constexpr const char* DEVICE_ID       = "AUTOCHAIR_ESP32";
constexpr const char* DEVICE_VERSION  = "0.1.0";

// =============================================================================
// Heartbeat  (SYSTEM_STATE_MACHINE.md §9 / SAFETY_ARCHITECTURE.md §11)
//
// NOTE: Timeout value must be validated experimentally.
// Do not treat the initial value as a final safety parameter.
// =============================================================================

constexpr uint32_t HEARTBEAT_INTERVAL_MS        = 1000;   ///< Pi transmit interval.
constexpr uint32_t HEARTBEAT_TIMEOUT_MS         = 5000;   ///< Must be validated.
constexpr uint32_t HEARTBEAT_RECOVERY_WINDOW_MS = 2000;   ///< Grace period.

// =============================================================================
// Watchdog  (ESP32_CONTROLLER_PRD.md §22)
// =============================================================================

constexpr uint32_t WATCHDOG_TIMEOUT_MS = 8000;  ///< ESP32 hardware WDT timeout.

// =============================================================================
// Communication  (ESP32_PI_PROTOCOL.md §39)
// =============================================================================

constexpr uint32_t SERIAL_BAUD_RATE   = 115200;
constexpr size_t   MAX_MESSAGE_SIZE   = 512;     ///< Bytes; reject larger messages.

// =============================================================================
// Ultrasonic sensors  (SENSOR_INTERFACE.md §7)
//
// Six-sensor GPIO mapping — verified 2026-10-22:
//
//   Sensor | ID (0-based) | TRIG   | ECHO
//   -------|--------------|--------|------
//   #1     |     0        | GPIO4  | GPIO34
//   #2     |     1        | GPIO5  | GPIO35
//   #3     |     2        | GPIO18 | GPIO36
//   #4     |     3        | GPIO19 | GPIO39
//   #5     |     4        | GPIO23 | GPIO32
//   #6     |     5        | GPIO13 | GPIO33
//
// GPIO34-39: Input-only pads, no internal pull-up/pull-down.
// ECHO boot-time risk: resistive divider must NOT be connected before
//   ESP32 boot completes. See ULTRASONIC_PHASE_VALIDATION_REPORT.md §4.
//
// Each ECHO line uses an independent 4.7 kΩ + 4.7 kΩ resistor divider:
//   Sensor ECHO (5V) → R1(4.7kΩ) → junction → ESP32 GPIO
//                                → R2(4.7kΩ) → GND
//   V_junction = 5.0 × 0.5 = 2.5V  (safe for 3.3V ESP32 GPIO)
// =============================================================================

constexpr uint8_t ULTRASONIC_COUNT = 6;

/// Trigger pins — one per sensor (OUTPUT, driven by ESP32).
constexpr uint8_t US_TRIG_PIN[ULTRASONIC_COUNT] = {
     4,   ///< Sensor #1 (ID 0)
     5,   ///< Sensor #2 (ID 1)
    18,   ///< Sensor #3 (ID 2)
    19,   ///< Sensor #4 (ID 3)
    23,   ///< Sensor #5 (ID 4)
    13    ///< Sensor #6 (ID 5)
};

/// Echo pins — one per sensor (INPUT, 5V→3.3V divided, input-only where applicable).
constexpr uint8_t US_ECHO_PIN[ULTRASONIC_COUNT] = {
    34,   ///< Sensor #1 (ID 0) — input-only, no pull
    35,   ///< Sensor #2 (ID 1) — input-only, no pull
    36,   ///< Sensor #3 (ID 2) — input-only, no pull
    39,   ///< Sensor #4 (ID 3) — input-only, no pull
    32,   ///< Sensor #5 (ID 4)
    33    ///< Sensor #6 (ID 5)
};

/// Minimum valid measurement distance (hardware blind zone).
constexpr float ULTRASONIC_MIN_MM = 20.0f;

/// Maximum valid measurement distance.
constexpr float ULTRASONIC_MAX_MM = 4000.0f;

/// Pulse timeout in microseconds (~5.1 m max range).
/// pulseIn() returns 0 if echo does not arrive within this window.
constexpr uint32_t ULTRASONIC_TIMEOUT_US = 30000;

/// Inter-sensor delay in milliseconds inserted between consecutive sensor
/// triggers during round-robin acquisition.
///
/// Rationale:
///   HC-SR04 acoustic burst: 8 × 40 kHz pulses = 200 µs active.
///   Speed of sound: 343 m/s. At 4 m max range, round-trip = ~23 ms.
///   At 50 ms main-loop cadence, one sensor is triggered per cycle.
///   Effective inter-trigger gap = (ULTRASONIC_CYCLE_INTERVAL_MS × NUM_SENSORS)
///   = 50 ms × 6 = 300 ms between any two triggers on the same sensor.
///   This exceeds the 30 ms timeout window; crosstalk between same-frequency
///   sensors is reduced but not eliminated — requires physical verification.
///
/// TIMING TRADE-OFF NOTE:
///   A shorter cycle reduces latency but increases acoustic crosstalk risk.
///   A longer cycle reduces crosstalk but increases sensor update staleness.
///   50 ms / sensor is a conservative starting point — validate experimentally.
constexpr uint32_t ULTRASONIC_CYCLE_INTERVAL_MS = 50;

/// Consecutive failure count before a sensor is escalated to FAULT status.
constexpr uint16_t ULTRASONIC_MAX_FAILURES = 5;

/// Age in milliseconds beyond which a reading is considered stale.
/// If a sensor's last-update timestamp is older than this, the reading
/// should not be used for zone classification.
constexpr uint32_t ULTRASONIC_STALE_THRESHOLD_MS = 1000;

// =============================================================================
// Obstacle-zone thresholds  (SENSOR_INTERFACE.md §7 / SAFETY_ARCHITECTURE.md)
//
// STATUS: UNVALIDATED PROVISIONAL ENGINEERING PARAMETERS
//
// These values have NO relationship to wheelchair stopping distance,
// braking performance, or any physical safety characterisation.
// They are bench-test classification parameters only.
// Must be validated experimentally before any safety use.
//
// Zone mapping:
//   OBSTACLE : distance < ZONE_OBSTACLE_MM         (< 400 mm)
//   CAUTION  : ZONE_OBSTACLE_MM ≤ d < ZONE_CAUTION_MM  (400–800 mm)
//   CLEAR    : distance ≥ ZONE_CAUTION_MM          (≥ 800 mm)
//   FAULT    : invalid / timed-out / stale reading
// =============================================================================

constexpr float ZONE_OBSTACLE_MM = 400.0f;   ///< UNVALIDATED — bench parameter only.
constexpr float ZONE_CAUTION_MM  = 800.0f;   ///< UNVALIDATED — bench parameter only.

// =============================================================================
// IMU (MPU6050 / MPU6500)
// =============================================================================

constexpr uint8_t  IMU_DEFAULT_I2C_ADDRESS = 0x68;
constexpr uint32_t IMU_ACQUIRE_INTERVAL_MS = 20;

// =============================================================================
// Encoder
// =============================================================================

constexpr uint32_t ENCODER_ACQUIRE_INTERVAL_MS = 5;

// =============================================================================
// Safety monitor
// =============================================================================

constexpr uint32_t SAFETY_MONITOR_INTERVAL_MS = 10;

// =============================================================================
// Telemetry / Diagnostics
// =============================================================================

constexpr uint32_t TELEMETRY_INTERVAL_MS    = 100;
constexpr uint32_t DIAGNOSTICS_INTERVAL_MS  = 500;

}  // namespace config
}  // namespace autochair
