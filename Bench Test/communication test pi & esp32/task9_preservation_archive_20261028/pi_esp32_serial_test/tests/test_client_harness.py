"""
End-to-End Integration Test Harness.
Spawns a mock ESP32 device running over a virtual PTY serial pair,
then connects PiEsp32SerialClient to validate full bidirectional comms,
background heartbeat threading, safety timeout, latching, recovery,
and graceful disconnection handling.
"""

import os
import pty
import time
import json
import threading
import unittest
import subprocess
import sys

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'raspberry_pi')))
from protocol import ProtocolMessage, MsgType, State, Mode, FrameBuffer
from client import PiEsp32SerialClient
from test_state_machine_logic import SimulatedSafetyStateMachine

class MockEsp32Device:
    """Simulates the ESP32 firmware running over a PTY slave descriptor."""
    def __init__(self, master_fd: int):
        self.fd = master_fd
        self.running = False
        self.sm = SimulatedSafetyStateMachine()
        self.sm.init(now_ms=int(time.time() * 1000))
        self.thread = None
        self.fb = FrameBuffer()
        self.telemetry_seq = 0

    def start(self):
        self.running = True
        self.thread = threading.Thread(target=self._run, daemon=True)
        self.thread.start()

    def stop(self):
        self.running = False
        if self.thread:
            self.thread.join(timeout=1.0)

    def _write_line(self, line: str):
        try:
            os.write(self.fd, line.encode('utf-8'))
        except OSError:
            pass

    def _run(self):
        last_telem = time.time()
        while self.running:
            now_ms = int(time.time() * 1000)
            self.sm.update(now_ms)

            # Check for incoming bytes from client
            try:
                data = os.read(self.fd, 512)
                if data:
                    lines = self.fb.feed(data)
                    for l in lines:
                        self._handle_client_line(l, now_ms)
            except (BlockingIOError, InterruptedError):
                pass
            except OSError:
                break

            # Send periodic telemetry (every 200 ms in mock test)
            if time.time() - last_telem > 0.2:
                last_telem = time.time()
                self.telemetry_seq += 1
                telem_msg = ProtocolMessage(
                    msg_type=MsgType.TELEMETRY,
                    msg_id=self.telemetry_seq,
                    timestamp_ms=now_ms,
                    payload={
                        "state": self.sm.state.value,
                        "mode": self.sm.mode.value,
                        "uptime_ms": now_ms,
                        "telemetry_seq": self.telemetry_seq,
                        "heartbeat": {
                            "ok": self.sm.is_heartbeat_healthy(now_ms),
                            "last_received_age_ms": now_ms - self.sm.last_heartbeat_ms,
                            "missed_count": self.sm.missed_heartbeats
                        },
                        "sensors": {
                            "ultrasonic": "NOT_CONNECTED",
                            "imu": "NOT_CONNECTED",
                            "encoders": "NOT_CONNECTED"
                        },
                        "simulated_motion": self.sm.simulated_motion
                    }
                )
                self._write_line(telem_msg.serialize())

            time.sleep(0.01)

    def _handle_client_line(self, line: str, now_ms: int):
        try:
            msg = ProtocolMessage.deserialize(line)
        except Exception as e:
            err_line = json.dumps({"version": 1, "msg_type": "EVENT", "msg_id": 0, "timestamp_ms": now_ms, "payload": {"event": "MALFORMED_FRAME", "desc": str(e)}}) + "\n"
            self._write_line(err_line)
            return

        if msg.msg_type == MsgType.HEARTBEAT:
            seq = msg.payload.get("seq", 1)
            accepted = self.sm.record_heartbeat(now_ms, seq=seq)
            resp = ProtocolMessage(
                msg_type=MsgType.HEARTBEAT,
                msg_id=msg.msg_id,
                timestamp_ms=now_ms,
                payload={
                    "seq": seq,
                    "ack": accepted,
                    "status": "ACCEPTED" if accepted else "REJECTED",
                    "reason": "HEARTBEAT_OK" if accepted else "STALE_OR_DUPLICATE_HEARTBEAT",
                    "state": self.sm.state.value
                }
            )
            self._write_line(resp.serialize())
        elif msg.msg_type == MsgType.COMMAND:
            cmd = msg.payload.get("command")
            res_payload = self.sm.handle_command(cmd, msg.payload, now_ms, msg_id=msg.msg_id)
            resp = ProtocolMessage(
                msg_type=MsgType.RESPONSE,
                msg_id=msg.msg_id,
                timestamp_ms=now_ms,
                payload=res_payload
            )
            self._write_line(resp.serialize())


