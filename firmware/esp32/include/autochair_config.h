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
 * IMPORTANT — Hardware pin assignments are marked PLACEHOLDER.
 * Actual GPIO numbers must be confirmed from verified wiring before use.
 *
 * IMPORTANT — Timing values are initial estimates only.
 * They must be validated experimentally as required by the documentation.
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
// GPIO pin numbers below are PLACEHOLDERS.
// Must be confirmed against actual wiring before connecting hardware.
//
// SAFETY NOTE: HC-SR04 ECHO is 5 V logic; level-shifting is required
// before connecting to ESP32 GPIO (3.3 V tolerant).
// =============================================================================

constexpr uint8_t ULTRASONIC_COUNT         = 6;   ///< Active sensors.
constexpr uint8_t ULTRASONIC_SPARE_COUNT   = 2;
constexpr uint32_t ULTRASONIC_TIMEOUT_US   = 30000; ///< ~5 m max range.
constexpr float    ULTRASONIC_MIN_MM       = 20.0f;
constexpr float    ULTRASONIC_MAX_MM       = 4000.0f;
constexpr uint32_t ULTRASONIC_INTERVAL_MS  = 50;  ///< Per-sensor cycle; validate on bench.

// Proposed 6-Channel Mapping (TARGET / Integration Configuration - Physical wiring validation pending):
// Sensor 0 (Physical Sensor #1): TRIG = GPIO4,  ECHO = GPIO34 (CONFIRMED)
// Sensor 1 (Physical Sensor #2): TRIG = GPIO5,  ECHO = GPIO35 (TARGET)
// Sensor 2 (Physical Sensor #3): TRIG = GPIO18, ECHO = GPIO36 (TARGET)
// Sensor 3 (Physical Sensor #4): TRIG = GPIO19, ECHO = GPIO39 (TARGET)
// Sensor 4 (Physical Sensor #5): TRIG = GPIO23, ECHO = GPIO32 (TARGET)
// Sensor 5 (Physical Sensor #6): TRIG = GPIO13, ECHO = GPIO33 (TARGET)
constexpr uint8_t US_TRIG_PIN[6]  = { 4,  5, 18, 19, 23, 13 };
constexpr uint8_t US_ECHO_PIN[6]  = { 34, 35, 36, 39, 32, 33 };

// =============================================================================
// IMU  (IMU_INTERFACE.md)
//
// Bus address and wiring must be confirmed for the selected IMU board.
// Do not assume I2C address from sensor model alone.
// =============================================================================

constexpr uint8_t  IMU_DEFAULT_I2C_ADDRESS = 0x68; ///< Common default — VERIFY.
constexpr uint32_t IMU_SAMPLE_INTERVAL_MS  = 20;   ///< 50 Hz initial; validate.

// =============================================================================
// Encoders  (ENCODER_INTERFACE.md)
//
// GPIO pin numbers are PLACEHOLDERS.
// Electrical compatibility (voltage, output type) must be verified
// before connecting encoder signals to ESP32 GPIOs.
//
// PPR = 600. Quadrature decoding factor must be established experimentally.
// Do NOT automatically assume 2400 counts/revolution.
// =============================================================================

constexpr uint16_t ENCODER_PPR = 600;  ///< Pulses per revolution (manufacturer spec).
// counts/revolution may be different depending on decoding mode — verify.

// Temporary GPIO assignment for Physical Bench Test (Phase 6):
// Left Encoder: A = GPIO25, B = GPIO26, Z = GPIO27
// Right Encoder: Disabled (0)
constexpr uint8_t ENCODER_LEFT_A_PIN  = 25;
constexpr uint8_t ENCODER_LEFT_B_PIN  = 26;
constexpr uint8_t ENCODER_LEFT_Z_PIN  = 27;
constexpr uint8_t ENCODER_RIGHT_A_PIN = 0;  // DISABLED / UNCONFIGURED
constexpr uint8_t ENCODER_RIGHT_B_PIN = 0;  // DISABLED / UNCONFIGURED
constexpr uint8_t ENCODER_RIGHT_Z_PIN = 0;  // DISABLED / UNCONFIGURED

// =============================================================================
// Emergency stop  (ESP32_CONTROLLER_PRD.md §12)
//
// GPIO is PLACEHOLDER. Physical E-stop circuit must be independently verified.
// Software monitoring of this input does NOT substitute for a verified
// physical emergency-stop circuit.
// =============================================================================

constexpr uint8_t ESTOP_GPIO_PIN     = 0;    // PLACEHOLDER
constexpr bool    ESTOP_ACTIVE_LOW   = true; // ASSUMPTION — verify from hardware

// =============================================================================
// Diagnostics indicators
// GPIO assignments are PLACEHOLDER — assign after physical wiring.
// =============================================================================

constexpr uint8_t LED_STATUS_PIN    = 2;  // PLACEHOLDER (onboard LED on many DevKits)
constexpr uint8_t BUZZER_PIN        = 0;  // PLACEHOLDER

// =============================================================================
// Wheelchair interface  (WHEELCHAIR_INTERFACE.md §23)
//
// Default mode for V1 is SIMULATION.
// PHYSICAL must never be selected until the controller interface is verified.
// =============================================================================

constexpr bool WHEELCHAIR_INTERFACE_PHYSICAL_ENABLED = false; ///< V1: always false.

// =============================================================================
// Self-test options
// =============================================================================

constexpr bool SELF_TEST_REQUIRE_ULTRASONIC = false; ///< Optional in V1 — no hardware yet.
constexpr bool SELF_TEST_REQUIRE_IMU        = false; ///< Optional in V1.
constexpr bool SELF_TEST_REQUIRE_ENCODER    = false; ///< Optional in V1.
constexpr bool SELF_TEST_REQUIRE_COMMS      = false; ///< Optional in V1.

// =============================================================================
// Scheduling intervals (approximate; validated during implementation)
// =============================================================================

constexpr uint32_t SAFETY_MONITOR_INTERVAL_MS   = 10;   ///< High-frequency.
constexpr uint32_t ENCODER_ACQUIRE_INTERVAL_MS  = 5;    ///< High-frequency.
constexpr uint32_t IMU_ACQUIRE_INTERVAL_MS       = 20;   ///< Medium.
constexpr uint32_t ULTRASONIC_CYCLE_INTERVAL_MS  = 50;   ///< Medium.
constexpr uint32_t TELEMETRY_INTERVAL_MS         = 100;  ///< Lower-frequency.
constexpr uint32_t DIAGNOSTICS_INTERVAL_MS       = 500;

}  // namespace config
}  // namespace autochair
