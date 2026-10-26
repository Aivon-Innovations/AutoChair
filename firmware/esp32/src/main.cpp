/**
 * @file main.cpp
 * @brief AutoChair ESP32 — Six-Sensor Obstacle-Zone Detection Firmware
 *
 * Project:  Aivon Innovations — AutoChair
 * Platform: ESP32-WROOM-32D
 * Branch:   alvira-dev
 * Status:   IN DEVELOPMENT — OBSTACLE DETECTION ONLY
 *
 * SAFETY BOUNDARY:
 *   This firmware classifies ultrasonic distance readings into obstacle zones.
 *   It does NOT issue motor commands, activate brakes, or change the
 *   wheelchair's physical motion state in any way.
 *   Obstacle zone classification only.
 *
 * THRESHOLD STATUS:
 *   OBSTACLE < 400 mm, CAUTION 400–799 mm, CLEAR >= 800 mm.
 *   These are UNVALIDATED PROVISIONAL bench-test parameters.
 *   They have no relationship to wheelchair stopping distance.
 *
 * Telemetry format (115200 baud):
 *   [CYCLE N] -------
 *   [S1] 243.2 mm  OK  AGE:50ms  ZONE:CLEAR
 *   [S2] TIMEOUT        AGE:--   ZONE:FAULT
 *   ...
 *   [OVERALL] CAUTION
 *   ----
 *
 * Upload procedure (IMPORTANT):
 *   1. Unplug sensor 5V supply before uploading.
 *   2. Upload via Arduino IDE or 'pio run -e esp32dev --target upload'.
 *   3. Plug sensor 5V supply back in after 'Done uploading'.
 *   4. Press EN on the ESP32.
 */

#include <Arduino.h>
#include "sensors/ultrasonic/ultrasonic_manager.h"
#include "sensors/ultrasonic/obstacle_zone.h"
#include "autochair_config.h"
#include "autochair_types.h"

using namespace autochair;
using namespace autochair::config;

// =============================================================================
// Module-level objects
// =============================================================================

static UltrasonicManager   g_manager;
static ObstacleZoneClassifier g_classifier;

// Snapshot of all readings taken once per full cycle for consistent telemetry.
static UltrasonicReading   g_snapshot[ULTRASONIC_COUNT];

static uint32_t g_cycleCount    = 0;
static uint32_t g_lastCycleMs   = 0;

// =============================================================================
// Helpers
// =============================================================================

/**
 * @brief Print one sensor line to Serial.
 * Format:  [S1] 243.2 mm  OK   AGE:50ms   ZONE:CLEAR
 */
static void printSensorLine(uint8_t idx,
                             const UltrasonicReading& r,
                             ObstacleZone zone,
                             uint32_t now_ms) {
    Serial.print(F("  [S"));
    Serial.print(idx + 1);
    Serial.print(F("] "));

    if (r.status == SensorStatus::OK) {
        Serial.print(r.distance_mm, 1);
        Serial.print(F(" mm"));

        // Padding for alignment
        if (r.distance_mm < 1000.0f) Serial.print(' ');
        if (r.distance_mm < 100.0f)  Serial.print(' ');
        if (r.distance_mm < 10.0f)   Serial.print(' ');

        Serial.print(F("  OK"));

        // Reading age
        uint32_t age = (now_ms >= r.timestamp_ms) ? (now_ms - r.timestamp_ms) : 0;
        Serial.print(F("   AGE:"));
        Serial.print(age);
        Serial.print(F("ms"));

    } else if (r.status == SensorStatus::TIMEOUT) {
        Serial.print(F("TIMEOUT            "));
    } else if (r.status == SensorStatus::FAULT) {
        Serial.print(F("FAULT              "));
    } else {
        Serial.print(F("UNINITIALIZED      "));
    }

    Serial.print(F("   ZONE:"));
    Serial.println(ObstacleZoneClassifier::zoneName(zone));
}

// =============================================================================
// Setup
// =============================================================================

void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    delay(500);

    Serial.println(F(""));
    Serial.println(F("============================================================"));
    Serial.println(F(" AutoChair — Obstacle Zone Detection"));
    Serial.println(F(" Branch: alvira-dev  |  ESP32-WROOM-32D"));
    Serial.println(F(" CLASSIFICATION ONLY — No motor commands issued"));
    Serial.println(F("============================================================"));
    Serial.println(F(" GPIO Map:"));
    Serial.println(F("   S1: TRIG=GPIO4  ECHO=GPIO34 | S4: TRIG=GPIO19 ECHO=GPIO39"));
    Serial.println(F("   S2: TRIG=GPIO5  ECHO=GPIO35 | S5: TRIG=GPIO23 ECHO=GPIO32"));
    Serial.println(F("   S3: TRIG=GPIO18 ECHO=GPIO36 | S6: TRIG=GPIO13 ECHO=GPIO33"));
    Serial.println(F(" Thresholds (UNVALIDATED PROVISIONAL):"));
    Serial.print  (F("   OBSTACLE < "));
    Serial.print  (ZONE_OBSTACLE_MM, 0);
    Serial.print  (F(" mm  |  CAUTION < "));
    Serial.print  (ZONE_CAUTION_MM, 0);
    Serial.println(F(" mm  |  CLEAR >= 800 mm"));
    Serial.println(F("============================================================"));
    Serial.println(F(""));

    g_manager.begin();
    g_lastCycleMs = millis();

    Serial.println(F("[BOOT] Manager initialised. Starting acquisition..."));
    delay(1000);
}

// =============================================================================
// Loop — round-robin acquisition + zone telemetry
// =============================================================================

void loop() {
    // Advance one sensor per ULTRASONIC_CYCLE_INTERVAL_MS tick.
    static uint32_t lastUpdateMs  = 0;
    static uint8_t  updateCount   = 0;

    uint32_t now = millis();

    if (now - lastUpdateMs >= ULTRASONIC_CYCLE_INTERVAL_MS) {
        lastUpdateMs = now;
        g_manager.update();
        updateCount++;
    }

    // After a full cycle (all 6 sensors updated), print telemetry.
    if (updateCount >= ULTRASONIC_COUNT) {
        updateCount = 0;
        g_cycleCount++;

        // Snapshot all readings at the same logical time.
        uint32_t snapMs = millis();
        for (uint8_t i = 0; i < ULTRASONIC_COUNT; ++i) {
            g_snapshot[i] = g_manager.getReading(i);
        }

        // Run zone classifier.
        g_classifier.update(g_snapshot, ULTRASONIC_COUNT, snapMs);

        // ── Telemetry output ──────────────────────────────────────────────
        Serial.println(F(""));
        Serial.print(F("[CYCLE "));
        Serial.print(g_cycleCount);
        Serial.print(F("]  t="));
        Serial.print(snapMs);
        Serial.println(F("ms  ─────────────────────────────────────"));

        for (uint8_t i = 0; i < ULTRASONIC_COUNT; ++i) {
            printSensorLine(i, g_snapshot[i], g_classifier.getSensorZone(i), snapMs);
        }

        Serial.print(F("  [OVERALL] "));
        Serial.println(ObstacleZoneClassifier::zoneName(g_classifier.getOverallZone()));
        Serial.println(F("  ──────────────────────────────────────────────────────"));
    }
}
