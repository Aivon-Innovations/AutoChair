#include "state_machine.h"

SafetyStateMachine::SafetyStateMachine()
    : _state(State::BOOT),
      _mode(Mode::NONE),
      _latchedInSafe(false),
      _firstHeartbeatReceived(false),
      _lastHeartbeatMillis(0),
      _lastHeartbeatSeq(0),
      _missedHeartbeats(0),
      _staleHeartbeatCount(0),
      _duplicateCommandCount(0),
      _cmdCacheHead(0),
      _cmdCacheCount(0) {
    strncpy(_simulatedMotion, "STOP", sizeof(_simulatedMotion) - 1);
    _simulatedMotion[sizeof(_simulatedMotion) - 1] = '\0';
    for (size_t i = 0; i < RECENT_CMD_CACHE_SIZE; i++) {
        _cmdCache[i].msgId = 0;
    }
}

void SafetyStateMachine::init() {
    // Non-active startup state: IDLE
    _state = State::IDLE;
    _mode = Mode::NONE;
    _latchedInSafe = false;
    _firstHeartbeatReceived = false;
    _lastHeartbeatMillis = 0;
    _lastHeartbeatSeq = 0;
    _missedHeartbeats = 0;
    _staleHeartbeatCount = 0;
    _duplicateCommandCount = 0;
    _cmdCacheHead = 0;
    _cmdCacheCount = 0;
    strncpy(_simulatedMotion, "STOP", sizeof(_simulatedMotion) - 1);
    _simulatedMotion[sizeof(_simulatedMotion) - 1] = '\0';
    for (size_t i = 0; i < RECENT_CMD_CACHE_SIZE; i++) {
        _cmdCache[i].msgId = 0;
    }
}

bool SafetyStateMachine::isHeartbeatHealthy(uint32_t currentMillis) const {
    if (!_firstHeartbeatReceived || _lastHeartbeatMillis == 0) {
        return false;
    }
    // Unsigned 32-bit subtraction naturally handles millis() rollover
    return (uint32_t)(currentMillis - _lastHeartbeatMillis) <= HEARTBEAT_TIMEOUT_MS;
}

uint32_t SafetyStateMachine::getHeartbeatAgeMs(uint32_t currentMillis) const {
    if (!_firstHeartbeatReceived || _lastHeartbeatMillis == 0) {
        return 999999;
    }
    return (uint32_t)(currentMillis - _lastHeartbeatMillis);
}

bool SafetyStateMachine::recordHeartbeat(uint32_t currentMillis, uint32_t seq) {
    // Rule: First valid heartbeat establishes sequence baseline and starts watchdog
    if (!_firstHeartbeatReceived) {
        _firstHeartbeatReceived = true;
        _lastHeartbeatSeq = seq;
        _lastHeartbeatMillis = currentMillis;
        return true;
    }

    // Sequence Policy (RFC 1982 Serial Number Arithmetic):
    // For unsigned 32-bit integers, seq represents a valid forward progression if:
    // (uint32_t)(seq - _lastHeartbeatSeq) > 0 && (uint32_t)(seq - _lastHeartbeatSeq) < 0x80000000UL
    // This cleanly handles normal monotonic increments as well as 32-bit rollover
    // (e.g. wrapping from 0xFFFFFFFF to 0, 1...) without threshold magic numbers.
    // Duplicate: diff == 0
    // Stale/Out-of-order: diff >= 0x80000000UL
    uint32_t diff = (uint32_t)(seq - _lastHeartbeatSeq);
    bool isValidAdvance = (diff > 0) && (diff < 0x80000000UL);

    if (!isValidAdvance) {
        _staleHeartbeatCount++;
        // CRITICAL SAFETY RULE: Stale, duplicate, or out-of-order heartbeats MUST NOT refresh the watchdog!
        return false;
    }

    // Fresh advancing heartbeat sequence: refresh watchdog
    _lastHeartbeatSeq = seq;
    _lastHeartbeatMillis = currentMillis;
    // CRITICAL SAFETY RULE: Receiving fresh heartbeats alone in SAFE state does NOT auto-recover!
    return true;
}

void SafetyStateMachine::update(uint32_t currentMillis) {
    // Heartbeat monitoring active in READY and ACTIVE_TEST states
    if (_state == State::READY || _state == State::ACTIVE_TEST) {
        if (!isHeartbeatHealthy(currentMillis)) {
            // Heartbeat expired or never received! Trip immediately to latched SAFE state
            _state = State::SAFE;
            _latchedInSafe = true;
            _missedHeartbeats++;
            strncpy(_simulatedMotion, "STOP", sizeof(_simulatedMotion) - 1);
            _simulatedMotion[sizeof(_simulatedMotion) - 1] = '\0';
            sendEvent("HEARTBEAT_TIMEOUT", "Heartbeat interval exceeded 1500ms; tripped to latched SAFE state", _state);
        }
    }
}

