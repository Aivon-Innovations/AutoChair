#include "protocol.h"
#include "state_machine.h"

const char* stateToString(State s) {
    switch (s) {
        case State::BOOT:        return "BOOT";
        case State::IDLE:        return "IDLE";
        case State::READY:       return "READY";
        case State::ACTIVE_TEST: return "ACTIVE_TEST";
        case State::SAFE:        return "SAFE";
        case State::FAULT:       return "FAULT";
        default:                 return "UNKNOWN";
    }
}

const char* modeToString(Mode m) {
    switch (m) {
        case Mode::NONE:     return "NONE";
        case Mode::MANUAL:   return "MANUAL";
        case Mode::ASSISTED: return "ASSISTED";
        default:             return "UNKNOWN";
    }
}

State stringToState(const char* str) {
    if (!str) return State::FAULT;
    if (strcmp(str, "BOOT") == 0) return State::BOOT;
    if (strcmp(str, "IDLE") == 0) return State::IDLE;
    if (strcmp(str, "READY") == 0) return State::READY;
    if (strcmp(str, "ACTIVE_TEST") == 0) return State::ACTIVE_TEST;
    if (strcmp(str, "SAFE") == 0) return State::SAFE;
    if (strcmp(str, "FAULT") == 0) return State::FAULT;
    return State::FAULT;
}

Mode stringToMode(const char* str) {
    if (!str) return Mode::NONE;
    if (strcmp(str, "MANUAL") == 0) return Mode::MANUAL;
    if (strcmp(str, "ASSISTED") == 0) return Mode::ASSISTED;
    return Mode::NONE;
}

SerialFrameParser::SerialFrameParser() : _index(0), _overflow(false) {}

void SerialFrameParser::reset() {
    _index = 0;
    _overflow = false;
}

bool SerialFrameParser::feed(char c, String& completedLine) {
    if (c == '\r') {
        return false; // Ignore carriage return
    }

    if (c == '\n') {
        if (_overflow) {
            // End of an oversized frame that was discarded; reset buffer cleanly for the next frame
            reset();
            return false;
        }

        if (_index == 0) {
            return false; // Empty line
        }

        _buffer[_index] = '\0';
        completedLine = String(_buffer);
        reset();
        return true;
    }

    if (_overflow) {
        // Still discarding characters until newline
        return false;
    }

    if (_index >= (MAX_LINE_BUFFER_BYTES - 1)) {
        // Frame exceeded maximum allowed bytes! Discard buffer and flag overflow
        _overflow = true;
        _index = 0;
        return false;
    }

    _buffer[_index++] = c;
    return false;
}

void sendJsonResponse(uint32_t msgId, bool ack, const char* result, const char* reason, State currentState, Mode currentMode) {
    StaticJsonDocument<512> doc;
    doc["version"] = PROTOCOL_VERSION;
    doc["msg_type"] = "RESPONSE";
    doc["msg_id"] = msgId;
    doc["timestamp_ms"] = millis();

    JsonObject payload = doc.createNestedObject("payload");
    payload["ack"] = ack;
    payload["result"] = result;
    if (reason && strlen(reason) > 0) {
        payload["reason"] = reason;
    }
    payload["state"] = stateToString(currentState);
    payload["mode"] = modeToString(currentMode);

    serializeJson(doc, Serial);
    Serial.print('\n');
}

void sendTelemetry(State currentState, Mode currentMode, uint32_t uptimeMs, uint32_t seq, bool hbOk, uint32_t hbAgeMs, uint32_t missedHb, uint32_t malformedCount, const char* motionAction) {
    StaticJsonDocument<512> doc;
    doc["version"] = PROTOCOL_VERSION;
    doc["msg_type"] = "TELEMETRY";
    doc["msg_id"] = seq;
    doc["timestamp_ms"] = uptimeMs;

    JsonObject payload = doc.createNestedObject("payload");
    payload["state"] = stateToString(currentState);
    payload["mode"] = modeToString(currentMode);
    payload["uptime_ms"] = uptimeMs;
    payload["telemetry_seq"] = seq;

    JsonObject hb = payload.createNestedObject("heartbeat");
    hb["ok"] = hbOk;
    hb["last_received_age_ms"] = hbAgeMs;
    hb["missed_count"] = missedHb;

    JsonObject sensors = payload.createNestedObject("sensors");
    sensors["ultrasonic"] = "NOT_CONNECTED";
    sensors["imu"] = "NOT_CONNECTED";
    sensors["encoders"] = "NOT_CONNECTED";

    JsonObject diag = payload.createNestedObject("diagnostics");
    diag["malformed_frames"] = malformedCount;

    payload["simulated_motion"] = motionAction ? motionAction : "STOP";

    serializeJson(doc, Serial);
    Serial.print('\n');
}

void sendEvent(const char* eventName, const char* description, State currentState) {
    StaticJsonDocument<512> doc;
    doc["version"] = PROTOCOL_VERSION;
    doc["msg_type"] = "EVENT";
    doc["msg_id"] = 0;
    doc["timestamp_ms"] = millis();

    JsonObject payload = doc.createNestedObject("payload");
    payload["event"] = eventName;
    payload["description"] = description;
    payload["state"] = stateToString(currentState);

    serializeJson(doc, Serial);
    Serial.print('\n');
}

