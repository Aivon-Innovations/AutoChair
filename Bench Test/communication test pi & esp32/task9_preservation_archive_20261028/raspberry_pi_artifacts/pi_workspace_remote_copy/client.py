"""
AutoChair Raspberry Pi Serial Communication Client.
Handles USB serial framing, bidirectional messaging, background heartbeats,
and synchronized command-response transactions.
"""

import threading
import time
import logging
from typing import Dict, Any, Optional, Callable, List
import serial

try:
    from .protocol import (
        ProtocolMessage,
        MsgType,
        State,
        Mode,
        FrameBuffer,
        ProtocolError,
        DEFAULT_HEARTBEAT_INTERVAL_MS,
        DEFAULT_HEARTBEAT_TIMEOUT_MS
    )
except ImportError:
    from protocol import (
        ProtocolMessage,
        MsgType,
        State,
        Mode,
        FrameBuffer,
        ProtocolError,
        DEFAULT_HEARTBEAT_INTERVAL_MS,
        DEFAULT_HEARTBEAT_TIMEOUT_MS
    )

logger = logging.getLogger("PiEsp32Client")

class PiEsp32SerialClient:
    def __init__(
        self,
        port: str,
        baudrate: int = 115200,
        heartbeat_interval_ms: int = DEFAULT_HEARTBEAT_INTERVAL_MS,
        heartbeat_timeout_ms: int = DEFAULT_HEARTBEAT_TIMEOUT_MS,
        enable_heartbeat: bool = True
    ):
        self.port = port
        self.baudrate = baudrate
        self.heartbeat_interval_s = heartbeat_interval_ms / 1000.0
        self.heartbeat_timeout_s = heartbeat_timeout_ms / 1000.0
        self.enable_heartbeat = enable_heartbeat

        self._ser: Optional[serial.Serial] = None
        self._rx_thread: Optional[threading.Thread] = None
        self._hb_thread: Optional[threading.Thread] = None
        self._running = False
        self._frame_buffer = FrameBuffer()

        self._msg_id_counter = 1000
        self._msg_id_lock = threading.Lock()

        # Command response correlation: msg_id -> (threading.Event, response_holder)
        self._pending_commands: Dict[int, Dict[str, Any]] = {}
        self._pending_lock = threading.Lock()

        # Telemetry & state
        self._current_state = State.BOOT
        self._current_mode = Mode.NONE
        self._last_telemetry: Optional[Dict[str, Any]] = None
        self._last_telemetry_time = 0.0
        self._telemetry_callbacks: List[Callable[[Dict[str, Any]], None]] = []

        # Heartbeat stats
        self._hb_seq = 0
        self._last_hb_ack_time = 0.0
        self._last_acked_hb_seq = -1
        self._missed_hb_count = 0

        # Diagnostics & counters
        self.stats = {
            "tx_messages": 0,
            "rx_messages": 0,
            "rx_bytes": 0,
            "malformed_frames": 0,
            "timeouts": 0,
            "rejections": 0,
            "duplicate_responses": 0,
            "out_of_order_hb": 0,
            "rejected_hb": 0,
            "disconnect_events": 0
        }

    @property
    def is_heartbeat_healthy(self) -> bool:
        """Returns True only if a valid heartbeat acknowledgement was received within timeout."""
        if self._last_hb_ack_time <= 0.0:
            return False
        return (time.time() - self._last_hb_ack_time) <= self.heartbeat_timeout_s

    @property
    def is_connected(self) -> bool:
        """Accurately returns True ONLY if serial is open, running, and reader thread is alive."""
        return (
            self._ser is not None and
            self._ser.is_open and
            self._running and
            self._rx_thread is not None and
            self._rx_thread.is_alive()
        )

    @property
    def current_state(self) -> State:
        return self._current_state

    @property
    def current_mode(self) -> Mode:
        return self._current_mode

    @property
    def last_telemetry(self) -> Optional[Dict[str, Any]]:
        return self._last_telemetry

    def _next_msg_id(self) -> int:
        with self._msg_id_lock:
            self._msg_id_counter += 1
            if self._msg_id_counter > 999999:
                self._msg_id_counter = 1000
            return self._msg_id_counter

    def connect(self) -> bool:
        """Establish serial connection and launch background threads."""
        if self.is_connected:
            return True

        logger.info(f"Opening serial port {self.port} at {self.baudrate} baud...")
        try:
            self._ser = serial.Serial()
            self._ser.port = self.port
            self._ser.baudrate = self.baudrate
            self._ser.timeout = 0.1
            self._ser.dtr = False
            self._ser.rts = False
            self._ser.open()
            time.sleep(0.3)  # Allow port settling

            # Flush any residual boot garbage
            self._ser.reset_input_buffer()
            self._ser.reset_output_buffer()
        except Exception as e:
            logger.error(f"Failed to open serial port {self.port}: {e}")
            self._ser = None
            return False

        self._running = True
        self._rx_thread = threading.Thread(target=self._rx_worker, daemon=True, name="SerialRX")
        self._rx_thread.start()

        self._hb_thread = threading.Thread(target=self._hb_worker, daemon=True, name="HeartbeatTX")
        self._hb_thread.start()

        logger.info(f"Connected to {self.port}. Background workers active.")
        return True

    def disconnect(self):
        """Cleanly stop background workers and close serial port."""
        self._running = False

        if self._ser and self._ser.is_open:
            try:
                self._ser.close()
            except Exception:
                pass
        self._ser = None

        if self._hb_thread and self._hb_thread.is_alive():
            self._hb_thread.join(timeout=1.0)
        self._hb_thread = None

        if self._rx_thread and self._rx_thread.is_alive():
            self._rx_thread.join(timeout=1.0)
        self._rx_thread = None

        # Clean and wake any pending commands waiting
        with self._pending_lock:
            for pending in self._pending_commands.values():
                pending["holder"]["response"] = None
                pending["event"].set()
            self._pending_commands.clear()

        logger.info("Disconnected serial interface.")

    def add_telemetry_callback(self, cb: Callable[[Dict[str, Any]], None]):
        self._telemetry_callbacks.append(cb)

    def _send_raw_message(self, msg: ProtocolMessage) -> bool:
        if not self._ser or not self._ser.is_open:
            return False
        try:
            line = msg.serialize()
            self._ser.write(line.encode('utf-8'))
            self._ser.flush()
            self.stats["tx_messages"] += 1
            return True
        except Exception as e:
            logger.error(f"Serial write error: {e}")
            self._mark_disconnected()
            return False

    def _mark_disconnected(self):
        """Internal helper to mark disconnection and unblock any waiting commands immediately."""
        if self._running:
            self.stats["disconnect_events"] += 1
            self._running = False
            with self._pending_lock:
                for pending in self._pending_commands.values():
                    pending["holder"]["response"] = None
                    pending["event"].set()

    def send_command(self, cmd_name: str, payload: Optional[Dict[str, Any]] = None, timeout: float = 1.0) -> Dict[str, Any]:
        """
        Thread-safe synchronous command transaction.
        Sends COMMAND, blocks until matching RESPONSE is received or timeout expires.
        """
        if not self.is_connected:
            return {"ack": False, "result": "ERROR", "reason": "SERIAL_NOT_CONNECTED"}

        msg_id = self._next_msg_id()
        cmd_payload = {"command": cmd_name}
        if payload:
            cmd_payload.update(payload)

        try:
            msg = ProtocolMessage(
                msg_type=MsgType.COMMAND,
                msg_id=msg_id,
                payload=cmd_payload
            )
        except ProtocolError as pe:
            return {"ack": False, "result": "ERROR", "reason": f"PROTOCOL_VALIDATION_FAILED: {pe}"}

        event = threading.Event()
        holder: Dict[str, Any] = {"response": None}

        with self._pending_lock:
            self._pending_commands[msg_id] = {"event": event, "holder": holder}

        try:
            if not self._send_raw_message(msg):
                with self._pending_lock:
                    self._pending_commands.pop(msg_id, None)
                return {"ack": False, "result": "ERROR", "reason": "SERIAL_WRITE_FAILED"}

            signaled = event.wait(timeout=timeout)
            with self._pending_lock:
                self._pending_commands.pop(msg_id, None)

            if not self.is_connected:
                return {"ack": False, "result": "ERROR", "reason": "SERIAL_DISCONNECTED_DURING_COMMAND"}

            if not signaled or holder["response"] is None:
                self.stats["timeouts"] += 1
                return {"ack": False, "result": "ERROR", "reason": "TIMEOUT"}

            resp_msg: ProtocolMessage = holder["response"]
            resp_payload = resp_msg.payload
            if not resp_payload.get("ack", False):
                self.stats["rejections"] += 1
            return resp_payload

        except Exception as e:
            logger.error(f"Command execution error: {e}")
            with self._pending_lock:
                self._pending_commands.pop(msg_id, None)
            return {"ack": False, "result": "ERROR", "reason": str(e)}

    # High-level command helpers
    def ping(self) -> Dict[str, Any]:
        return self.send_command("PING")

    def get_status(self) -> Dict[str, Any]:
        return self.send_command("GET_STATUS")

    def set_mode(self, mode: str) -> Dict[str, Any]:
        return self.send_command("SET_MODE", {"mode": mode})

    def send_motion(self, action: str, speed_pct: int = 50) -> Dict[str, Any]:
        return self.send_command("MOVE", {"action": action, "speed_pct": speed_pct})

    def reset_safety(self) -> Dict[str, Any]:
        return self.send_command("RESET")

    def _rx_worker(self):
        """Background thread parsing incoming serial bytes."""
        while self._running and self._ser and self._ser.is_open:
            try:
                raw_bytes = self._ser.read(self._ser.in_waiting or 1)
                if not raw_bytes:
                    continue
                self.stats["rx_bytes"] += len(raw_bytes)
                lines = self._frame_buffer.feed(raw_bytes)

                for line in lines:
                    try:
                        msg = ProtocolMessage.deserialize(line)
                        self.stats["rx_messages"] += 1
                        self._handle_incoming_message(msg)
                    except ProtocolError as pe:
                        self.stats["malformed_frames"] += 1
                        logger.warning(f"Protocol frame error: {pe} | Line: {line}")
            except Exception as e:
                if self._running:
                    logger.error(f"RX worker exception: {e}")
                break

        # If reader loop exits unexpectedly while running, mark disconnected
        self._mark_disconnected()

    def _handle_incoming_message(self, msg: ProtocolMessage):
        """Dispatch incoming parsed protocol message."""
        if msg.msg_type == MsgType.RESPONSE:
            with self._pending_lock:
                pending = self._pending_commands.get(msg.msg_id)
                if pending:
                    pending["holder"]["response"] = msg
                    pending["event"].set()
                else:
                    self.stats["duplicate_responses"] += 1
                    logger.debug(f"Unsolicited or duplicate response for msg_id {msg.msg_id}")

            # Update cached state and mode from response payload if present
            if "state" in msg.payload:
                try:
                    self._current_state = State(msg.payload["state"])
                except ValueError:
                    pass
            if "mode" in msg.payload:
                try:
                    self._current_mode = Mode(msg.payload["mode"])
                except ValueError:
                    pass

        elif msg.msg_type == MsgType.TELEMETRY:
            self._last_telemetry = msg.payload
            self._last_telemetry_time = time.time()
            if "state" in msg.payload:
                try:
                    self._current_state = State(msg.payload["state"])
                except ValueError:
                    pass
            if "mode" in msg.payload:
                try:
                    self._current_mode = Mode(msg.payload["mode"])
                except ValueError:
                    pass

            for cb in self._telemetry_callbacks:
                try:
                    cb(msg.payload)
                except Exception as e:
                    logger.error(f"Telemetry callback error: {e}")

        elif msg.msg_type == MsgType.HEARTBEAT:
            # ESP32 heartbeat response / echo
            is_ack = msg.payload.get("ack", True)
            status = msg.payload.get("status", "ACCEPTED")
            incoming_seq = msg.payload.get("seq", -1)

            if is_ack and status == "ACCEPTED":
                self._last_hb_ack_time = time.time()
                if incoming_seq <= self._last_acked_hb_seq and self._last_acked_hb_seq != -1:
                    self.stats["out_of_order_hb"] += 1
                self._last_acked_hb_seq = incoming_seq
            else:
                self.stats["rejected_hb"] += 1
                logger.warning(
                    f"Heartbeat rejected by ESP32: seq={incoming_seq}, "
                    f"reason={msg.payload.get('reason', 'UNKNOWN')}"
                )

            if "state" in msg.payload:
                try:
                    self._current_state = State(msg.payload["state"])
                except ValueError:
                    pass

        elif msg.msg_type == MsgType.EVENT:
            logger.info(f"[EVENT FROM ESP32] {msg.payload.get('event')}: {msg.payload}")
            if "state" in msg.payload:
                try:
                    self._current_state = State(msg.payload["state"])
                except ValueError:
                    pass

    def _hb_worker(self):
        """Background worker transmitting 250ms heartbeats."""
        while self._running:
            if self.enable_heartbeat:
                try:
                    self._hb_seq += 1
                    hb_msg = ProtocolMessage(
                        msg_type=MsgType.HEARTBEAT,
                        msg_id=self._next_msg_id(),
                        payload={"seq": self._hb_seq, "expected_state": self._current_state.value}
                    )
                    self._send_raw_message(hb_msg)
                except Exception as e:
                    logger.error(f"Heartbeat send error: {e}")

            time.sleep(self.heartbeat_interval_s)
