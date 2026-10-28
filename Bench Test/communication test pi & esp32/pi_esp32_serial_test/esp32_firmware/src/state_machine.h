#pragma once

#include "protocol.h"

// Bounded recent command cache size for duplicate detection
#define RECENT_CMD_CACHE_SIZE 16

struct CachedCommand {
    uint32_t msgId;
    bool ack;
    char result[16];
    char reason[48];
    State state;
    Mode mode;
};

class SafetyStateMachine {
public:
    SafetyStateMachine();

    void init();
    void update(uint32_t currentMillis);

    State getState() const { return _state; }
    Mode getMode() const { return _mode; }
    bool isLatchedInSafe() const { return _latchedInSafe; }
    bool hasReceivedFirstHeartbeat() const { return _firstHeartbeatReceived; }
    bool isHeartbeatHealthy(uint32_t currentMillis) const;
    uint32_t getHeartbeatAgeMs(uint32_t currentMillis) const;
    uint32_t getLastHeartbeatSeq() const { return _lastHeartbeatSeq; }
    uint32_t getMissedHeartbeats() const { return _missedHeartbeats; }
    uint32_t getDuplicateCommandCount() const { return _duplicateCommandCount; }
    uint32_t getStaleHeartbeatCount() const { return _staleHeartbeatCount; }
    const char* getSimulatedMotion() const { return _simulatedMotion; }

    // Sequence Policy (RFC 1982 Serial Number Arithmetic):
    // Returns true if seq represents a fresh, advancing sequence number.
    // Duplicate, out-of-order, or stale sequences return false and DO NOT refresh the watchdog.
    bool recordHeartbeat(uint32_t currentMillis, uint32_t seq);
    void handleCommand(uint32_t msgId, const char* commandName, JsonObject& payload, uint32_t currentMillis);

private:
    State _state;
    Mode _mode;
    bool _latchedInSafe;
    bool _firstHeartbeatReceived;
    uint32_t _lastHeartbeatMillis;
    uint32_t _lastHeartbeatSeq;
    uint32_t _missedHeartbeats;
    uint32_t _staleHeartbeatCount;
    uint32_t _duplicateCommandCount;
    char _simulatedMotion[32];

    // Bounded FIFO circular cache for duplicate command suppression
    CachedCommand _cmdCache[RECENT_CMD_CACHE_SIZE];
    uint8_t _cmdCacheHead;
    uint8_t _cmdCacheCount;

    bool findCachedCommand(uint32_t msgId, CachedCommand& outCmd) const;
    void recordCachedCommand(uint32_t msgId, bool ack, const char* result, const char* reason);

    static constexpr uint32_t HEARTBEAT_TIMEOUT_MS = 1500;
};
