/**
 * @file main.cpp
 * @brief AutoChair — Standalone Dual Wheel Encoder Bench Validation Firmware
 *
 * Platform: ESP32-WROOM-32D (CP2102, USB-C)
 * Target Hardware: 2 × Incremental Optical Rotary Encoders (600 PPR / 2400 CPR X4)
 *
 * Pin Mapping:
 *   - LEFT Encoder Channel A (Green)  -> ESP32 GPIO25 (4.7kΩ pull-up to 3.3V)
 *   - LEFT Encoder Channel B (White)  -> ESP32 GPIO26 (4.7kΩ pull-up to 3.3V)
 *   - LEFT Encoder Channel Z (Yellow) -> ESP32 GPIO27 (4.7kΩ pull-up to 3.3V)
 *   - LEFT Encoder VCC (Red)          -> ESP32 5V
 *   - LEFT Encoder GND (Black)        -> ESP32 GND
 *
 *   - RIGHT Encoder Channel A (Green)  -> ESP32 GPIO16 (4.7kΩ pull-up to 3.3V)
 *   - RIGHT Encoder Channel B (White)  -> ESP32 GPIO17 (4.7kΩ pull-up to 3.3V)
 *   - RIGHT Encoder Channel Z (Yellow) -> ESP32 GPIO14 (4.7kΩ pull-up to 3.3V)
 *   - RIGHT Encoder VCC (Red)          -> ESP32 5V
 *   - RIGHT Encoder GND (Black)        -> ESP32 GND
 */

#include <Arduino.h>
#include <rom/rtc.h>

// =============================================================================
// Pin Configuration & Constants
// =============================================================================

// LEFT Encoder (Instance 0)
constexpr uint8_t PIN_LEFT_A = 25;
constexpr uint8_t PIN_LEFT_B = 26;
constexpr uint8_t PIN_LEFT_Z = 27;

// RIGHT Encoder (Instance 1)
constexpr uint8_t PIN_RIGHT_A = 16;
constexpr uint8_t PIN_RIGHT_B = 17;
constexpr uint8_t PIN_RIGHT_Z = 14;

constexpr uint16_t ENCODER_PPR       = 600;   // 600 waveform cycles / revolution
constexpr uint8_t  DECODING_MODE_X4  = 4;     // X4 quadrature decoding
constexpr uint32_t COUNTS_PER_REV    = ENCODER_PPR * DECODING_MODE_X4; // 2400 counts / rev

constexpr uint32_t TELEMETRY_INTERVAL_MS = 100; // 10 Hz streaming rate

// =============================================================================
// 4-bit Quadrature Transition Table
// =============================================================================

// Index: (prevState << 2) | currState
// Values: +1 (CW: A leads B), -1 (CCW: B leads A), 0 (invalid or unchanged)
static const int8_t QUAD_TABLE[16] = {
     0, -1,  1,  0,   // prev 00 -> curr 00, 01, 10, 11
     1,  0,  0, -1,   // prev 01 -> curr 00, 01, 10, 11
    -1,  0,  0,  1,   // prev 10 -> curr 00, 01, 10, 11
     0,  1, -1,  0    // prev 11 -> curr 00, 01, 10, 11
};

// =============================================================================
// Volatile Interrupt State
// =============================================================================

struct EncoderState {
    volatile int64_t  rawCount           = 0;
    volatile uint32_t indexCount         = 0;
    volatile uint32_t invalidTransitions = 0;
    volatile uint32_t lastInterruptMs    = 0;
    volatile uint8_t  prevState          = 0;
};

static EncoderState g_leftState;
static EncoderState g_rightState;

// =============================================================================
// ISR Handlers (IRAM_ATTR)
// =============================================================================

// LEFT Encoder ISRs
void IRAM_ATTR isrLeftPinChange() {
    uint8_t a = digitalRead(PIN_LEFT_A);
    uint8_t b = digitalRead(PIN_LEFT_B);
    uint8_t currState = (a << 1) | b;

    uint8_t tableIdx = (g_leftState.prevState << 2) | currState;
    int8_t step = QUAD_TABLE[tableIdx];

    if (step != 0) {
        g_leftState.rawCount += step;
        g_leftState.lastInterruptMs = millis();
    } else if (g_leftState.prevState != currState) {
        g_leftState.invalidTransitions++;
    }

    g_leftState.prevState = currState;
}