class TestClientWithMockEsp32(unittest.TestCase):
    def setUp(self):
        # Create virtual PTY pair
        self.master_fd, self.slave_fd = pty.openpty()
        os.set_blocking(self.master_fd, False)
        self.slave_name = os.ttyname(self.slave_fd)

        self.mock_esp32 = MockEsp32Device(self.master_fd)
        self.mock_esp32.start()

        self.client = PiEsp32SerialClient(port=self.slave_name, baudrate=115200, enable_heartbeat=True)
        self.assertTrue(self.client.connect())
        time.sleep(0.3)  # Allow initial handshake

    def tearDown(self):
        self.client.disconnect()
        self.mock_esp32.stop()
        try:
            os.close(self.slave_fd)
        except OSError:
            pass
        try:
            os.close(self.master_fd)
        except OSError:
            pass

    def test_e2e_ping_and_status(self):
        ping_res = self.client.ping()
        self.assertTrue(ping_res.get("ack"))
        self.assertEqual(ping_res.get("reason"), "PONG")

        status_res = self.client.get_status()
        self.assertTrue(status_res.get("ack"))
        self.assertEqual(status_res.get("state"), "IDLE")

    def test_e2e_mode_switch_arm_and_motion(self):
        # In IDLE: motion rejected
        move_idle = self.client.send_motion("TEST_FORWARD")
        self.assertFalse(move_idle.get("ack"))
        self.assertEqual(move_idle.get("reason"), "SYSTEM_NOT_ARMED_IDLE_STATE")

        # Set mode in IDLE
        mode_res = self.client.set_mode("MANUAL")
        self.assertTrue(mode_res.get("ack"))
        self.assertEqual(self.client.current_mode, Mode.MANUAL)

        # Arm to READY via RESET
        arm_res = self.client.reset_safety()
        self.assertTrue(arm_res.get("ack"))
        self.assertEqual(self.client.current_state, State.READY)

        # Move command accepted in READY
        move_res = self.client.send_motion("TEST_FORWARD", speed_pct=50)
        self.assertTrue(move_res.get("ack"))
        self.assertEqual(move_res.get("reason"), "SIMULATED_MOTION_ACTIVE")

        # Stop command returns to READY
        stop_res = self.client.send_motion("STOP")
        self.assertTrue(stop_res.get("ack"))
        self.assertEqual(stop_res.get("reason"), "MOTION_STOPPED")

    def test_e2e_heartbeat_timeout_latch_and_recovery(self):
        # 1. Arm to READY
        self.client.reset_safety()
        st = self.client.get_status()
        self.assertEqual(st.get("state"), "READY")

        # 2. Disable client heartbeats to induce timeout (>1.5s)
        self.client.enable_heartbeat = False
        time.sleep(1.7)

        # 3. Verify state transitioned to SAFE
        st_safe = self.client.get_status()
        self.assertEqual(st_safe.get("state"), "SAFE")

        # 4. Normal motion commands MUST be rejected in SAFE
        rej_res = self.client.send_motion("TEST_FORWARD")
        self.assertFalse(rej_res.get("ack"))
        self.assertEqual(rej_res.get("result"), "REJECTED")
        self.assertEqual(rej_res.get("reason"), "SYSTEM_IN_SAFE_STATE")

        # 5. Resume heartbeats; verify state DOES NOT auto-clear
        self.client.enable_heartbeat = True
        time.sleep(0.5)
        st_still_safe = self.client.get_status()
        self.assertEqual(st_still_safe.get("state"), "SAFE")

        # 6. Issue explicit RESET command -> State returns to READY
        reset_res = self.client.reset_safety()
        self.assertTrue(reset_res.get("ack"))
        self.assertEqual(reset_res.get("reason"), "SAFETY_LATCH_CLEARED_READY")

        # 7. Motion command now accepted again
        move_ok = self.client.send_motion("TEST_FORWARD")
        self.assertTrue(move_ok.get("ack"))

    def test_e2e_disconnection_handling(self):
        self.assertTrue(self.client.is_connected)

        # Close slave/master side to simulate hardware disconnect
        os.close(self.master_fd)
        time.sleep(0.3)

        # is_connected must accurately report False
        self.assertFalse(self.client.is_connected)

        # Sending command while disconnected fails fast
        res = self.client.ping()
        self.assertFalse(res.get("ack"))
        self.assertEqual(res.get("result"), "ERROR")

    def test_e2e_rejected_heartbeat_not_counted_as_healthy(self):
        """Prove that when ESP32 rejects a heartbeat, Pi client does not treat it as a healthy ack."""
        # Feed an explicit rejected heartbeat directly to the client
        rej_msg = ProtocolMessage(
            msg_type=MsgType.HEARTBEAT,
            msg_id=9999,
            timestamp_ms=int(time.time() * 1000),
            payload={
                "seq": 0,
                "ack": False,
                "status": "REJECTED",
                "reason": "STALE_OR_DUPLICATE_HEARTBEAT",
                "state": "READY"
            }
        )
        prev_rejected = self.client.stats.get("rejected_hb", 0)
        self.client._handle_incoming_message(rej_msg)
        self.assertEqual(self.client.stats["rejected_hb"], prev_rejected + 1)

    def test_cli_monitor_executable_and_imports(self):
        # Verify direct execution of cli_monitor.py --help
        cli_path = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'raspberry_pi', 'cli_monitor.py'))
        p1 = subprocess.run([sys.executable, cli_path, "--help"], capture_output=True, text=True)
        self.assertEqual(p1.returncode, 0)
        self.assertIn("AutoChair Standalone Pi <-> ESP32 Serial Monitor", p1.stdout)

        # Verify module execution python -m raspberry_pi.cli_monitor --help
        proj_root = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
        p2 = subprocess.run([sys.executable, "-m", "raspberry_pi.cli_monitor", "--help"], cwd=proj_root, capture_output=True, text=True)
        self.assertEqual(p2.returncode, 0)
        self.assertIn("AutoChair Standalone Pi <-> ESP32 Serial Monitor", p2.stdout)

if __name__ == '__main__':
    unittest.main()
