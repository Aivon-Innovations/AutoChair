/**
 * @file main.cpp
 * @brief AutoChair — Standalone HC-SR04 Ultrasonic Sensor Bench Test Engine
 * 
 * Hardware Requirements:
 *  - ESP32-WROOM-32D Development Board (powered via USB)
 *  - HC-SR04 Ultrasonic Distance Sensor Modules (Sensors #1–#6)
 *  - Level Divider: 4.7kΩ + 4.7kΩ on each ECHO line (5V -> 2.5V safe step-down)
 *  - Power: Isolated 5.0V DC supply with common GND rail
 * 
 * Supported Channels:
 *  - Sensor 0 (Phys #1): TRIG = GPIO4,  ECHO = GPIO34
 *  - Sensor 1 (Phys #2): TRIG = GPIO5,  ECHO = GPIO35
 *  - Sensor 2 (Phys #3): TRIG = GPIO18, ECHO = GPIO36
 *  - Sensor 3 (Phys #4): TRIG = GPIO19, ECHO = GPIO39
 *  - Sensor 4 (Phys #5): TRIG = GPIO23, ECHO = GPIO32
 *  - Sensor 5 (Phys #6): TRIG = GPIO13, ECHO = GPIO33
 */

#include <Arduino.h>

constexpr uint8_t SENSOR_COUNT = 6;
constexpr uint32_t TIMEOUT_US = 30000; // ~5.1 m max range
constexpr float SPEED_OF_SOUND_MM_US = 0.343f; // 343 m/s at 20°C

// GPIO Configuration
constexpr uint8_t TRIG_PINS[SENSOR_COUNT] = { 4,  5, 18, 19, 23, 13 };
constexpr uint8_t ECHO_PINS[SENSOR_COUNT] = { 34, 35, 36, 39, 32, 33 };

enum class Mode {
    SINGLE_SENSOR, // Standalone single sensor bench test (Sensor 0 on GPIO4/34)
    ROUND_ROBIN    // 6-Sensor sequential round-robin array
};

static Mode currentMode = Mode::SINGLE_SENSOR;
static uint8_t currentSensorIndex = 0;
static uint32_t lastSampleMs = 0;
constexpr uint32_t SAMPLE_INTERVAL_MS = 50;

struct SensorData {
    float distance_mm = -1.0f;
    uint8_t status = 0; // 0=UNINIT, 2=OK, 4=TIMEOUT, 7=INVALID
    uint32_t total_samples = 0;
    uint32_t valid_samples = 0;
};

static SensorData sensors[SENSOR_COUNT];

float measureDistanceMm(uint8_t trigPin, uint8_t echoPin, uint8_t& outStatus) {
    if (trigPin == 0 || echoPin == 0) {
        outStatus = 7; // INVALID
        return -1.0f;
    }

    // Trigger pulse: 10 µs HIGH
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);

    // Measure echo pulse duration
    uint32_t durationUs = pulseIn(echoPin, HIGH, TIMEOUT_US);

    if (durationUs == 0) {
        outStatus = 4; // TIMEOUT / No echo
        return -1.0f;
    }

    float distanceMm = (durationUs * SPEED_OF_SOUND_MM_US) / 2.0f;

    // Bounds checking (20 mm to 4000 mm)
    if (distanceMm < 20.0f || distanceMm > 4000.0f) {
        outStatus = 7; // INVALID / Out of range
        return -1.0f;
    }

    outStatus = 2; // OK
    return distanceMm;
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 1000);

    Serial.println("\n==================================================");
    Serial.println("AutoChair — HC-SR04 Ultrasonic Sensor Bench Test");
    Serial.println("==================================================");

    // Initialize configured GPIO pins
    for (uint8_t i = 0; i < SENSOR_COUNT; ++i) {
        if (TRIG_PINS[i] != 0 && ECHO_PINS[i] != 0) {
            pinMode(TRIG_PINS[i], OUTPUT);
            digitalWrite(TRIG_PINS[i], LOW);
            pinMode(ECHO_PINS[i], INPUT);
        }
    }

    Serial.println("[INIT] All Ultrasonic channels configured.");
    Serial.println("[MODE] SINGLE_SENSOR active (Sensor 0 / GPIO4 TRIG, GPIO34 ECHO).");
    Serial.println("Send 'ARRAY' for 6-sensor round-robin mode or 'SINGLE' for single sensor mode.\n");
}

void loop() {
    // Process serial commands
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        cmd.toUpperCase();

        if (cmd == "SINGLE") {
            currentMode = Mode::SINGLE_SENSOR;
            Serial.println("[MODE] Switched to SINGLE_SENSOR mode (Sensor 0 / GPIO4/GPIO34).");
        } else if (cmd == "ARRAY") {
            currentMode = Mode::ROUND_ROBIN;
            Serial.println("[MODE] Switched to ROUND_ROBIN mode (Sequential 6-Sensor array).");
        } else if (cmd == "RESET") {
            for (uint8_t i = 0; i < SENSOR_COUNT; ++i) {
                sensors[i] = SensorData();
            }
            Serial.println("[STATUS] Statistics reset.");
        }
    }

    uint32_t now = millis();
    if (now - lastSampleMs >= SAMPLE_INTERVAL_MS) {
        lastSampleMs = now;

        if (currentMode == Mode::SINGLE_SENSOR) {
            // Single Sensor Bench Smoke Test (Sensor 0: TRIG=4, ECHO=34)
            uint8_t status = 0;
            float dist = measureDistanceMm(TRIG_PINS[0], ECHO_PINS[0], status);
            sensors[0].distance_mm = dist;
            sensors[0].status = status;
            sensors[0].total_samples++;
            if (status == 2) sensors[0].valid_samples++;

            float distCm = (dist >= 0.0f) ? (dist / 10.0f) : -0.1f;
            Serial.printf("[US0_BENCH] dist=%.1f mm (%.1f cm) [status=%d] valid=%u/%u\n",
                          dist, distCm, status, sensors[0].valid_samples, sensors[0].total_samples);
        } else {
            // Sequential Round-Robin Array Polling
            uint8_t idx = currentSensorIndex;
            uint8_t status = 0;
            float dist = measureDistanceMm(TRIG_PINS[idx], ECHO_PINS[idx], status);
            sensors[idx].distance_mm = dist;
            sensors[idx].status = status;
            sensors[idx].total_samples++;
            if (status == 2) sensors[idx].valid_samples++;

            currentSensorIndex = (currentSensorIndex + 1) % SENSOR_COUNT;

            // Output array status after completing full cycle
            if (currentSensorIndex == 0) {
                float minDist = -1.0f;
                for (uint8_t i = 0; i < SENSOR_COUNT; ++i) {
                    if (sensors[i].status == 2 && sensors[i].distance_mm >= 0.0f) {
                        if (minDist < 0.0f || sensors[i].distance_mm < minDist) {
                            minDist = sensors[i].distance_mm;
                        }
                    }
                }

                Serial.printf("[US_ARRAY] US0=%.1f(st=%d) US1=%.1f(st=%d) US2=%.1f(st=%d) US3=%.1f(st=%d) US4=%.1f(st=%d) US5=%.1f(st=%d) | min=%.1f mm\n",
                              sensors[0].distance_mm, sensors[0].status,
                              sensors[1].distance_mm, sensors[1].status,
                              sensors[2].distance_mm, sensors[2].status,
                              sensors[3].distance_mm, sensors[3].status,
                              sensors[4].distance_mm, sensors[4].status,
                              sensors[5].distance_mm, sensors[5].status,
                              minDist);
            }
        }
    }

    delay(1); // FreeRTOS watchdog yield
}
