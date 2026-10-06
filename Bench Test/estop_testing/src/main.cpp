#include <Arduino.h>

#define ESTOP_PIN 32
#define DEBOUNCE_DELAY_MS 25

enum SafetyState {
    STATE_READY,
    STATE_SAFE
};

static SafetyState currentSafetyState = STATE_SAFE;
static int lastRawPinState = -1;
static int stablePinState = -1;
static unsigned long lastDebounceTime = 0;
static bool latchedInSafe = false;

const char* stateToString(SafetyState state) {
    switch (state) {
        case STATE_READY: return "READY";
        case STATE_SAFE:  return "SAFE";
        default:          return "UNKNOWN";
    }
}

void printStatus() {
    int raw = digitalRead(ESTOP_PIN);
    Serial.printf("[STATUS] SAFETY=%s | RAW_PIN=%d | STABLE_PIN=%d | LATCHED=%s | UPTIME_MS=%lu\n",
                  stateToString(currentSafetyState),
                  raw,
                  stablePinState,
                  latchedInSafe ? "YES" : "NO",
                  millis());
}

void processCommand(const String& cmd) {
    String trimmedCmd = cmd;
    trimmedCmd.trim();
    if (trimmedCmd.length() == 0) return;

    if (trimmedCmd.equalsIgnoreCase("CMD:STATUS") || trimmedCmd.equalsIgnoreCase("STATUS")) {
        printStatus();
    }
    else if (trimmedCmd.equalsIgnoreCase("CMD:MOVE_FORWARD") || trimmedCmd.equalsIgnoreCase("MOVE_FORWARD")) {
        if (currentSafetyState == STATE_READY) {
            Serial.printf("COMMAND = MOVE_FORWARD | SAFETY = READY | RESULT = ACCEPTED\n");
        } else {
            Serial.printf("COMMAND = MOVE_FORWARD | SAFETY = SAFE | RESULT = REJECTED\n");
        }
    }
    else if (trimmedCmd.equalsIgnoreCase("CMD:RESET") || trimmedCmd.equalsIgnoreCase("RESET") ||
             trimmedCmd.equalsIgnoreCase("CMD:RECOVER") || trimmedCmd.equalsIgnoreCase("RECOVER")) {
        int currentRaw = digitalRead(ESTOP_PIN);
        if (currentRaw == LOW && stablePinState == LOW) { // E-stop is physically released (NC closed to GND)
            currentSafetyState = STATE_READY;
            latchedInSafe = false;
            Serial.printf("[RESPONSE] RESET: SUCCESS | GPIO32=LOW | SAFETY = READY\n");
        } else {
            Serial.printf("[RESPONSE] RESET: REJECTED (ESTOP_ACTIVE_OR_OPEN) | GPIO32=%d | SAFETY = SAFE\n", currentRaw);
        }
    }
    else if (trimmedCmd.equalsIgnoreCase("CMD:PING") || trimmedCmd.equalsIgnoreCase("PING")) {
        Serial.printf("[PONG] ESP32_ESTOP_VALIDATION_ONLINE\n");
    }
    else {
        Serial.printf("[ERROR] UNKNOWN_COMMAND: %s\n", trimmedCmd.c_str());
    }
}

void setup() {
    Serial.begin(115200);
    // Give serial a moment
    delay(500);

    // E-stop terminal 11 -> GPIO32, terminal 12 -> GND
    // NC contact: Released = Closed (LOW), Pressed = Open (HIGH via pullup)
    pinMode(ESTOP_PIN, INPUT_PULLUP);

    // Initial read
    int initialRaw = digitalRead(ESTOP_PIN);
    stablePinState = initialRaw;
    lastRawPinState = initialRaw;

    if (initialRaw == LOW) {
        currentSafetyState = STATE_READY;
        latchedInSafe = false;
    } else {
        currentSafetyState = STATE_SAFE;
        latchedInSafe = true;
    }

    Serial.println("\n==========================================");
    Serial.println("ESP32 EMERGENCY-STOP BENCH VALIDATION FIRMWARE");
    Serial.printf("Pin: GPIO%d (INPUT_PULLUP)\n", ESTOP_PIN);
    Serial.printf("Initial State: %s (GPIO%d = %s)\n",
                  stateToString(currentSafetyState),
                  ESTOP_PIN,
                  initialRaw == LOW ? "LOW (RELEASED)" : "HIGH (PRESSED/OPEN)");
    Serial.println("==========================================");
    printStatus();
}

void loop() {
    // 1. Process Serial Commands
    while (Serial.available() > 0) {
        String inputLine = Serial.readStringUntil('\n');
        processCommand(inputLine);
    }

    // 2. Read GPIO and Debounce
    int rawReading = digitalRead(ESTOP_PIN);
    if (rawReading != lastRawPinState) {
        lastDebounceTime = millis();
        lastRawPinState = rawReading;
    }

    if ((millis() - lastDebounceTime) >= DEBOUNCE_DELAY_MS) {
        if (rawReading != stablePinState) {
            stablePinState = rawReading;
            
            // Physical state changed
            if (stablePinState == HIGH) {
                // E-stop PRESSED or Wire DISCONNECTED (Open circuit -> HIGH)
                currentSafetyState = STATE_SAFE;
                latchedInSafe = true;
                Serial.printf("[EVENT] ESTOP_TRIPPED | GPIO32=HIGH (OPEN/PRESSED) | SAFETY = SAFE | TIMESTAMP_MS=%lu\n", millis());
            } else {
                // E-stop RELEASED (Closed circuit to GND -> LOW)
                // MUST REMAIN IN SAFE (LATCHED) until explicit software reset!
                Serial.printf("[EVENT] ESTOP_PHYSICAL_RELEASED | GPIO32=LOW (CLOSED/RELEASED) | SAFETY = %s (LATCHED=%s) | TIMESTAMP_MS=%lu\n",
                              stateToString(currentSafetyState),
                              latchedInSafe ? "YES" : "NO",
                              millis());
            }
        }
    }

    delay(2);
}