void IRAM_ATTR isrLeftIndex() {
    g_leftState.indexCount++;
    g_leftState.lastInterruptMs = millis();
}

// RIGHT Encoder ISRs
void IRAM_ATTR isrRightPinChange() {
    uint8_t a = digitalRead(PIN_RIGHT_A);
    uint8_t b = digitalRead(PIN_RIGHT_B);
    uint8_t currState = (a << 1) | b;

    uint8_t tableIdx = (g_rightState.prevState << 2) | currState;
    int8_t step = QUAD_TABLE[tableIdx];

    if (step != 0) {
        g_rightState.rawCount += step;
        g_rightState.lastInterruptMs = millis();
    } else if (g_rightState.prevState != currState) {
        g_rightState.invalidTransitions++;
    }

    g_rightState.prevState = currState;
}

void IRAM_ATTR isrRightIndex() {
    g_rightState.indexCount++;
    g_rightState.lastInterruptMs = millis();
}

// =============================================================================
// Telemetry & Diagnostic State
// =============================================================================

static int64_t  lastTelemetryCountL = 0;
static int64_t  lastTelemetryCountR = 0;
static uint32_t lastTelemetryTime   = 0;
static bool     streamActive        = true;

void resetEncoderState() {
    noInterrupts();
    g_leftState.rawCount             = 0;
    g_leftState.indexCount           = 0;
    g_leftState.invalidTransitions   = 0;
    g_rightState.rawCount            = 0;
    g_rightState.indexCount          = 0;
    g_rightState.invalidTransitions  = 0;
    lastTelemetryCountL              = 0;
    lastTelemetryCountR              = 0;
    lastTelemetryTime                = millis();
    interrupts();

    Serial.println("[STATUS] DUAL ENCODER COUNTERS RESET TO ZERO");
}

void printSystemStatus() {
    Serial.println("\n--- SYSTEM TELEMETRY ---");
    Serial.print("Uptime_ms: "); Serial.println(millis());
    Serial.print("Free_Heap_bytes: "); Serial.println(esp_get_free_heap_size());
    Serial.print("CPU0_Reset_Reason: "); Serial.println(rtc_get_reset_reason(0));
    Serial.print("CPU1_Reset_Reason: "); Serial.println(rtc_get_reset_reason(1));
    Serial.print("Configured_CPR: "); Serial.println(COUNTS_PER_REV);
    Serial.print("LEFT Pins: A=GPIO"); Serial.print(PIN_LEFT_A);
    Serial.print(" B=GPIO"); Serial.print(PIN_LEFT_B);
    Serial.print(" Z=GPIO"); Serial.println(PIN_LEFT_Z);
    Serial.print("RIGHT Pins: A=GPIO"); Serial.print(PIN_RIGHT_A);
    Serial.print(" B=GPIO"); Serial.print(PIN_RIGHT_B);
    Serial.print(" Z=GPIO"); Serial.println(PIN_RIGHT_Z);
    Serial.println("------------------------\n");
}

// =============================================================================
// Setup
// =============================================================================

