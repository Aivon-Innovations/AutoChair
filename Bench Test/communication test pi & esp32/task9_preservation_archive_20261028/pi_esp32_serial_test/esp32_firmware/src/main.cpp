#include <Arduino.h>
#include <ArduinoJson.h>
#include "protocol.h"
#include "state_machine.h"

static SerialFrameParser frameParser;
static SafetyStateMachine stateMachine;

static uint32_t lastTelemetryMillis = 0;
static uint32_t telemetrySequence = 0;
static constexpr uint32_t TELEMETRY_INTERVAL_MS = 500;

// Diagnostics & event flood protection
static uint32_t malformedFrameCount = 0;
static uint32_t lastMalformedEventMillis = 0;

void setup() {
    Serial.begin(115200);
    delay(500);

    stateMachine.init();

    sendEvent("SYSTEM_BOOT", "AutoChair Standalone USB Serial Firmware Initialized in IDLE", stateMachine.getState());
}

void loop() {
    uint32_t now = millis();

    // 1. Process incoming serial bytes via frame parser and central frame processor
    while (Serial.available() > 0) {
        char c = (char)Serial.read();
        String completeLine;
        if (frameParser.feed(c, completeLine)) {
            processIncomingLine(completeLine.c_str(), now, stateMachine, malformedFrameCount, lastMalformedEventMillis);
        }
    }

    // 2. State machine cyclic safety check (heartbeat timeout)
    stateMachine.update(now);

    // 3. Periodic telemetry transmission
    if (now - lastTelemetryMillis >= TELEMETRY_INTERVAL_MS) {
        lastTelemetryMillis = now;
        telemetrySequence++;

        sendTelemetry(
            stateMachine.getState(),
            stateMachine.getMode(),
            now,
            telemetrySequence,
            stateMachine.isHeartbeatHealthy(now),
            stateMachine.getHeartbeatAgeMs(now),
            stateMachine.getMissedHeartbeats(),
            malformedFrameCount,
            stateMachine.getSimulatedMotion()
        );
    }

    delay(2);
}
