/**
 * @file main.cpp
 * @brief AutoChair ESP32 — Application entry point
 *
 * Project:  Aivon Innovations — AutoChair
 * Platform: ESP32-WROOM-32D
 * Status:   IN DEVELOPMENT — Phase 0/1 scaffold
 *
 * This is the top-level application file for the AutoChair ESP32 firmware.
 *
 * Architecture:
 *   setup()  — one-time initialization, runs BOOT → INITIALIZING → SELF_TEST
 *   loop()   — cooperative task dispatch, runs continuously
 *
 * Module initialization order  (SENSOR_INTERFACE.md §28):
 *   1. Logger
 *   2. StateMachine
 *   3. SafetyManager
 *   4. Sensor stubs (real drivers added in Phases 4–6)
 *   5. WheelchairInterface (simulation only in V1)
 *   6. CommandHandler
 *   7. Self-test
 *
 * Task schedule:
 *   - Safety monitor:     10 ms cadence (SAFETY_MONITOR_INTERVAL_MS)
 *   - Heartbeat monitor:  checked within safety update
 *   - Encoder acquire:    5 ms cadence  (ENCODER_ACQUIRE_INTERVAL_MS)
 *   - IMU acquire:        20 ms cadence (IMU_ACQUIRE_INTERVAL_MS)
 *   - Ultrasonic cycle:   50 ms cadence (ULTRASONIC_CYCLE_INTERVAL_MS)
 *   - Telemetry dispatch: 100 ms cadence (TELEMETRY_INTERVAL_MS)
 *   - Diagnostics:        500 ms cadence (DIAGNOSTICS_INTERVAL_MS)
 *
 * Safety boundary:
 *   All physical hardware interfaces (wheelchair, motors, brakes) are
 *   NOT connected in V1. Only sensor stubs and simulation are active.
 */

#include "autochair_types.h"
#include "autochair_config.h"

#include "diagnostics/logger.h"
#include "core/state_machine.h"
#include "safety/safety_manager.h"
#include "sensors/ultrasonic/hcsr04_driver.h"
#include "sensors/ultrasonic/ultrasonic_manager.h"
#include "sensors/imu/mpu_driver.h"
#include "sensors/encoder/encoder_manager.h"
#include "hardware/wheelchair_interface/wheelchair_interface.h"
#include "communication/protocol_types.h"
#include "communication/command_handler.h"

#ifndef ENV_NATIVE
#  include <Arduino.h>
#else
#  include <chrono>
#endif

using namespace autochair;
using namespace autochair::config;

// =============================================================================
// Module instances  (heap-free; static allocation)
// =============================================================================

static StateMachine       stateMachine;
static SafetyManager      safetyManager(stateMachine);
static UltrasonicManager  ultrasonicManager;

static MPUConfig          imuConfig{0, IMUDeviceType::UNKNOWN, config::IMU_DEFAULT_I2C_ADDRESS, AccelScale::SCALE_4G, GyroScale::SCALE_500DPS, 5};
static MPUDriver          imu(imuConfig);

static EncoderManager     encoderManager;

// Wheelchair interface — simulation only in V1
static WheelchairInterfaceSimulation wheelchairInterface;

// Command handler
static CommandHandler commandHandler(stateMachine, safetyManager);

// =============================================================================
// Task timing
// =============================================================================

static uint32_t lastSafetyMs     = 0;
static uint32_t lastEncoderMs    = 0;
static uint32_t lastImuMs        = 0;
static uint32_t lastUltrasonicMs = 0;
static uint32_t lastTelemetryMs  = 0;
static uint32_t lastDiagnosticsMs= 0;

// =============================================================================
// Self-test  (SYSTEM_STATE_MACHINE.md §3.3)
// =============================================================================

static bool runSelfTest() {
    Logger::info("Main", "Running self-test...");

    // Required subsystems for V1 self-test:
    //   - StateMachine initialized
    //   - SafetyManager initialized
    //   - WheelchairInterface in simulation mode

    if (stateMachine.getState() != SystemState::SELF_TEST) {
        Logger::error("Main", "Self-test: unexpected state");
        return false;
    }

    // Confirm wheelchair interface is in simulation mode (NOT physical).
    if (wheelchairInterface.getMode() != WheelchairInterfaceMode::SIMULATION) {
        Logger::fault("Main",
            "SELF-TEST FAIL: wheelchair interface not in simulation mode. "
            "Physical mode must not be active in V1.");
        return false;
    }

    // V1: sensors stubs are allowed to be UNINITIALIZED.
    // Real hardware acceptance criteria are set in Phases 4–6.

    Logger::info("Main", "Self-test passed (V1 software-only)");
    return true;
}

// =============================================================================
// Setup
// =============================================================================

