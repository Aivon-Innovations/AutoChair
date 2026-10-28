#include <cassert>
#include <iostream>
#include "arduino_mock.h"

// Define Arduino.h redirection to our mock
#define Arduino_h
#include "arduino_mock.h"

#include "../esp32_firmware/src/protocol.h"
#include "../esp32_firmware/src/state_machine.h"

int tests_passed = 0;

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "FAILED: " << msg << " (" << #cond << ") at line " << __LINE__ << std::endl; \
            return 1; \
        } else { \
            tests_passed++; \
        } \
    } while(0)

int main() {
    std::cout << "--- Running Native C++ Safety State Machine & Protocol Tests (Stage A.3) ---" << std::endl;

    SafetyStateMachine sm;
    sm.init();

    // 1. Startup in non-active IDLE state
    TEST_ASSERT(sm.getState() == State::IDLE, "Startup state must be IDLE");
    TEST_ASSERT(!sm.isLatchedInSafe(), "Must not be latched on startup");
    TEST_ASSERT(!sm.hasReceivedFirstHeartbeat(), "Must not have received first heartbeat on startup");
    TEST_ASSERT(!sm.isHeartbeatHealthy(1000), "Heartbeat must NOT be healthy before first heartbeat arrives");
    TEST_ASSERT(std::string(sm.getSimulatedMotion()) == "STOP", "Motion must be STOP on startup");

    // 2. Fix 1: Arming / RESET before first heartbeat must be REJECTED
    {
        StaticJsonDocument<256> doc;
        JsonObject p = doc.as<JsonObject>();
        Serial.clear();
        sm.handleCommand(101, "RESET", p, 1000);
        TEST_ASSERT(sm.getState() == State::IDLE, "State must remain IDLE when RESET attempted before first heartbeat");
        TEST_ASSERT(Serial.buffer.find("CANNOT_RESET_BEFORE_FIRST_HEARTBEAT") != std::string::npos, "Reason must state cannot reset before first heartbeat");
    }

    // 3. Fix 1: MOVE rejected in IDLE
    {
        StaticJsonDocument<256> doc;
        doc["action"] = "TEST_FORWARD";
        JsonObject p = doc.as<JsonObject>();
        Serial.clear();
        sm.handleCommand(102, "MOVE", p, 1000);
        TEST_ASSERT(sm.getState() == State::IDLE, "MOVE must not activate in IDLE");
        TEST_ASSERT(Serial.buffer.find("SYSTEM_NOT_ARMED_IDLE_STATE") != std::string::npos, "Rejection reason must be IDLE");
    }

    // 4. Fix 1 & 2: First valid heartbeat arrives
    {
        bool accepted = sm.recordHeartbeat(1000, 1);
        TEST_ASSERT(accepted, "First heartbeat must be accepted");
        TEST_ASSERT(sm.hasReceivedFirstHeartbeat(), "firstHeartbeatReceived must now be true");
        TEST_ASSERT(sm.isHeartbeatHealthy(1000), "Heartbeat must now be healthy");
        TEST_ASSERT(sm.getLastHeartbeatSeq() == 1, "Last seq must be 1");
    }

    // 5. Fix 1: Arming IDLE -> READY succeeds now that first heartbeat is healthy
    {
        StaticJsonDocument<256> doc;
        JsonObject p = doc.as<JsonObject>();
        Serial.clear();
        sm.handleCommand(103, "RESET", p, 1000);
        TEST_ASSERT(sm.getState() == State::READY, "System must arm to READY upon valid RESET with fresh heartbeat");
        TEST_ASSERT(Serial.buffer.find("SAFETY_LATCH_CLEARED_READY") != std::string::npos, "Arm response expected");
    }

    // 6. In READY: MOVE command enters ACTIVE_TEST
    {
        StaticJsonDocument<256> doc;
        doc["action"] = "TEST_FORWARD";
        doc["speed_pct"] = 60;
        JsonObject p = doc.as<JsonObject>();
        Serial.clear();
        sm.handleCommand(104, "MOVE", p, 1000);
        TEST_ASSERT(sm.getState() == State::ACTIVE_TEST, "Must enter ACTIVE_TEST on valid movement");
        TEST_ASSERT(std::string(sm.getSimulatedMotion()) == "TEST_FORWARD", "Simulated motion must be TEST_FORWARD");
    }

    // 7. MOVE STOP returns to READY
    {
        StaticJsonDocument<256> doc;
        doc["action"] = "STOP";
        JsonObject p = doc.as<JsonObject>();
        Serial.clear();
        sm.handleCommand(105, "MOVE", p, 1000);
        TEST_ASSERT(sm.getState() == State::READY, "Must return to READY on STOP action");
        TEST_ASSERT(std::string(sm.getSimulatedMotion()) == "STOP", "Simulated motion must be STOP");
    }

    // 8. Fix 2: Stale/duplicate heartbeat sequence validation
    {
        // Advance time to 1200 ms, try sending duplicate seq = 1
        bool acceptedDup = sm.recordHeartbeat(1200, 1);
        TEST_ASSERT(!acceptedDup, "Duplicate heartbeat seq=1 must be rejected");
        TEST_ASSERT(sm.getStaleHeartbeatCount() == 1, "Stale heartbeat count must increment");
        TEST_ASSERT(sm.getHeartbeatAgeMs(1200) == 200, "Duplicate seq must NOT refresh the watchdog timestamp");

        // Try sending out-of-order seq = 0
        bool acceptedStale = sm.recordHeartbeat(1250, 0);
        TEST_ASSERT(!acceptedStale, "Out-of-order heartbeat seq=0 must be rejected");
        TEST_ASSERT(sm.getStaleHeartbeatCount() == 2, "Stale heartbeat count must increment");
        TEST_ASSERT(sm.getHeartbeatAgeMs(1250) == 250, "Out-of-order seq must NOT refresh the watchdog timestamp");
    }

    // 9. Fix 2: Proof that duplicate heartbeats CANNOT keep system healthy indefinitely
    {
        // Keep sending duplicate seq = 1 at t = 2600 ms (elapsed > 1500 ms from t=1000)
        sm.recordHeartbeat(2600, 1);
        sm.update(2600);
        TEST_ASSERT(sm.getState() == State::SAFE, "State must trip to SAFE because duplicate sequence did not refresh watchdog");
        TEST_ASSERT(sm.isLatchedInSafe(), "System must latch in SAFE");
        TEST_ASSERT(sm.getMissedHeartbeats() == 1, "Missed heartbeat counter must increment");
    }

    // 10. Rejection of MOVE in SAFE state
    {
        StaticJsonDocument<256> doc;
        doc["action"] = "TEST_FORWARD";
        JsonObject p = doc.as<JsonObject>();
        Serial.clear();
        sm.handleCommand(106, "MOVE", p, 2600);
        TEST_ASSERT(sm.getState() == State::SAFE, "State must remain SAFE");
        TEST_ASSERT(Serial.buffer.find("SYSTEM_IN_SAFE_STATE") != std::string::npos, "Rejection reason must be SYSTEM_IN_SAFE_STATE");
    }

    // 11. Resuming heartbeats with valid sequence updates watchdog but DOES NOT auto-recover SAFE latch
    {
        bool acceptedFresh = sm.recordHeartbeat(2650, 2);
        TEST_ASSERT(acceptedFresh, "Fresh monotonic seq=2 must be accepted");
        sm.update(2650);
        TEST_ASSERT(sm.getState() == State::SAFE, "Heartbeat resumption alone must NOT clear latched SAFE state");
        TEST_ASSERT(sm.isLatchedInSafe(), "Latch must remain held");
    }

    // 12. Explicit RESET while heartbeat is healthy recovers to READY
    {
        StaticJsonDocument<256> doc;
        JsonObject p = doc.as<JsonObject>();
        Serial.clear();
        sm.handleCommand(107, "RESET", p, 2700);
        TEST_ASSERT(sm.getState() == State::READY, "State must be READY after explicit RESET");
        TEST_ASSERT(!sm.isLatchedInSafe(), "Latch must be cleared");
        TEST_ASSERT(Serial.buffer.find("SAFETY_LATCH_CLEARED_READY") != std::string::npos, "Success reason expected");
    }

    // 13. Stale heartbeat prevents recovery
    {
        // Trip back to safe at t = 4250 (elapsed 1550 ms since last hb at 2700)
        sm.update(4250);
        TEST_ASSERT(sm.getState() == State::SAFE, "Must trip to SAFE");

        // Attempt recovery without refreshing heartbeat
        StaticJsonDocument<256> doc;
        JsonObject p = doc.as<JsonObject>();
        Serial.clear();
        sm.handleCommand(108, "RESET", p, 4250);
        TEST_ASSERT(sm.getState() == State::SAFE, "State must remain SAFE");
        TEST_ASSERT(Serial.buffer.find("CANNOT_RESET_WITHOUT_HEALTHY_HEARTBEAT") != std::string::npos, "Rejection reason expected");

        // Now supply fresh heartbeat and recover cleanly
        sm.recordHeartbeat(4260, 3);
        Serial.clear();
        sm.handleCommand(109, "RESET", p, 4260);
        TEST_ASSERT(sm.getState() == State::READY, "Must recover to READY once fresh heartbeat is supplied");
    }

    // 14. Fix 3: Bounded Duplicate Command Cache
    {
        // Send a unique MOVE command (STOP) with msgId = 500
        StaticJsonDocument<256> doc;
        doc["action"] = "STOP";
        JsonObject p = doc.as<JsonObject>();
        Serial.clear();
        sm.handleCommand(500, "MOVE", p, 4300);
        TEST_ASSERT(Serial.buffer.find("MOTION_STOPPED") != std::string::npos, "First execution accepted");

        // Send immediate duplicate msgId = 500
        Serial.clear();
        sm.handleCommand(500, "MOVE", p, 4300);
        TEST_ASSERT(Serial.buffer.find("DUPLICATE_COMMAND_CACHED") != std::string::npos, "Immediate duplicate must be flagged from cache");
        TEST_ASSERT(sm.getDuplicateCommandCount() == 1, "Duplicate count must increment");

        // Send 15 intervening unique commands (id 501 to 515)
        for (uint32_t i = 501; i <= 515; i++) {
            StaticJsonDocument<256> d;
            JsonObject pj = d.as<JsonObject>();
            sm.handleCommand(i, "PING", pj, 4300);
        }

        // msgId = 500 is still in the 16-slot cache
        Serial.clear();
        sm.handleCommand(500, "MOVE", p, 4300);
        TEST_ASSERT(Serial.buffer.find("DUPLICATE_COMMAND_CACHED") != std::string::npos, "msgId 500 still recognized as duplicate after 15 intervening commands");

        // Now add one more command (id = 516) to evict msgId = 500
        {
            StaticJsonDocument<256> d;
            JsonObject pj = d.as<JsonObject>();
            sm.handleCommand(516, "PING", pj, 4300);
        }

        // msgId = 500 should now be evicted from bounded cache
        Serial.clear();
        sm.handleCommand(500, "MOVE", p, 4300);
        TEST_ASSERT(Serial.buffer.find("MOTION_STOPPED") != std::string::npos, "msgId 500 re-executed fresh after cache eviction");
    }

    // 15. Fix 2: Rollover Safety Check (millis wrapping around 0xFFFFFFFF)
    {
        uint32_t t_pre = 0xFFFFFFF0; // 16 ms before rollover
        uint32_t t_post = 0x00000010; // 16 ms after rollover
        sm.recordHeartbeat(t_pre, 9999);

        TEST_ASSERT(sm.isHeartbeatHealthy(t_post), "Heartbeat must be healthy across millis() rollover");
        TEST_ASSERT(sm.getHeartbeatAgeMs(t_post) == 32, "Heartbeat age must correctly calculate as 32 ms across rollover");
    }

    // 16. Fix 2: RFC 1982 Sequence Number Rollover Suite
    {
        SafetyStateMachine smRoll;
        smRoll.init();
        // Baseline near rollover boundary
        TEST_ASSERT(smRoll.recordHeartbeat(1000, 0xFFFFFFFA), "Baseline near boundary accepted");
        TEST_ASSERT(smRoll.getLastHeartbeatSeq() == 0xFFFFFFFA, "Baseline seq matches");

        // Normal progression approaching 0xFFFFFFFF
        TEST_ASSERT(smRoll.recordHeartbeat(1250, 0xFFFFFFFF), "Progression to 0xFFFFFFFF accepted");
        TEST_ASSERT(smRoll.getLastHeartbeatSeq() == 0xFFFFFFFF, "Last seq is 0xFFFFFFFF");

        // Duplicate at 0xFFFFFFFF
        TEST_ASSERT(!smRoll.recordHeartbeat(1500, 0xFFFFFFFF), "Duplicate at rollover boundary rejected");
        TEST_ASSERT(smRoll.getStaleHeartbeatCount() == 1, "Stale count incremented");

        // Rollover advance to 0 (0 - 0xFFFFFFFF = 1 step)
        TEST_ASSERT(smRoll.recordHeartbeat(1750, 0), "Rollover advance to 0 accepted");
        TEST_ASSERT(smRoll.getLastHeartbeatSeq() == 0, "Last seq updated to 0");

        // Rollover advance to 10
        TEST_ASSERT(smRoll.recordHeartbeat(2000, 10), "Advance to 10 accepted");
        TEST_ASSERT(smRoll.getLastHeartbeatSeq() == 10, "Last seq updated to 10");

        // Stale sequence from before rollover (0xFFFFFFFE)
        TEST_ASSERT(!smRoll.recordHeartbeat(2250, 0xFFFFFFFE), "Stale seq from before rollover rejected");
        TEST_ASSERT(smRoll.getStaleHeartbeatCount() == 2, "Stale count incremented");
    }

    // 17. SerialFrameParser Oversized Frame Discard & Next Frame Recovery
    {
        SerialFrameParser parser;
        String line;
        // Feed 600 'A' characters (exceeds 512 bytes limit)
        for (int i = 0; i < 600; i++) {
            parser.feed('A', line);
        }
        TEST_ASSERT(parser.hasOverflow(), "Parser must flag overflow for frame > 512 bytes");
        // Terminate oversized frame with newline
        bool gotOversized = parser.feed('\n', line);
        TEST_ASSERT(!gotOversized, "Oversized frame must be cleanly discarded");
        TEST_ASSERT(!parser.hasOverflow(), "Overflow flag must be reset after newline");

        // Immediately feed a valid JSON frame
        const char* valid = "{\"version\":1,\"msg_type\":\"COMMAND\",\"msg_id\":99,\"timestamp_ms\":100,\"payload\":{\"command\":\"PING\"}}\n";
        bool gotValid = false;
        for (const char* p = valid; *p; p++) {
            if (parser.feed(*p, line)) {
                gotValid = true;
            }
        }
        TEST_ASSERT(gotValid, "Valid frame immediately following oversized frame must be successfully extracted");
        TEST_ASSERT(line.find("\"msg_id\":99") != std::string::npos, "Extracted line content must match valid frame");
    }

    // 18. Heartbeat Acknowledgement Framing: Accepted vs Rejected
    {
        Serial.clear();
        sendHeartbeatAck(201, 15, true, "HEARTBEAT_OK", State::READY);
        TEST_ASSERT(Serial.buffer.find("\"msg_type\":\"HEARTBEAT\"") != std::string::npos, "Ack must have msg_type HEARTBEAT");
        TEST_ASSERT(Serial.buffer.find("\"seq\":15") != std::string::npos, "Ack must contain seq");
        TEST_ASSERT(Serial.buffer.find("\"ack\":true") != std::string::npos, "Accepted ack must have ack:true");
        TEST_ASSERT(Serial.buffer.find("\"status\":\"ACCEPTED\"") != std::string::npos, "Accepted ack must have status ACCEPTED");
        TEST_ASSERT(Serial.buffer.find("\"reason\":\"HEARTBEAT_OK\"") != std::string::npos, "Accepted ack reason check");

        Serial.clear();
        sendHeartbeatAck(202, 15, false, "STALE_OR_DUPLICATE_HEARTBEAT", State::READY);
        TEST_ASSERT(Serial.buffer.find("\"ack\":false") != std::string::npos, "Rejected ack must have ack:false");
        TEST_ASSERT(Serial.buffer.find("\"status\":\"REJECTED\"") != std::string::npos, "Rejected ack must have status REJECTED");
        TEST_ASSERT(Serial.buffer.find("\"reason\":\"STALE_OR_DUPLICATE_HEARTBEAT\"") != std::string::npos, "Rejected ack reason check");
    }

    // 19. Comprehensive processIncomingLine Protocol & Field Validation Suite
    {
        SafetyStateMachine smTest;
        smTest.init();
        uint32_t malformedCount = 0;
        uint32_t lastMalformedEvent = 0;

        // A. Malformed non-JSON
        Serial.clear();
        processIncomingLine("not json at all", 1000, smTest, malformedCount, lastMalformedEvent);
        TEST_ASSERT(malformedCount == 1, "Malformed frame counter must increment");
        TEST_ASSERT(Serial.buffer.find("MALFORMED_FRAME") != std::string::npos, "MALFORMED_FRAME event must be emitted");

        // B. Non-object JSON root (array)
        Serial.clear();
        processIncomingLine("[1, 2, 3]", 1000, smTest, malformedCount, lastMalformedEvent);
        TEST_ASSERT(malformedCount == 2, "Array root must increment malformed counter");

        // C. Boolean version rejected
        Serial.clear();
        processIncomingLine("{\"version\":true,\"msg_type\":\"COMMAND\",\"msg_id\":1,\"timestamp_ms\":100,\"payload\":{}}", 1000, smTest, malformedCount, lastMalformedEvent);
        TEST_ASSERT(Serial.buffer.find("UNSUPPORTED_OR_INVALID_PROTOCOL_VERSION") != std::string::npos, "Boolean version rejected");

        // D. String version rejected
        Serial.clear();
        processIncomingLine("{\"version\":\"1\",\"msg_type\":\"COMMAND\",\"msg_id\":1,\"timestamp_ms\":100,\"payload\":{}}", 1000, smTest, malformedCount, lastMalformedEvent);
        TEST_ASSERT(Serial.buffer.find("UNSUPPORTED_OR_INVALID_PROTOCOL_VERSION") != std::string::npos, "String version rejected");

        // E. Version 2 (unsupported version) rejected
        Serial.clear();
        processIncomingLine("{\"version\":2,\"msg_type\":\"COMMAND\",\"msg_id\":1,\"timestamp_ms\":100,\"payload\":{}}", 1000, smTest, malformedCount, lastMalformedEvent);
        TEST_ASSERT(Serial.buffer.find("UNSUPPORTED_OR_INVALID_PROTOCOL_VERSION") != std::string::npos, "Version 2 rejected");

        // F. Boolean msg_id rejected
        Serial.clear();
        processIncomingLine("{\"version\":1,\"msg_type\":\"COMMAND\",\"msg_id\":false,\"timestamp_ms\":100,\"payload\":{}}", 1000, smTest, malformedCount, lastMalformedEvent);
        TEST_ASSERT(Serial.buffer.find("INVALID_MSG_ID_TYPE") != std::string::npos, "Boolean msg_id rejected");

        // G. Boolean timestamp_ms rejected
        Serial.clear();
        processIncomingLine("{\"version\":1,\"msg_type\":\"COMMAND\",\"msg_id\":1,\"timestamp_ms\":true,\"payload\":{}}", 1000, smTest, malformedCount, lastMalformedEvent);
        TEST_ASSERT(Serial.buffer.find("INVALID_TIMESTAMP_TYPE") != std::string::npos, "Boolean timestamp rejected");

        // H. Missing msg_type rejected
        Serial.clear();
        processIncomingLine("{\"version\":1,\"msg_id\":1,\"timestamp_ms\":100,\"payload\":{}}", 1000, smTest, malformedCount, lastMalformedEvent);
        TEST_ASSERT(Serial.buffer.find("MISSING_OR_INVALID_MSG_TYPE") != std::string::npos, "Missing msg_type rejected");

        // I. Unsupported msg_type rejected
        Serial.clear();
        processIncomingLine("{\"version\":1,\"msg_type\":\"BOGUS_TYPE\",\"msg_id\":1,\"timestamp_ms\":100,\"payload\":{}}", 1000, smTest, malformedCount, lastMalformedEvent);
        TEST_ASSERT(Serial.buffer.find("UNSUPPORTED_MSG_TYPE") != std::string::npos, "Unsupported msg_type rejected");

        // J. Non-object payload rejected
        Serial.clear();
        processIncomingLine("{\"version\":1,\"msg_type\":\"COMMAND\",\"msg_id\":1,\"timestamp_ms\":100,\"payload\":\"not an object\"}", 1000, smTest, malformedCount, lastMalformedEvent);
        TEST_ASSERT(Serial.buffer.find("PAYLOAD_MUST_BE_OBJECT") != std::string::npos, "String payload rejected");

        // K. Missing command name in COMMAND
        Serial.clear();
        processIncomingLine("{\"version\":1,\"msg_type\":\"COMMAND\",\"msg_id\":1,\"timestamp_ms\":100,\"payload\":{}}", 1000, smTest, malformedCount, lastMalformedEvent);
        TEST_ASSERT(Serial.buffer.find("MISSING_OR_INVALID_COMMAND_NAME") != std::string::npos, "Missing command name rejected");

        // L. Boolean sequence in HEARTBEAT rejected
        Serial.clear();
        processIncomingLine("{\"version\":1,\"msg_type\":\"HEARTBEAT\",\"msg_id\":1,\"timestamp_ms\":100,\"payload\":{\"seq\":true}}", 1000, smTest, malformedCount, lastMalformedEvent);
        TEST_ASSERT(Serial.buffer.find("INVALID_HEARTBEAT_SEQ_TYPE") != std::string::npos, "Boolean seq rejected");

        // M. Valid HEARTBEAT via processIncomingLine -> produces ACCEPTED ack
        Serial.clear();
        processIncomingLine("{\"version\":1,\"msg_type\":\"HEARTBEAT\",\"msg_id\":10,\"timestamp_ms\":1000,\"payload\":{\"seq\":1}}", 1000, smTest, malformedCount, lastMalformedEvent);
        TEST_ASSERT(smTest.hasReceivedFirstHeartbeat(), "First heartbeat received via processIncomingLine");
        TEST_ASSERT(Serial.buffer.find("\"ack\":true") != std::string::npos, "Heartbeat ack must be true");
        TEST_ASSERT(Serial.buffer.find("\"status\":\"ACCEPTED\"") != std::string::npos, "Heartbeat ack status ACCEPTED");

        // N. Duplicate HEARTBEAT via processIncomingLine -> produces REJECTED ack
        Serial.clear();
        processIncomingLine("{\"version\":1,\"msg_type\":\"HEARTBEAT\",\"msg_id\":11,\"timestamp_ms\":1100,\"payload\":{\"seq\":1}}", 1100, smTest, malformedCount, lastMalformedEvent);
        TEST_ASSERT(Serial.buffer.find("\"ack\":false") != std::string::npos, "Duplicate heartbeat ack must be false");
        TEST_ASSERT(Serial.buffer.find("\"status\":\"REJECTED\"") != std::string::npos, "Duplicate heartbeat ack status REJECTED");
        TEST_ASSERT(Serial.buffer.find("STALE_OR_DUPLICATE_HEARTBEAT") != std::string::npos, "Duplicate reason check");

        // O. Valid COMMAND via processIncomingLine (PING)
        Serial.clear();
        processIncomingLine("{\"version\":1,\"msg_type\":\"COMMAND\",\"msg_id\":12,\"timestamp_ms\":1100,\"payload\":{\"command\":\"PING\"}}", 1100, smTest, malformedCount, lastMalformedEvent);
        TEST_ASSERT(Serial.buffer.find("\"reason\":\"PONG\"") != std::string::npos, "PING command executed via processIncomingLine");
    }

    std::cout << "All " << tests_passed << " native C++ assertion checks PASSED!" << std::endl;
    return 0;
}