bool SafetyStateMachine::findCachedCommand(uint32_t msgId, CachedCommand& outCmd) const {
    if (msgId == 0) return false;
    for (uint8_t i = 0; i < _cmdCacheCount; i++) {
        if (_cmdCache[i].msgId == msgId) {
            outCmd = _cmdCache[i];
            return true;
        }
    }
    return false;
}

void SafetyStateMachine::recordCachedCommand(uint32_t msgId, bool ack, const char* result, const char* reason) {
    if (msgId == 0) return;
    CachedCommand& slot = _cmdCache[_cmdCacheHead];
    slot.msgId = msgId;
    slot.ack = ack;
    strncpy(slot.result, result ? result : "", sizeof(slot.result) - 1);
    slot.result[sizeof(slot.result) - 1] = '\0';
    strncpy(slot.reason, reason ? reason : "", sizeof(slot.reason) - 1);
    slot.reason[sizeof(slot.reason) - 1] = '\0';
    slot.state = _state;
    slot.mode = _mode;

    _cmdCacheHead = (_cmdCacheHead + 1) % RECENT_CMD_CACHE_SIZE;
    if (_cmdCacheCount < RECENT_CMD_CACHE_SIZE) {
        _cmdCacheCount++;
    }
}

void SafetyStateMachine::handleCommand(uint32_t msgId, const char* cmd, JsonObject& payload, uint32_t currentMillis) {
    if (!cmd || strlen(cmd) == 0) {
        sendJsonResponse(msgId, false, "REJECTED", "MISSING_COMMAND_NAME", _state, _mode);
        return;
    }

    // Duplicate command detection via bounded cache
    CachedCommand cached;
    if (msgId != 0 && findCachedCommand(msgId, cached)) {
        _duplicateCommandCount++;
        // Echo cached response without repeating state changes or simulated motion
        sendJsonResponse(msgId, cached.ack, cached.result, "DUPLICATE_COMMAND_CACHED", _state, _mode);
        return;
    }

    // 1. PING (Allowed in all states)
    if (strcasecmp(cmd, "PING") == 0) {
        recordCachedCommand(msgId, true, "ACCEPTED", "PONG");
        sendJsonResponse(msgId, true, "ACCEPTED", "PONG", _state, _mode);
        return;
    }

    // 2. GET_STATUS (Allowed in all states)
    if (strcasecmp(cmd, "GET_STATUS") == 0) {
        recordCachedCommand(msgId, true, "ACCEPTED", "STATUS_OK");
        sendJsonResponse(msgId, true, "ACCEPTED", "STATUS_OK", _state, _mode);
        return;
    }

    // 3. SET_MODE
    if (strcasecmp(cmd, "SET_MODE") == 0) {
        if (_state == State::SAFE || _state == State::FAULT || _state == State::ACTIVE_TEST) {
            recordCachedCommand(msgId, false, "REJECTED", "CANNOT_CHANGE_MODE_IN_SAFE_FAULT_OR_ACTIVE");
            sendJsonResponse(msgId, false, "REJECTED", "CANNOT_CHANGE_MODE_IN_SAFE_FAULT_OR_ACTIVE", _state, _mode);
            return;
        }

        if (!payload.containsKey("mode") || !payload["mode"].is<const char*>()) {
            recordCachedCommand(msgId, false, "REJECTED", "MISSING_OR_INVALID_MODE_FIELD");
            sendJsonResponse(msgId, false, "REJECTED", "MISSING_OR_INVALID_MODE_FIELD", _state, _mode);
            return;
        }

        const char* modeStr = payload["mode"];
        if (strcasecmp(modeStr, "MANUAL") == 0) {
            _mode = Mode::MANUAL;
        } else if (strcasecmp(modeStr, "ASSISTED") == 0) {
            _mode = Mode::ASSISTED;
        } else if (strcasecmp(modeStr, "NONE") == 0) {
            _mode = Mode::NONE;
        } else {
            recordCachedCommand(msgId, false, "REJECTED", "INVALID_MODE_VALUE");
            sendJsonResponse(msgId, false, "REJECTED", "INVALID_MODE_VALUE", _state, _mode);
            return;
        }

        recordCachedCommand(msgId, true, "ACCEPTED", "MODE_UPDATED");
        sendJsonResponse(msgId, true, "ACCEPTED", "MODE_UPDATED", _state, _mode);
        return;
    }

    // 4. MOVE (Simulated motion command)
    if (strcasecmp(cmd, "MOVE") == 0) {
        // Enforce strict state gating
        if (_state == State::SAFE || _state == State::FAULT) {
            recordCachedCommand(msgId, false, "REJECTED", "SYSTEM_IN_SAFE_STATE");
            sendJsonResponse(msgId, false, "REJECTED", "SYSTEM_IN_SAFE_STATE", _state, _mode);
            return;
        }
        if (_state == State::IDLE || _state == State::BOOT) {
            recordCachedCommand(msgId, false, "REJECTED", "SYSTEM_NOT_ARMED_IDLE_STATE");
            sendJsonResponse(msgId, false, "REJECTED", "SYSTEM_NOT_ARMED_IDLE_STATE", _state, _mode);
            return;
        }

        if (!payload.containsKey("action") || !payload["action"].is<const char*>()) {
            recordCachedCommand(msgId, false, "REJECTED", "MISSING_OR_INVALID_ACTION_FIELD");
            sendJsonResponse(msgId, false, "REJECTED", "MISSING_OR_INVALID_ACTION_FIELD", _state, _mode);
            return;
        }

        // Validate optional speed_pct
        if (payload.containsKey("speed_pct")) {
            if (!payload["speed_pct"].is<int>() || payload["speed_pct"].is<bool>()) {
                recordCachedCommand(msgId, false, "REJECTED", "INVALID_SPEED_PCT_TYPE");
                sendJsonResponse(msgId, false, "REJECTED", "INVALID_SPEED_PCT_TYPE", _state, _mode);
                return;
            }
            int spd = payload["speed_pct"].as<int>();
            if (spd < 0 || spd > 100) {
                recordCachedCommand(msgId, false, "REJECTED", "SPEED_PCT_OUT_OF_RANGE");
                sendJsonResponse(msgId, false, "REJECTED", "SPEED_PCT_OUT_OF_RANGE", _state, _mode);
                return;
            }
        }

        const char* action = payload["action"];
        if (strcasecmp(action, "STOP") == 0) {
            strncpy(_simulatedMotion, "STOP", sizeof(_simulatedMotion) - 1);
            _simulatedMotion[sizeof(_simulatedMotion) - 1] = '\0';
            _state = State::READY;
            recordCachedCommand(msgId, true, "ACCEPTED", "MOTION_STOPPED");
            sendJsonResponse(msgId, true, "ACCEPTED", "MOTION_STOPPED", _state, _mode);
        } else if (strcasecmp(action, "TEST_FORWARD") == 0 ||
                   strcasecmp(action, "TEST_LEFT") == 0 ||
                   strcasecmp(action, "TEST_RIGHT") == 0) {
            strncpy(_simulatedMotion, action, sizeof(_simulatedMotion) - 1);
            _simulatedMotion[sizeof(_simulatedMotion) - 1] = '\0';
            _state = State::ACTIVE_TEST;
            recordCachedCommand(msgId, true, "ACCEPTED", "SIMULATED_MOTION_ACTIVE");
            sendJsonResponse(msgId, true, "ACCEPTED", "SIMULATED_MOTION_ACTIVE", _state, _mode);
        } else {
            recordCachedCommand(msgId, false, "REJECTED", "UNKNOWN_MOTION_ACTION");
            sendJsonResponse(msgId, false, "REJECTED", "UNKNOWN_MOTION_ACTION", _state, _mode);
        }
        return;
    }

    // 5. RESET / RECOVER / ARM (Explicit recovery or arming)
    if (strcasecmp(cmd, "RESET") == 0 || strcasecmp(cmd, "RECOVER") == 0 || strcasecmp(cmd, "ARM") == 0) {
        if (!_firstHeartbeatReceived) {
            recordCachedCommand(msgId, false, "REJECTED", "CANNOT_RESET_BEFORE_FIRST_HEARTBEAT");
            sendJsonResponse(msgId, false, "REJECTED", "CANNOT_RESET_BEFORE_FIRST_HEARTBEAT", _state, _mode);
            return;
        }

        if (!isHeartbeatHealthy(currentMillis)) {
            recordCachedCommand(msgId, false, "REJECTED", "CANNOT_RESET_WITHOUT_HEALTHY_HEARTBEAT");
            sendJsonResponse(msgId, false, "REJECTED", "CANNOT_RESET_WITHOUT_HEALTHY_HEARTBEAT", _state, _mode);
            return;
        }

        _latchedInSafe = false;
        _state = State::READY;
        strncpy(_simulatedMotion, "STOP", sizeof(_simulatedMotion) - 1);
        _simulatedMotion[sizeof(_simulatedMotion) - 1] = '\0';
        recordCachedCommand(msgId, true, "ACCEPTED", "SAFETY_LATCH_CLEARED_READY");
        sendJsonResponse(msgId, true, "ACCEPTED", "SAFETY_LATCH_CLEARED_READY", _state, _mode);
        sendEvent("RECOVERY_COMPLETE", "Safety latch cleared; system in READY", _state);
        return;
    }

    // 6. Unknown command rejection
    recordCachedCommand(msgId, false, "REJECTED", "UNKNOWN_COMMAND");
    sendJsonResponse(msgId, false, "REJECTED", "UNKNOWN_COMMAND", _state, _mode);
}