void setup() {
    Logger::begin(SERIAL_BAUD_RATE);
    Logger::info("Main", "AutoChair ESP32 firmware starting");
    Logger::info("Main", "Firmware v" AUTOCHAIR_FIRMWARE_VERSION);

    // --- BOOT → INITIALIZING ---
    stateMachine.begin();
    stateMachine.processEvent(SystemEvent::STARTUP_COMPLETE, "Boot complete");

    // --- Initialize subsystems ---
    safetyManager.begin();

    // Initialize sensors
    ultrasonicManager.begin();
    imu.begin();
    encoderManager.begin();

    // Wheelchair interface — simulation only.
    if (!wheelchairInterface.initialize()) {
        Logger::fault("Main", "Wheelchair interface init failed");
        stateMachine.processEvent(SystemEvent::INITIALIZATION_FAILED,
                                  "Wheelchair interface");
        return;
    }
    Logger::info("Main", "Wheelchair interface: SIMULATION");

    stateMachine.processEvent(SystemEvent::INITIALIZATION_COMPLETE,
                               "All modules ready");

    // --- SELF_TEST ---
    if (runSelfTest()) {
        stateMachine.processEvent(SystemEvent::SELF_TEST_PASSED);
    } else {
        stateMachine.processEvent(SystemEvent::SELF_TEST_FAILED);
        Logger::fault("Main",
            "System cannot proceed — remaining in FAULT. "
            "Address self-test failures before continuing.");
        return;
    }

    Logger::info("Main", "AutoChair ESP32 in IDLE — ready for Pi connection");
}

// =============================================================================
// Main loop  (cooperative scheduling)
// =============================================================================

void loop() {
#ifndef ENV_NATIVE
    uint32_t now = millis();
#else
    // In native environment, millis() shim is used via lambda.
    // For this file we use Arduino millis() signature.
    uint32_t now = 0;
    {
        using namespace std::chrono;
        static auto start = steady_clock::now();
        now = static_cast<uint32_t>(
            duration_cast<milliseconds>(steady_clock::now() - start).count());
    }
#endif

    // ---- Safety (highest priority) ----
    if ((now - lastSafetyMs) >= SAFETY_MONITOR_INTERVAL_MS) {
        lastSafetyMs = now;
        safetyManager.update();
    }

    // ---- Encoder acquisition (high frequency) ----
    if ((now - lastEncoderMs) >= ENCODER_ACQUIRE_INTERVAL_MS) {
        lastEncoderMs = now;
        encoderManager.update();
    }

    // ---- IMU acquisition ----
    if ((now - lastImuMs) >= IMU_ACQUIRE_INTERVAL_MS) {
        lastImuMs = now;
        imu.update();
    }

    // ---- Ultrasonic sensors (sequential to reduce interference) ----
    if ((now - lastUltrasonicMs) >= ULTRASONIC_CYCLE_INTERVAL_MS) {
        lastUltrasonicMs = now;
        ultrasonicManager.update();
    }

    // ---- Telemetry (lower frequency) ----
    if ((now - lastTelemetryMs) >= TELEMETRY_INTERVAL_MS) {
        lastTelemetryMs = now;
        // Phase 9: serialize and transmit status/sensor telemetry to Pi.
        // Placeholder: nothing transmitted in Phase 1.
    }

    // ---- Diagnostics ----
    if ((now - lastDiagnosticsMs) >= DIAGNOSTICS_INTERVAL_MS) {
        lastDiagnosticsMs = now;
        Logger::logf(LogLevel::DEBUG, "Main", "State=%d Safety=%d Mode=%d",
                     static_cast<int>(stateMachine.getState()),
                     static_cast<int>(stateMachine.getSafety()),
                     static_cast<int>(stateMachine.getMode()));

        const auto& enc = encoderManager.getWheelData().left;
        const auto& health = encoderManager.getLeftDriver().getHealth();
        Logger::logf(LogLevel::DEBUG, "Encoder", "L: cnt=%lld dir=%d rpm=%.2f pulses=%u idx=%u inv=%u st=%d",
                     static_cast<long long>(enc.count),
                     static_cast<int>(enc.direction),
                     static_cast<double>(enc.rpm),
                     static_cast<unsigned int>(enc.pulses_since_last_update),
                     static_cast<unsigned int>(health.index_events),
                     static_cast<unsigned int>(health.invalid_transitions),
                     static_cast<int>(enc.status));

        // 6-Channel Ultrasonic Telemetry
        const auto& us0 = ultrasonicManager.getReading(0);
        const auto& us1 = ultrasonicManager.getReading(1);
        const auto& us2 = ultrasonicManager.getReading(2);
        const auto& us3 = ultrasonicManager.getReading(3);
        const auto& us4 = ultrasonicManager.getReading(4);
        const auto& us5 = ultrasonicManager.getReading(5);
        Logger::logf(LogLevel::INFO, "Ultrasonic",
                     "US0=%.1f(st=%d) US1=%.1f(st=%d) US2=%.1f(st=%d) US3=%.1f(st=%d) US4=%.1f(st=%d) US5=%.1f(st=%d) min=%.1f",
                     static_cast<double>(us0.distance_mm), static_cast<int>(us0.status),
                     static_cast<double>(us1.distance_mm), static_cast<int>(us1.status),
                     static_cast<double>(us2.distance_mm), static_cast<int>(us2.status),
                     static_cast<double>(us3.distance_mm), static_cast<int>(us3.status),
                     static_cast<double>(us4.distance_mm), static_cast<int>(us4.status),
                     static_cast<double>(us5.distance_mm), static_cast<int>(us5.status),
                     static_cast<double>(ultrasonicManager.getMinValidDistanceMm()));
    }

#ifndef ENV_NATIVE
    delay(1);
#endif
}
