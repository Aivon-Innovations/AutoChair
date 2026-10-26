/**
 * @file main.cpp
 * @brief AutoChair — Six-Sensor HC-SR04 Connection Verification Test
 *
 * Project  : Aivon Innovations — AutoChair
 * Branch   : alvira-dev
 * Date     : 2026-10-22
 * Purpose  : Stage A/B/C physical connection verification for the six-sensor
 *            ultrasonic array bench test.
 *
 * GPIO Map (verified):
 *   Sensor #1 (ID 0) : TRIG = GPIO4,  ECHO = GPIO34
 *   Sensor #2 (ID 1) : TRIG = GPIO5,  ECHO = GPIO35
 *   Sensor #3 (ID 2) : TRIG = GPIO18, ECHO = GPIO36
 *   Sensor #4 (ID 3) : TRIG = GPIO19, ECHO = GPIO39
 *   Sensor #5 (ID 4) : TRIG = GPIO23, ECHO = GPIO32
 *   Sensor #6 (ID 5) : TRIG = GPIO13, ECHO = GPIO33
 *
 * Each ECHO uses an independent 4.7kΩ + 4.7kΩ voltage divider.
 * Sensor VCC powered from separate 5V USB supply.
 * ESP32 powered from Windows laptop USB.
 *
 * Output format (115200 baud):
 *   [BOOT] line on startup
 *   One line per sensor per cycle:
 *   [S1] 243.2 mm  OK
 *   [S2] TIMEOUT
 *   [S3] 412.8 mm  OK
 *   ...
 *   [CYCLE 4] ---- summary ----
 */

#include <Arduino.h>

// =============================================================================
// Hardware configuration
// =============================================================================

static constexpr uint8_t SENSOR_COUNT = 6;

static constexpr uint8_t TRIG_PIN[SENSOR_COUNT] = {  4,  5, 18, 19, 23, 13 };
static constexpr uint8_t ECHO_PIN[SENSOR_COUNT] = { 34, 35, 36, 39, 32, 33 };

// Maximum echo wait = ~5.1 m round trip at 343 m/s
static constexpr uint32_t TIMEOUT_US   = 30000;

// Minimum inter-trigger delay between consecutive sensors (ms).
// Allows the previous acoustic pulse to decay before next trigger.
static constexpr uint32_t INTER_SENSOR_DELAY_MS = 60;

// Pause between complete 6-sensor cycles (ms).
static constexpr uint32_t CYCLE_PAUSE_MS = 500;

// Speed of sound at 20°C in mm/µs
static constexpr float SOUND_MM_PER_US = 0.3430f;

// Valid range limits
static constexpr float MIN_DIST_MM = 20.0f;
static constexpr float MAX_DIST_MM = 4000.0f;

// =============================================================================
// Per-sensor counters (for summary line)
// =============================================================================

struct SensorStats {
    uint32_t total   = 0;
    uint32_t ok      = 0;
    uint32_t timeout = 0;
    uint32_t invalid = 0;
    float    lastDist = -1.0f;
};

static SensorStats stats[SENSOR_COUNT];
static uint32_t    cycleCount = 0;

// =============================================================================
// Measure a single sensor
// =============================================================================

/**
 * @brief Trigger sensor at index and return measured distance in mm.
 * @param idx Sensor index (0–5).
 * @param[out] outStatus  0=OK, 1=TIMEOUT, 2=INVALID
 * @return distance_mm (>= 0) on success, -1.0f on failure.
 */
static float measureSensor(uint8_t idx, uint8_t& outStatus) {
    const uint8_t trig = TRIG_PIN[idx];
    const uint8_t echo = ECHO_PIN[idx];

    // Ensure TRIG is low before pulse
    digitalWrite(trig, LOW);
    delayMicroseconds(4);

    // 10 µs HIGH trigger pulse
    digitalWrite(trig, HIGH);
    delayMicroseconds(10);
    digitalWrite(trig, LOW);

    // Measure ECHO pulse width
    uint32_t durationUs = pulseIn(echo, HIGH, TIMEOUT_US);

    if (durationUs == 0) {
        outStatus = 1;  // TIMEOUT
        return -1.0f;
    }

    float dist_mm = (durationUs * SOUND_MM_PER_US) / 2.0f;

    if (dist_mm < MIN_DIST_MM || dist_mm > MAX_DIST_MM) {
        outStatus = 2;  // INVALID range
        return dist_mm; // Return for logging even if invalid
    }

    outStatus = 0;  // OK
    return dist_mm;
}

