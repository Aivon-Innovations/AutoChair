/**
 * @file main.cpp
 * @brief AutoChair — Standalone Wheel Encoder Bench Validation Firmware
 *
 * Platform: ESP32-WROOM-32D (CP2102, USB-C)
 * Target Hardware: Incremental Optical Rotary Encoder (600 PPR / 2400 CPR X4)
 *
 * Pin Mapping:
 *   - Encoder Channel A (Green)  -> ESP32 GPIO25 (4.7kΩ pull-up to 3.3V)
 *   - Encoder Channel B (White)  -> ESP32 GPIO26 (4.7kΩ pull-up to 3.3V)
 *   - Encoder Channel Z (Yellow) -> ESP32 GPIO27 (4.7kΩ pull-up to 3.3V)
 *   - Encoder VCC (Red)          -> ESP32 5V
 *   - Encoder GND (Black)        -> ESP32 GND
 */

#include <Arduino.h>
#include <rom/rtc.h>

// =============================================================================
// Pin Configuration & Constants
// =============================================================================

constexpr uint8_t PIN_ENCODER_A = 25;
constexpr uint8_t PIN_ENCODER_B = 26;
constexpr uint8_t PIN_ENCODER_Z = 27;

constexpr uint16_t ENCODER_PPR       = 600;   // 600 waveform cycles / revolution
constexpr uint8_t  DECODING_MODE_X4  = 4;     // X4 quadrature decoding
constexpr uint32_t COUNTS_PER_REV    = ENCODER_PPR * DECODING_MODE_X4; // 2400 counts / rev

constexpr uint32_t TELEMETRY_INTERVAL_MS = 100; // 10 Hz streaming rate

// =============================================================================
// Volatile Interrupt State
// =============================================================================

static volatile int64_t  g_rawCount           = 0;
static volatile uint32_t g_indexCount         = 0;
static volatile uint32_t g_invalidTransitions = 0;
static volatile uint32_t g_lastInterruptMs    = 0;
static volatile uint8_t  g_prevState          = 0;

// =============================================================================
// ISR Handlers (IRAM_ATTR)
// =============================================================================

// 4-bit Quadrature transition lookup table
// Index: (prevState << 2) | currState
// Values: +1 (CW: A leads B), -1 (CCW: B leads A), 0 (invalid or unchanged)
static const int8_t QUAD_TABLE[16] = {
     0, -1,  1,  0,   // prev 00 -> curr 00, 01, 10, 11
     1,  0,  0, -1,   // prev 01 -> curr 00, 01, 10, 11
    -1,  0,  0,  1,   // prev 10 -> curr 00, 01, 10, 11
     0,  1, -1,  0    // prev 11 -> curr 00, 01, 10, 11
};

void IRAM_ATTR isrPinChange() {
    uint8_t a = digitalRead(PIN_ENCODER_A);
    uint8_t b = digitalRead(PIN_ENCODER_B);
    uint8_t currState = (a << 1) | b;

    uint8_t tableIdx = (g_prevState << 2) | currState;
    int8_t step = QUAD_TABLE[tableIdx];

    if (step != 0) {
        g_rawCount += step;
        g_lastInterruptMs = millis();
    } else if (g_prevState != currState) {
        // Double transition / noise glitch
        g_invalidTransitions++;
    }

    g_prevState = currState;
}

void IRAM_ATTR isrIndexRising() {
    g_indexCount++;
    g_lastInterruptMs = millis();
}

// =============================================================================
// Telemetry & Diagnostic State
// =============================================================================

static int64_t  lastTelemetryCount = 0;
static uint32_t lastTelemetryTime  = 0;
static bool     streamActive       = true;

void resetEncoderState() {
    noInterrupts();
    g_rawCount           = 0;
    g_indexCount         = 0;
    g_invalidTransitions = 0;
    lastTelemetryCount   = 0;
    lastTelemetryTime    = millis();
    interrupts();

    Serial.println("[STATUS] ENCODER COUNTERS RESET TO ZERO");
}

