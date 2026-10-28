"""
Unit tests for Safety State Machine logic and heartbeat timeout rules (Python reference model).
Reflects Stage A.3 fixes: first heartbeat requirement, sequence validation, and bounded duplicate cache.
"""

import unittest
import sys
import os

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'raspberry_pi')))
from protocol import State, Mode

RECENT_CMD_CACHE_SIZE = 16

class SimulatedSafetyStateMachine:
    """Python reference model mirrors exact C++ state machine rules."""
    def __init__(self):
        self.state = State.BOOT
        self.mode = Mode.NONE
        self.latched_in_safe = False
        self.first_heartbeat_received = False
        self.last_heartbeat_ms = 0
        self.last_heartbeat_seq = 0
        self.missed_heartbeats = 0
        self.stale_heartbeats = 0
        self.simulated_motion = "STOP"
        self.heartbeat_timeout_ms = 1500
        self.cmd_cache = [] # List of dicts up to RECENT_CMD_CACHE_SIZE
        self.duplicate_commands = 0

    def init(self, now_ms: int = 1000):
        self.state = State.IDLE
        self.mode = Mode.NONE
        self.latched_in_safe = False
        self.first_heartbeat_received = False
        self.last_heartbeat_ms = 0
        self.last_heartbeat_seq = 0
        self.missed_heartbeats = 0
        self.stale_heartbeats = 0
        self.duplicate_commands = 0
        self.cmd_cache = []
        self.simulated_motion = "STOP"

    def is_heartbeat_healthy(self, now_ms: int) -> bool:
        if not self.first_heartbeat_received or self.last_heartbeat_ms == 0:
            return False
        diff = (now_ms - self.last_heartbeat_ms) & 0xFFFFFFFF
        return diff <= self.heartbeat_timeout_ms

    def record_heartbeat(self, now_ms: int, seq: int) -> bool:
        if not self.first_heartbeat_received:
            self.first_heartbeat_received = True
            self.last_heartbeat_seq = seq
            self.last_heartbeat_ms = now_ms
            return True

        # RFC 1982 Serial Number Arithmetic (unsigned 32-bit window)
        diff = (seq - self.last_heartbeat_seq) & 0xFFFFFFFF
        is_valid_advance = (diff > 0) and (diff < 0x80000000)

        if not is_valid_advance:
            self.stale_heartbeats += 1
            # MUST NOT refresh the watchdog timestamp
            return False

        self.last_heartbeat_seq = seq
        self.last_heartbeat_ms = now_ms
        return True

    def update(self, now_ms: int):
        if self.state in (State.READY, State.ACTIVE_TEST):
            if not self.is_heartbeat_healthy(now_ms):
                self.state = State.SAFE
                self.latched_in_safe = True
                self.missed_heartbeats += 1
                self.simulated_motion = "STOP"

    def _find_cache(self, msg_id: int):
        if msg_id == 0:
            return None
        for entry in self.cmd_cache:
            if entry["msg_id"] == msg_id:
                return entry
        return None

    def _add_cache(self, msg_id: int, ack: bool, result: str, reason: str):
        if msg_id == 0:
            return
        if len(self.cmd_cache) >= RECENT_CMD_CACHE_SIZE:
            self.cmd_cache.pop(0) # Evict oldest FIFO
        self.cmd_cache.append({
            "msg_id": msg_id,
            "ack": ack,
            "result": result,
            "reason": reason,
            "state": self.state.value,
            "mode": self.mode.value
        })

    def handle_command(self, cmd: str, payload: dict, now_ms: int, msg_id: int = 0) -> dict:
        cached = self._find_cache(msg_id)
        if cached:
            self.duplicate_commands += 1
            return {
                "ack": cached["ack"],
                "result": cached["result"],
                "reason": "DUPLICATE_COMMAND_CACHED",
                "state": self.state.value,
                "mode": self.mode.value
            }

        if cmd == "PING":
            self._add_cache(msg_id, True, "ACCEPTED", "PONG")
            return {"ack": True, "result": "ACCEPTED", "reason": "PONG", "state": self.state.value, "mode": self.mode.value}

        if cmd == "GET_STATUS":
            self._add_cache(msg_id, True, "ACCEPTED", "STATUS_OK")
            return {"ack": True, "result": "ACCEPTED", "state": self.state.value, "mode": self.mode.value}

        if cmd == "SET_MODE":
            if self.state in (State.SAFE, State.FAULT, State.ACTIVE_TEST):
                self._add_cache(msg_id, False, "REJECTED", "CANNOT_CHANGE_MODE_IN_SAFE_FAULT_OR_ACTIVE")
                return {"ack": False, "result": "REJECTED", "reason": "CANNOT_CHANGE_MODE_IN_SAFE_FAULT_OR_ACTIVE", "state": self.state.value, "mode": self.mode.value}
            m = payload.get("mode")
            if m in ("MANUAL", "ASSISTED", "NONE"):
                self.mode = Mode(m)
                self._add_cache(msg_id, True, "ACCEPTED", "MODE_UPDATED")
                return {"ack": True, "result": "ACCEPTED", "mode": self.mode.value, "state": self.state.value}
            self._add_cache(msg_id, False, "REJECTED", "INVALID_MODE_VALUE")
            return {"ack": False, "result": "REJECTED", "reason": "INVALID_MODE_VALUE", "state": self.state.value, "mode": self.mode.value}

        if cmd == "MOVE":
            if self.state in (State.SAFE, State.FAULT):
                self._add_cache(msg_id, False, "REJECTED", "SYSTEM_IN_SAFE_STATE")
                return {"ack": False, "result": "REJECTED", "reason": "SYSTEM_IN_SAFE_STATE", "state": self.state.value, "mode": self.mode.value}
            if self.state in (State.IDLE, State.BOOT):
                self._add_cache(msg_id, False, "REJECTED", "SYSTEM_NOT_ARMED_IDLE_STATE")
                return {"ack": False, "result": "REJECTED", "reason": "SYSTEM_NOT_ARMED_IDLE_STATE", "state": self.state.value, "mode": self.mode.value}

            action = payload.get("action")
            if action == "STOP":
                self.simulated_motion = "STOP"
                self.state = State.READY
                self._add_cache(msg_id, True, "ACCEPTED", "MOTION_STOPPED")
                return {"ack": True, "result": "ACCEPTED", "reason": "MOTION_STOPPED", "state": self.state.value, "mode": self.mode.value}
            elif action in ("TEST_FORWARD", "TEST_LEFT", "TEST_RIGHT"):
                self.simulated_motion = action
                self.state = State.ACTIVE_TEST
                self._add_cache(msg_id, True, "ACCEPTED", "SIMULATED_MOTION_ACTIVE")
                return {"ack": True, "result": "ACCEPTED", "reason": "SIMULATED_MOTION_ACTIVE", "state": self.state.value, "mode": self.mode.value}
            self._add_cache(msg_id, False, "REJECTED", "UNKNOWN_MOTION_ACTION")
            return {"ack": False, "result": "REJECTED", "reason": "UNKNOWN_MOTION_ACTION", "state": self.state.value, "mode": self.mode.value}

        if cmd in ("RESET", "RECOVER", "ARM"):
            if not self.first_heartbeat_received:
                self._add_cache(msg_id, False, "REJECTED", "CANNOT_RESET_BEFORE_FIRST_HEARTBEAT")
                return {"ack": False, "result": "REJECTED", "reason": "CANNOT_RESET_BEFORE_FIRST_HEARTBEAT", "state": self.state.value, "mode": self.mode.value}
            if not self.is_heartbeat_healthy(now_ms):
                self._add_cache(msg_id, False, "REJECTED", "CANNOT_RESET_WITHOUT_HEALTHY_HEARTBEAT")
                return {"ack": False, "result": "REJECTED", "reason": "CANNOT_RESET_WITHOUT_HEALTHY_HEARTBEAT", "state": self.state.value, "mode": self.mode.value}
            self.latched_in_safe = False
            self.state = State.READY
            self.simulated_motion = "STOP"
            self._add_cache(msg_id, True, "ACCEPTED", "SAFETY_LATCH_CLEARED_READY")
            return {"ack": True, "result": "ACCEPTED", "reason": "SAFETY_LATCH_CLEARED_READY", "state": self.state.value, "mode": self.mode.value}

        self._add_cache(msg_id, False, "REJECTED", "UNKNOWN_COMMAND")
        return {"ack": False, "result": "REJECTED", "reason": "UNKNOWN_COMMAND", "state": self.state.value, "mode": self.mode.value}