// =============================================================================
// Setup
// =============================================================================

void setup() {
    Serial.begin(115200);
    delay(500);  // Let UART stabilise

    Serial.println(F(""));
    Serial.println(F("============================================================"));
    Serial.println(F(" AutoChair — Six-Sensor HC-SR04 Connection Test"));
    Serial.println(F(" Branch: alvira-dev  |  2026-10-22"));
    Serial.println(F("============================================================"));
    Serial.println(F(" GPIO Map:"));
    Serial.println(F("   S1: TRIG=4   ECHO=34"));
    Serial.println(F("   S2: TRIG=5   ECHO=35"));
    Serial.println(F("   S3: TRIG=18  ECHO=36"));
    Serial.println(F("   S4: TRIG=19  ECHO=39"));
    Serial.println(F("   S5: TRIG=23  ECHO=32"));
    Serial.println(F("   S6: TRIG=13  ECHO=33"));
    Serial.println(F("============================================================"));
    Serial.println(F(" Place hand/target in front of each sensor in turn."));
    Serial.println(F(" Expected: distance reading + OK for connected sensors."));
    Serial.println(F(" TIMEOUT = sensor not responding (check wiring/power)."));
    Serial.println(F("============================================================"));
    Serial.println(F(""));

    // Configure GPIO pins
    for (uint8_t i = 0; i < SENSOR_COUNT; ++i) {
        pinMode(TRIG_PIN[i], OUTPUT);
        digitalWrite(TRIG_PIN[i], LOW);
        pinMode(ECHO_PIN[i], INPUT);
    }

    Serial.println(F("[BOOT] GPIO configured. Starting acquisition in 1 s..."));
    delay(1000);
}

// =============================================================================
// Loop — round-robin acquisition
// =============================================================================

void loop() {
    cycleCount++;
    Serial.print(F("\n[CYCLE "));
    Serial.print(cycleCount);
    Serial.println(F("] -----------------------------------------------"));

    for (uint8_t i = 0; i < SENSOR_COUNT; ++i) {
        uint8_t status = 0;
        float dist = measureSensor(i, status);

        stats[i].total++;
        stats[i].lastDist = dist;

        // Build label
        Serial.print(F("  [S"));
        Serial.print(i + 1);
        Serial.print(F("] "););

        if (status == 0) {
            // OK
            stats[i].ok++;
            Serial.print(dist, 1);
            Serial.print(F(" mm  "));

            // Zone hint
            if (dist < 400.0f) {
                Serial.println(F("OK  [ZONE: OBSTACLE]"));
            } else if (dist < 800.0f) {
                Serial.println(F("OK  [ZONE: CAUTION]"));
            } else {
                Serial.println(F("OK  [ZONE: CLEAR]"));
            }

        } else if (status == 1) {
            // Timeout
            stats[i].timeout++;
            Serial.println(F("TIMEOUT  (no echo — check power/wiring)"));

        } else {
            // Out of range
            stats[i].invalid++;
            Serial.print(dist, 1);
            Serial.println(F(" mm  INVALID (out of 20–4000 mm range)"));
        }

        // Inter-sensor delay to reduce acoustic crosstalk
        delay(INTER_SENSOR_DELAY_MS);
    }

    // Summary every 5 cycles
    if (cycleCount % 5 == 0) {
        Serial.println(F("\n  --- 5-Cycle Summary ---"));
        Serial.println(F("  Sensor | Total | OK    | TIMEOUT | INVALID | Last(mm)"));
        Serial.println(F("  -------|-------|-------|---------|---------|----------"));
        for (uint8_t i = 0; i < SENSOR_COUNT; ++i) {
            Serial.printf("  S%-6d | %-5lu | %-5lu | %-7lu | %-7lu | ",
                          i + 1,
                          (unsigned long)stats[i].total,
                          (unsigned long)stats[i].ok,
                          (unsigned long)stats[i].timeout,
                          (unsigned long)stats[i].invalid);
            if (stats[i].lastDist >= 0.0f) {
                Serial.printf("%.1f\n", stats[i].lastDist);
            } else {
                Serial.println(F("-"));
            }
        }
        Serial.println();
    }

    delay(CYCLE_PAUSE_MS);
}