void printSystemStatus() {
    Serial.println("\n--- SYSTEM TELEMETRY ---");
    Serial.print("Uptime_ms: "); Serial.println(millis());
    Serial.print("Free_Heap_bytes: "); Serial.println(esp_get_free_heap_size());
    Serial.print("CPU0_Reset_Reason: "); Serial.println(rtc_get_reset_reason(0));
    Serial.print("CPU1_Reset_Reason: "); Serial.println(rtc_get_reset_reason(1));
    Serial.print("Configured_CPR: "); Serial.println(COUNTS_PER_REV);
    Serial.print("Pin_A: GPIO"); Serial.println(PIN_ENCODER_A);
    Serial.print("Pin_B: GPIO"); Serial.println(PIN_ENCODER_B);
    Serial.print("Pin_Z: GPIO"); Serial.println(PIN_ENCODER_Z);
    Serial.println("------------------------\n");
}

// =============================================================================
// Setup
// =============================================================================

void setup() {
    Serial.begin(115200);
    delay(100);

    Serial.println("\n========================================================");
    Serial.println("  AutoChair — Wheel Encoder Standalone Bench Test");
    Serial.println("  Quadrature X4 Decoder (2400 CPR) + Dynamic RPM");
    Serial.println("========================================================");

    // Configure GPIOs with pull-ups
    pinMode(PIN_ENCODER_A, INPUT_PULLUP);
    pinMode(PIN_ENCODER_B, INPUT_PULLUP);
    pinMode(PIN_ENCODER_Z, INPUT_PULLUP);

    // Initial state
    uint8_t a = digitalRead(PIN_ENCODER_A);
    uint8_t b = digitalRead(PIN_ENCODER_B);
    g_prevState = (a << 1) | b;

    // Attach interrupts
    attachInterrupt(digitalPinToInterrupt(PIN_ENCODER_A), isrPinChange, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_ENCODER_B), isrPinChange, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_ENCODER_Z), isrIndexRising, RISING);

    lastTelemetryTime  = millis();
    lastTelemetryCount = 0;

    Serial.println("[INFO] Encoder driver initialized. Pins: A=25, B=26, Z=27");
    Serial.println("[INFO] Send 'CMD:RESET' to zero counts, 'CMD:STATUS' for system health.\n");
}

// =============================================================================
// Main Loop
// =============================================================================

void loop() {
    uint32_t now = millis();

    // Check for incoming serial commands
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();

        if (cmd == "CMD:RESET") {
            resetEncoderState();
        } else if (cmd == "CMD:STATUS") {
            printSystemStatus();
        } else if (cmd == "CMD:STREAM_ON") {
            streamActive = true;
            Serial.println("[STATUS] Streaming ON");
        } else if (cmd == "CMD:STREAM_OFF") {
            streamActive = false;
            Serial.println("[STATUS] Streaming OFF");
        }
    }

    // Periodic telemetry output (100 ms)
    if (streamActive && (now - lastTelemetryTime >= TELEMETRY_INTERVAL_MS)) {
        uint32_t elapsedMs = now - lastTelemetryTime;
        lastTelemetryTime = now;

        // Atomically snapshot interrupt counters
        noInterrupts();
        int64_t currentCount = g_rawCount;
        uint32_t indexEvents = g_indexCount;
        uint32_t invalidTrans = g_invalidTransitions;
        interrupts();

        int64_t delta = currentCount - lastTelemetryCount;
        lastTelemetryCount = currentCount;

        uint32_t absDelta = static_cast<uint32_t>(delta >= 0 ? delta : -delta);

        // Direction derivation
        int dir = 0;
        if (delta > 0) {
            dir = 1;   // FORWARD / CW
        } else if (delta < 0) {
            dir = -1;  // REVERSE / CCW
        } else {
            dir = 0;   // STATIONARY
        }

        // RPM calculation:
        // RPM = (delta_count / CPR) * (60,000 / elapsed_ms)
        float rpm = 0.0f;
        if (elapsedMs > 0 && COUNTS_PER_REV > 0 && delta != 0) {
            rpm = (static_cast<float>(delta) / static_cast<float>(COUNTS_PER_REV)) *
                  (60000.0f / static_cast<float>(elapsedMs));
        }

        // Sensor status: 2 = OK
        int st = 2;

        // Standard AutoChair encoder diagnostic line format
        Serial.printf("[Encoder] L: cnt=%lld dir=%d rpm=%.2f pulses=%u idx=%u inv=%u st=%d\n",
                      static_cast<long long>(currentCount),
                      dir,
                      static_cast<double>(rpm),
                      static_cast<unsigned int>(absDelta),
                      static_cast<unsigned int>(indexEvents),
                      static_cast<unsigned int>(invalidTrans),
                      st);
    }
}