void setup() {
    Serial.begin(115200);
    delay(100);

    Serial.println("\n========================================================");
    Serial.println("  AutoChair — Dual Wheel Encoder Bench Validation");
    Serial.println("  LEFT (GPIO 25, 26, 27) | RIGHT (GPIO 16, 17, 14)");
    Serial.println("  Quadrature X4 Decoder (2400 CPR) + Dynamic RPM");
    Serial.println("========================================================");

    // Configure LEFT GPIOs with pull-ups
    pinMode(PIN_LEFT_A, INPUT_PULLUP);
    pinMode(PIN_LEFT_B, INPUT_PULLUP);
    pinMode(PIN_LEFT_Z, INPUT_PULLUP);

    // Configure RIGHT GPIOs with pull-ups
    pinMode(PIN_RIGHT_A, INPUT_PULLUP);
    pinMode(PIN_RIGHT_B, INPUT_PULLUP);
    pinMode(PIN_RIGHT_Z, INPUT_PULLUP);

    // Read initial states
    g_leftState.prevState  = (digitalRead(PIN_LEFT_A) << 1) | digitalRead(PIN_LEFT_B);
    g_rightState.prevState = (digitalRead(PIN_RIGHT_A) << 1) | digitalRead(PIN_RIGHT_B);

    // Attach LEFT interrupts
    attachInterrupt(digitalPinToInterrupt(PIN_LEFT_A), isrLeftPinChange, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_LEFT_B), isrLeftPinChange, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_LEFT_Z), isrLeftIndex, RISING);

    // Attach RIGHT interrupts
    attachInterrupt(digitalPinToInterrupt(PIN_RIGHT_A), isrRightPinChange, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_RIGHT_B), isrRightPinChange, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_RIGHT_Z), isrRightIndex, RISING);

    lastTelemetryTime   = millis();
    lastTelemetryCountL = 0;
    lastTelemetryCountR = 0;

    Serial.println("[INFO] Dual encoder drivers initialized successfully.");
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

        // Atomically snapshot LEFT interrupt counters
        noInterrupts();
        int64_t countL       = g_leftState.rawCount;
        uint32_t indexL      = g_leftState.indexCount;
        uint32_t invalidL    = g_leftState.invalidTransitions;

        int64_t countR       = g_rightState.rawCount;
        uint32_t indexR      = g_rightState.indexCount;
        uint32_t invalidR    = g_rightState.invalidTransitions;
        interrupts();

        // LEFT calculations
        int64_t deltaL = countL - lastTelemetryCountL;
        lastTelemetryCountL = countL;
        uint32_t absDeltaL = static_cast<uint32_t>(deltaL >= 0 ? deltaL : -deltaL);

        int dirL = 0;
        if (deltaL > 0) dirL = 1;
        else if (deltaL < 0) dirL = -1;

        float rpmL = 0.0f;
        if (elapsedMs > 0 && COUNTS_PER_REV > 0 && deltaL != 0) {
            rpmL = (static_cast<float>(deltaL) / static_cast<float>(COUNTS_PER_REV)) *
                   (60000.0f / static_cast<float>(elapsedMs));
        }

        // RIGHT calculations
        int64_t deltaR = countR - lastTelemetryCountR;
        lastTelemetryCountR = countR;
        uint32_t absDeltaR = static_cast<uint32_t>(deltaR >= 0 ? deltaR : -deltaR);

        int dirR = 0;
        if (deltaR > 0) dirR = 1;
        else if (deltaR < 0) dirR = -1;

        float rpmR = 0.0f;
        if (elapsedMs > 0 && COUNTS_PER_REV > 0 && deltaR != 0) {
            rpmR = (static_cast<float>(deltaR) / static_cast<float>(COUNTS_PER_REV)) *
                   (60000.0f / static_cast<float>(elapsedMs));
        }

        // Sensor status: 2 = OK
        int st = 2;

        // Standard AutoChair dual encoder diagnostic line format
        Serial.printf("[Encoder] L: cnt=%lld dir=%d rpm=%.2f pulses=%u idx=%u inv=%u st=%d\n",
                      static_cast<long long>(countL),
                      dirL,
                      static_cast<double>(rpmL),
                      static_cast<unsigned int>(absDeltaL),
                      static_cast<unsigned int>(indexL),
                      static_cast<unsigned int>(invalidL),
                      st);

        Serial.printf("[Encoder] R: cnt=%lld dir=%d rpm=%.2f pulses=%u idx=%u inv=%u st=%d\n",
                      static_cast<long long>(countR),
                      dirR,
                      static_cast<double>(rpmR),
                      static_cast<unsigned int>(absDeltaR),
                      static_cast<unsigned int>(indexR),
                      static_cast<unsigned int>(invalidR),
                      st);
    }
}