void sendHeartbeatAck(uint32_t msgId, uint32_t hbSeq, bool accepted, const char* reason, State currentState) {
    StaticJsonDocument<512> doc;
    doc["version"] = PROTOCOL_VERSION;
    doc["msg_type"] = "HEARTBEAT";
    doc["msg_id"] = msgId;
    doc["timestamp_ms"] = millis();

    JsonObject payload = doc.createNestedObject("payload");
    payload["seq"] = hbSeq;
    payload["ack"] = accepted;
    payload["status"] = accepted ? "ACCEPTED" : "REJECTED";
    payload["reason"] = reason ? reason : (accepted ? "HEARTBEAT_OK" : "STALE_OR_DUPLICATE_HEARTBEAT");
    payload["state"] = stateToString(currentState);

    serializeJson(doc, Serial);
    Serial.print('\n');
}

void processIncomingLine(const char* line, uint32_t now, SafetyStateMachine& stateMachine, uint32_t& malformedCount, uint32_t& lastMalformedEventMillis) {
    if (!line || strlen(line) == 0) return;

    StaticJsonDocument<768> doc;
    DeserializationError err = deserializeJson(doc, line);

    if (err) {
        malformedCount++;
        if (now - lastMalformedEventMillis >= 500) {
            lastMalformedEventMillis = now;
            sendEvent("MALFORMED_FRAME", err.c_str(), stateMachine.getState());
        }
        return;
    }

    // Strict validation: root must be JSON object
    if (!doc.is<JsonObject>()) {
        malformedCount++;
        return;
    }

    // Strict validation: version must be integer and equal PROTOCOL_VERSION
    if (!doc.containsKey("version") || !doc["version"].is<int>() || doc["version"].is<bool>() || doc["version"].as<int>() != PROTOCOL_VERSION) {
        uint32_t msgId = (doc.containsKey("msg_id") && doc["msg_id"].is<uint32_t>() && !doc["msg_id"].is<bool>()) ? doc["msg_id"].as<uint32_t>() : 0;
        sendJsonResponse(msgId, false, "REJECTED", "UNSUPPORTED_OR_INVALID_PROTOCOL_VERSION", stateMachine.getState(), stateMachine.getMode());
        return;
    }

    // Strict validation: msg_id must be non-negative integer
    if (!doc.containsKey("msg_id") || !doc["msg_id"].is<uint32_t>() || doc["msg_id"].is<bool>()) {
        sendJsonResponse(0, false, "REJECTED", "INVALID_MSG_ID_TYPE", stateMachine.getState(), stateMachine.getMode());
        return;
    }
    uint32_t msgId = doc["msg_id"].as<uint32_t>();

    // Strict validation: timestamp_ms must be non-negative integer
    if (!doc.containsKey("timestamp_ms") || !doc["timestamp_ms"].is<uint32_t>() || doc["timestamp_ms"].is<bool>()) {
        sendJsonResponse(msgId, false, "REJECTED", "INVALID_TIMESTAMP_TYPE", stateMachine.getState(), stateMachine.getMode());
        return;
    }

    // Strict validation: msg_type must be string
    if (!doc.containsKey("msg_type") || !doc["msg_type"].is<const char*>()) {
        sendJsonResponse(msgId, false, "REJECTED", "MISSING_OR_INVALID_MSG_TYPE", stateMachine.getState(), stateMachine.getMode());
        return;
    }
    const char* msgType = doc["msg_type"];

    // Strict validation: payload must be object
    if (!doc.containsKey("payload") || !doc["payload"].is<JsonObject>()) {
        sendJsonResponse(msgId, false, "REJECTED", "PAYLOAD_MUST_BE_OBJECT", stateMachine.getState(), stateMachine.getMode());
        return;
    }
    JsonObject payload = doc["payload"];

    if (strcasecmp(msgType, "HEARTBEAT") == 0) {
        if (!payload.containsKey("seq") || !payload["seq"].is<uint32_t>() || payload["seq"].is<bool>()) {
            sendJsonResponse(msgId, false, "REJECTED", "INVALID_HEARTBEAT_SEQ_TYPE", stateMachine.getState(), stateMachine.getMode());
            return;
        }
        uint32_t seq = payload["seq"].as<uint32_t>();
        bool accepted = stateMachine.recordHeartbeat(now, seq);
        sendHeartbeatAck(msgId, seq, accepted, accepted ? "HEARTBEAT_OK" : "STALE_OR_DUPLICATE_HEARTBEAT", stateMachine.getState());
    }
    else if (strcasecmp(msgType, "COMMAND") == 0) {
        if (!payload.containsKey("command") || !payload["command"].is<const char*>()) {
            sendJsonResponse(msgId, false, "REJECTED", "MISSING_OR_INVALID_COMMAND_NAME", stateMachine.getState(), stateMachine.getMode());
            return;
        }
        const char* cmdName = payload["command"];
        stateMachine.handleCommand(msgId, cmdName, payload, now);
    }
    else {
        sendJsonResponse(msgId, false, "REJECTED", "UNSUPPORTED_MSG_TYPE", stateMachine.getState(), stateMachine.getMode());
    }
}
