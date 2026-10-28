#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#define PROTOCOL_VERSION 1
#define MAX_LINE_BUFFER_BYTES 512

enum class State {
    BOOT,
    IDLE,
    READY,
    ACTIVE_TEST,
    SAFE,
    FAULT
};

enum class Mode {
    NONE,
    MANUAL,
    ASSISTED
};

const char* stateToString(State s);
const char* modeToString(Mode m);
State stringToState(const char* str);
Mode stringToMode(const char* str);

class SerialFrameParser {
public:
    SerialFrameParser();
    bool feed(char c, String& completedLine);
    void reset();
    bool hasOverflow() const { return _overflow; }

private:
    char _buffer[MAX_LINE_BUFFER_BYTES];
    size_t _index;
    bool _overflow;
};

// Forward declaration of SafetyStateMachine to avoid circular dependency
class SafetyStateMachine;

void sendJsonResponse(uint32_t msgId, bool ack, const char* result, const char* reason, State currentState, Mode currentMode);
void sendTelemetry(State currentState, Mode currentMode, uint32_t uptimeMs, uint32_t seq, bool hbOk, uint32_t hbAgeMs, uint32_t missedHb, uint32_t malformedCount, const char* motionAction);
void sendEvent(const char* eventName, const char* description, State currentState);
void sendHeartbeatAck(uint32_t msgId, uint32_t hbSeq, bool accepted, const char* reason, State currentState);

// Central frame processor: parses incoming JSON line, validates required protocol fields,
// dispatches to state machine, and issues appropriate responses or error events.
void processIncomingLine(const char* line, uint32_t now, SafetyStateMachine& stateMachine, uint32_t& malformedCount, uint32_t& lastMalformedEventMillis);
