"""
AutoChair Standalone USB Serial Protocol Definition & Framing.
Protocol Version: 1
"""

import json
import time
from enum import Enum
from typing import Dict, Any, Optional, Tuple, List

PROTOCOL_VERSION = 1
MAX_MESSAGE_BYTES = 512
DEFAULT_HEARTBEAT_INTERVAL_MS = 250
DEFAULT_HEARTBEAT_TIMEOUT_MS = 1500

class State(str, Enum):
    BOOT = "BOOT"
    IDLE = "IDLE"
    READY = "READY"
    ACTIVE_TEST = "ACTIVE_TEST"
    SAFE = "SAFE"
    FAULT = "FAULT"

class Mode(str, Enum):
    NONE = "NONE"
    MANUAL = "MANUAL"
    ASSISTED = "ASSISTED"

class MsgType(str, Enum):
    COMMAND = "COMMAND"
    RESPONSE = "RESPONSE"
    TELEMETRY = "TELEMETRY"
    HEARTBEAT = "HEARTBEAT"
    EVENT = "EVENT"

class ProtocolError(Exception):
    """Protocol parsing or validation exception."""
    pass

class ProtocolMessage:
    def __init__(
        self,
        msg_type: MsgType,
        msg_id: int,
        payload: Dict[str, Any],
        timestamp_ms: Optional[int] = None,
        version: int = PROTOCOL_VERSION
    ):
        # Strict type and range validation
        if type(version) is not int or isinstance(version, bool):
            raise ProtocolError(f"Field 'version' must be an integer, got {type(version).__name__}")
        if version != PROTOCOL_VERSION:
            raise ProtocolError(f"Unsupported protocol version: {version}, expected {PROTOCOL_VERSION}")

        if type(msg_id) is not int or isinstance(msg_id, bool):
            raise ProtocolError(f"Field 'msg_id' must be an integer, got {type(msg_id).__name__}")
        if msg_id < 0:
            raise ProtocolError(f"Field 'msg_id' cannot be negative, got {msg_id}")

        ts = int(timestamp_ms if timestamp_ms is not None else (time.monotonic_ns() // 1_000_000) & 0xFFFFFFFF)
        if type(ts) is not int or isinstance(ts, bool) or ts < 0:
            raise ProtocolError(f"Field 'timestamp_ms' must be a non-negative integer, got {ts}")

        if not isinstance(payload, dict):
            raise ProtocolError(f"Field 'payload' must be a dictionary, got {type(payload).__name__}")

        self.version = version
        self.msg_type = msg_type if isinstance(msg_type, MsgType) else MsgType(msg_type)
        self.msg_id = msg_id
        self.timestamp_ms = ts
        self.payload = payload

    def to_dict(self) -> Dict[str, Any]:
        return {
            "version": self.version,
            "msg_type": self.msg_type.value,
            "msg_id": self.msg_id,
            "timestamp_ms": self.timestamp_ms,
            "payload": self.payload
        }

    def serialize(self) -> str:
        """Serialize message to a single line JSON string with newline terminator."""
        data = self.to_dict()
        line = json.dumps(data, separators=(',', ':')) + "\n"
        if len(line.encode('utf-8')) > MAX_MESSAGE_BYTES:
            raise ProtocolError(f"Serialized message exceeds {MAX_MESSAGE_BYTES} bytes limit")
        return line

    @classmethod
    def deserialize(cls, raw_line: str) -> 'ProtocolMessage':
        """Deserialize and validate raw JSON line."""
        clean = raw_line.strip()
        if not clean:
            raise ProtocolError("Empty message line")
        if len(clean.encode('utf-8')) > MAX_MESSAGE_BYTES:
            raise ProtocolError(f"Message exceeds maximum allowed length of {MAX_MESSAGE_BYTES} bytes")

        try:
            data = json.loads(clean)
        except Exception as e:
            raise ProtocolError(f"Malformed JSON: {e}")

        if not isinstance(data, dict):
            raise ProtocolError("JSON root must be an object")

        # Validate required top-level fields existence
        for field in ("version", "msg_type", "msg_id", "timestamp_ms", "payload"):
            if field not in data:
                raise ProtocolError(f"Missing required field: '{field}'")

        # Strict type & range validation for top-level fields
        v = data["version"]
        if type(v) is not int or isinstance(v, bool):
            raise ProtocolError(f"Field 'version' must be an integer, got {type(v).__name__}")
        if v != PROTOCOL_VERSION:
            raise ProtocolError(f"Unsupported protocol version: {v}, expected {PROTOCOL_VERSION}")

        mid = data["msg_id"]
        if type(mid) is not int or isinstance(mid, bool):
            raise ProtocolError(f"Field 'msg_id' must be an integer, got {type(mid).__name__}")
        if mid < 0:
            raise ProtocolError(f"Field 'msg_id' cannot be negative, got {mid}")

        ts = data["timestamp_ms"]
        if type(ts) is not int or isinstance(ts, bool):
            raise ProtocolError(f"Field 'timestamp_ms' must be an integer, got {type(ts).__name__}")
        if ts < 0:
            raise ProtocolError(f"Field 'timestamp_ms' cannot be negative, got {ts}")

        mtype_raw = data["msg_type"]
        if not isinstance(mtype_raw, str):
            raise ProtocolError(f"Field 'msg_type' must be a string, got {type(mtype_raw).__name__}")
        try:
            msg_type = MsgType(mtype_raw)
        except ValueError:
            raise ProtocolError(f"Unknown message type: '{mtype_raw}'")

        payload = data["payload"]
        if type(payload) is not dict:
            raise ProtocolError(f"Field 'payload' must be an object (dictionary), got {type(payload).__name__}")

        # Payload specific validation
        if msg_type == MsgType.COMMAND:
            if "command" not in payload or not isinstance(payload["command"], str):
                raise ProtocolError("Command payload must contain a string 'command' field")
            cmd_name = payload["command"]
            if cmd_name == "SET_MODE":
                if "mode" not in payload or payload["mode"] not in ("NONE", "MANUAL", "ASSISTED"):
                    raise ProtocolError(f"Invalid mode for SET_MODE: {payload.get('mode')}")
            elif cmd_name == "MOVE":
                if "action" not in payload or payload["action"] not in ("STOP", "TEST_FORWARD", "TEST_LEFT", "TEST_RIGHT"):
                    raise ProtocolError(f"Invalid action for MOVE: {payload.get('action')}")
                if "speed_pct" in payload:
                    spd = payload["speed_pct"]
                    if type(spd) is not int or isinstance(spd, bool) or spd < 0 or spd > 100:
                        raise ProtocolError(f"Invalid speed_pct: {spd}, must be integer 0..100")
        elif msg_type == MsgType.HEARTBEAT:
            if "seq" not in payload:
                raise ProtocolError("Heartbeat payload must contain 'seq'")
            seq = payload["seq"]
            if type(seq) is not int or isinstance(seq, bool) or seq < 0:
                raise ProtocolError(f"Heartbeat 'seq' must be a non-negative integer, got {seq}")
            if "ack" in payload and type(payload["ack"]) is not bool:
                raise ProtocolError(f"Heartbeat 'ack' must be boolean, got {type(payload['ack']).__name__}")
            if "status" in payload and payload["status"] not in ("ACCEPTED", "REJECTED"):
                raise ProtocolError(f"Heartbeat 'status' must be ACCEPTED or REJECTED, got {payload['status']}")
        elif msg_type == MsgType.RESPONSE:
            if "ack" in payload and (type(payload["ack"]) is not bool):
                raise ProtocolError(f"Response 'ack' must be a boolean, got {type(payload['ack']).__name__}")

        return cls(
            msg_type=msg_type,
            msg_id=mid,
            payload=payload,
            timestamp_ms=ts,
            version=v
        )


class FrameBuffer:
    """
    Robust stream framing buffer.
    Accumulates incoming bytes, extracts newline-terminated JSON lines,
    and protects against buffer overflows / oversized corrupted chunks.
    """
    def __init__(self, max_buffer_size: int = 2048, max_line_bytes: int = MAX_MESSAGE_BYTES):
        self.buffer = bytearray()
        self.max_buffer_size = max_buffer_size
        self.max_line_bytes = max_line_bytes

    def feed(self, data: bytes) -> List[str]:
        """Feed raw incoming bytes into buffer and return any complete message lines."""
        if not data:
            return []

        self.buffer.extend(data)

        # Buffer overflow guard: if buffer has no newline and exceeds max_buffer_size, clear it
        if len(self.buffer) > self.max_buffer_size and b'\n' not in self.buffer:
            self.buffer.clear()
            return []

        lines = []
        while b'\n' in self.buffer:
            idx = self.buffer.find(b'\n')
            raw_line = self.buffer[:idx]
            del self.buffer[:idx + 1]

            # Strip carriage returns if present
            line_bytes = raw_line.rstrip(b'\r')
            if not line_bytes:
                continue

            if len(line_bytes) > self.max_line_bytes:
                # Discard oversized frame cleanly
                continue

            try:
                decoded = line_bytes.decode('utf-8')
                lines.append(decoded)
            except UnicodeDecodeError:
                # Discard frame with invalid character encoding
                continue

        return lines