class TestSafetyStateMachineLogic(unittest.TestCase):
    def setUp(self):
        self.sm = SimulatedSafetyStateMachine()
        self.sm.init(now_ms=1000)

    def test_boot_and_idle_startup(self):
        self.assertEqual(self.sm.state, State.IDLE)
        self.assertFalse(self.sm.latched_in_safe)
        self.assertFalse(self.sm.first_heartbeat_received)
        self.assertEqual(self.sm.simulated_motion, "STOP")

        # Movement in IDLE is rejected
        res = self.sm.handle_command("MOVE", {"action": "TEST_FORWARD"}, now_ms=1050)
        self.assertFalse(res["ack"])
        self.assertEqual(res["reason"], "SYSTEM_NOT_ARMED_IDLE_STATE")

    def test_arming_rejected_before_first_heartbeat(self):
        # Attempt to arm before any heartbeat
        res = self.sm.handle_command("RESET", {}, now_ms=1050, msg_id=1)
        self.assertFalse(res["ack"])
        self.assertEqual(res["reason"], "CANNOT_RESET_BEFORE_FIRST_HEARTBEAT")
        self.assertEqual(self.sm.state, State.IDLE)

    def test_first_heartbeat_enables_arming(self):
        # First heartbeat arrives
        accepted = self.sm.record_heartbeat(1050, seq=1)
        self.assertTrue(accepted)
        self.assertTrue(self.sm.first_heartbeat_received)

        # Now arming succeeds
        res_arm = self.sm.handle_command("RESET", {}, now_ms=1060, msg_id=2)
        self.assertTrue(res_arm["ack"])
        self.assertEqual(self.sm.state, State.READY)

    def test_heartbeat_sequence_policy(self):
        self.sm.record_heartbeat(1000, seq=1)

        # Duplicate seq = 1 must be rejected and not update timestamp
        dup_ok = self.sm.record_heartbeat(1200, seq=1)
        self.assertFalse(dup_ok)
        self.assertEqual(self.sm.stale_heartbeats, 1)
        self.assertEqual(self.sm.last_heartbeat_ms, 1000)

        # Fresh monotonic seq = 2 accepted
        fresh_ok = self.sm.record_heartbeat(1250, seq=2)
        self.assertTrue(fresh_ok)
        self.assertEqual(self.sm.last_heartbeat_seq, 2)
        self.assertEqual(self.sm.last_heartbeat_ms, 1250)

    def test_heartbeat_timeout_trips_to_safe(self):
        self.sm.record_heartbeat(1000, seq=1)
        self.sm.handle_command("RESET", {}, now_ms=1000, msg_id=1)
        self.assertEqual(self.sm.state, State.READY)

        now = 1000
        # Advance 1000 ms (timeout is 1500 ms) -> still healthy
        now += 1000
        self.sm.update(now)
        self.assertEqual(self.sm.state, State.READY)

        # Advance another 600 ms (total 1600 ms > 1500 ms) -> Trips to SAFE!
        now += 600
        self.sm.update(now)
        self.assertEqual(self.sm.state, State.SAFE)
        self.assertTrue(self.sm.latched_in_safe)
        self.assertEqual(self.sm.missed_heartbeats, 1)

    def test_command_rejection_in_safe_state(self):
        self.sm.record_heartbeat(1000, seq=1)
        self.sm.handle_command("RESET", {}, now_ms=1000, msg_id=1)
        self.sm.update(now_ms=3000)
        self.assertEqual(self.sm.state, State.SAFE)

        # Attempt motion command
        res = self.sm.handle_command("MOVE", {"action": "TEST_FORWARD"}, now_ms=3100, msg_id=2)
        self.assertFalse(res["ack"])
        self.assertEqual(res["result"], "REJECTED")
        self.assertEqual(res["reason"], "SYSTEM_IN_SAFE_STATE")

    def test_safe_latch_no_auto_recovery_when_heartbeat_resumes(self):
        self.sm.record_heartbeat(1000, seq=1)
        self.sm.handle_command("RESET", {}, now_ms=1000, msg_id=1)
        self.sm.update(now_ms=3000)
        self.assertEqual(self.sm.state, State.SAFE)

        # Heartbeat resumes with monotonic seq=2
        self.sm.record_heartbeat(now_ms=3200, seq=2)
        self.sm.update(now_ms=3250)

        # Crucial check: MUST REMAIN SAFE!
        self.assertEqual(self.sm.state, State.SAFE)
        self.assertTrue(self.sm.latched_in_safe)

    def test_explicit_recovery_requires_healthy_heartbeat(self):
        self.sm.record_heartbeat(1000, seq=1)
        self.sm.handle_command("RESET", {}, now_ms=1000, msg_id=1)
        self.sm.update(now_ms=3000)
        self.assertEqual(self.sm.state, State.SAFE)

        # Try to reset while heartbeat is dead (last hb was t=1000)
        res_fail = self.sm.handle_command("RESET", {}, now_ms=3100, msg_id=2)
        self.assertFalse(res_fail["ack"])
        self.assertEqual(res_fail["reason"], "CANNOT_RESET_WITHOUT_HEALTHY_HEARTBEAT")
        self.assertEqual(self.sm.state, State.SAFE)

        # Restore heartbeat with fresh seq at t=3200
        self.sm.record_heartbeat(now_ms=3200, seq=2)
        # Now reset while heartbeat is healthy (t=3250)
        res_ok = self.sm.handle_command("RESET", {}, now_ms=3250, msg_id=3)
        self.assertTrue(res_ok["ack"])
        self.assertEqual(self.sm.state, State.READY)
        self.assertFalse(self.sm.latched_in_safe)

    def test_sequence_progression_and_rollover(self):
        # 1. First heartbeat baseline
        self.assertTrue(self.sm.record_heartbeat(1000, seq=100))
        self.assertEqual(self.sm.last_heartbeat_seq, 100)

        # 2. Normal progression (seq > last)
        self.assertTrue(self.sm.record_heartbeat(1250, seq=101))
        self.assertTrue(self.sm.record_heartbeat(1500, seq=105))

        # 3. Duplicate sequence (seq == last) -> REJECTED, does not refresh timestamp
        last_ts = self.sm.last_heartbeat_ms
        self.assertFalse(self.sm.record_heartbeat(1750, seq=105))
        self.assertEqual(self.sm.last_heartbeat_ms, last_ts)
        self.assertEqual(self.sm.stale_heartbeats, 1)

        # 4. Out-of-order sequence (seq < last) -> REJECTED
        self.assertFalse(self.sm.record_heartbeat(2000, seq=104))
        self.assertEqual(self.sm.last_heartbeat_ms, last_ts)
        self.assertEqual(self.sm.stale_heartbeats, 2)

        # 5. Initialize fresh state machine near rollover boundary (0xFFFFFFFA)
        rollover_sm = SimulatedSafetyStateMachine()
        rollover_sm.init()
        self.assertTrue(rollover_sm.record_heartbeat(2000, seq=0xFFFFFFFA))
        self.assertEqual(rollover_sm.last_heartbeat_seq, 0xFFFFFFFA)

        # Advance to 0xFFFFFFFF
        self.assertTrue(rollover_sm.record_heartbeat(2250, seq=0xFFFFFFFF))
        self.assertEqual(rollover_sm.last_heartbeat_seq, 0xFFFFFFFF)

        # 6. Duplicate at rollover boundary -> REJECTED
        last_ts = self.sm.last_heartbeat_ms
        self.assertFalse(rollover_sm.record_heartbeat(2500, seq=0xFFFFFFFF))
        self.assertEqual(self.sm.last_heartbeat_ms, last_ts)
        self.assertEqual(rollover_sm.stale_heartbeats, 1)

        # 7. Rollover to 0 (0 - 0xFFFFFFFF = 1) -> ACCEPTED
        self.assertTrue(rollover_sm.record_heartbeat(2750, seq=0))
        self.assertEqual(rollover_sm.last_heartbeat_seq, 0)

        # 8. Rollover advance to 5 -> ACCEPTED
        self.assertTrue(rollover_sm.record_heartbeat(3000, seq=5))
        self.assertEqual(rollover_sm.last_heartbeat_seq, 5)

        # 9. Stale seq from before rollover (e.g. 0xFFFFFFFE) -> REJECTED
        last_ts = self.sm.last_heartbeat_ms
        self.assertFalse(rollover_sm.record_heartbeat(3250, seq=0xFFFFFFFE))
        self.assertEqual(self.sm.last_heartbeat_ms, last_ts)
        self.assertEqual(rollover_sm.stale_heartbeats, 2)

    def test_rollover_safety(self):
        t_pre = 0xFFFFFFF0
        t_post = 0x00000010
        self.sm.record_heartbeat(t_pre, seq=100)
        self.assertTrue(self.sm.is_heartbeat_healthy(t_post))

    def test_bounded_cache_and_duplicate_handling(self):
        self.sm.record_heartbeat(1000, seq=1)
        res1 = self.sm.handle_command("PING", {}, now_ms=1000, msg_id=50)
        self.assertTrue(res1["ack"])

        # Immediate duplicate
        res2 = self.sm.handle_command("PING", {}, now_ms=1000, msg_id=50)
        self.assertEqual(res2["reason"], "DUPLICATE_COMMAND_CACHED")
        self.assertEqual(self.sm.duplicate_commands, 1)

        # Fill cache with 15 commands (51 to 65)
        for i in range(51, 66):
            self.sm.handle_command("PING", {}, now_ms=1000, msg_id=i)

        # msg_id=50 should still be cached (16th entry)
        res3 = self.sm.handle_command("PING", {}, now_ms=1000, msg_id=50)
        self.assertEqual(res3["reason"], "DUPLICATE_COMMAND_CACHED")

        # Insert one more to evict msg_id=50
        self.sm.handle_command("PING", {}, now_ms=1000, msg_id=66)

        # msg_id=50 is now evicted, executed fresh
        res4 = self.sm.handle_command("PING", {}, now_ms=1000, msg_id=50)
        self.assertEqual(res4["reason"], "PONG")

if __name__ == '__main__':
    unittest.main()
