"""
Comprehensive Unit tests for AutoChair Standalone Serial Protocol Framing & Parsing.
Covers strict type checking, negative numbers, boolean rejection, buffer framing,
oversized frame discard with subsequent valid frame recovery, and error handling.
"""

import unittest
import json
import time
import sys
import os

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'raspberry_pi')))

from protocol import (
    ProtocolMessage,
    MsgType,
    State,
    Mode,
    FrameBuffer,
    ProtocolError,
    PROTOCOL_VERSION,
    MAX_MESSAGE_BYTES
)

class TestProtocolMessage(unittest.TestCase):
    def test_valid_serialization(self):
        msg = ProtocolMessage(
            msg_type=MsgType.COMMAND,
            msg_id=1001,
            payload={"command": "PING"},
            timestamp_ms=123456
        )
        serialized = msg.serialize()
        self.assertTrue(serialized.endswith("\n"))
        data = json.loads(serialized.strip())
        self.assertEqual(data["version"], PROTOCOL_VERSION)
        self.assertEqual(data["msg_type"], "COMMAND")
        self.assertEqual(data["msg_id"], 1001)
        self.assertEqual(data["payload"]["command"], "PING")

    def test_valid_deserialization(self):
        raw = '{"version":1,"msg_type":"RESPONSE","msg_id":1001,"timestamp_ms":123456,"payload":{"ack":true,"result":"ACCEPTED","state":"READY","mode":"NONE"}}\n'
        msg = ProtocolMessage.deserialize(raw)
        self.assertEqual(msg.version, 1)
        self.assertEqual(msg.msg_type, MsgType.RESPONSE)
        self.assertEqual(msg.msg_id, 1001)
        self.assertTrue(msg.payload["ack"])
        self.assertEqual(msg.payload["result"], "ACCEPTED")

    def test_missing_field_rejected(self):
        raw = '{"version":1,"msg_type":"COMMAND","payload":{}}\n'
        with self.assertRaises(ProtocolError):
            ProtocolMessage.deserialize(raw)

    def test_unsupported_version_rejected(self):
        raw = '{"version":99,"msg_type":"COMMAND","msg_id":1,"timestamp_ms":100,"payload":{"command":"PING"}}\n'
        with self.assertRaises(ProtocolError):
            ProtocolMessage.deserialize(raw)

    def test_boolean_version_rejected(self):
        raw = '{"version":true,"msg_type":"COMMAND","msg_id":1,"timestamp_ms":100,"payload":{"command":"PING"}}\n'
        with self.assertRaises(ProtocolError):
            ProtocolMessage.deserialize(raw)

    def test_boolean_msg_id_rejected(self):
        raw = '{"version":1,"msg_type":"COMMAND","msg_id":false,"timestamp_ms":100,"payload":{"command":"PING"}}\n'
        with self.assertRaises(ProtocolError):
            ProtocolMessage.deserialize(raw)

    def test_string_msg_id_rejected(self):
        raw = '{"version":1,"msg_type":"COMMAND","msg_id":"1001","timestamp_ms":100,"payload":{"command":"PING"}}\n'
        with self.assertRaises(ProtocolError):
            ProtocolMessage.deserialize(raw)

    def test_negative_msg_id_rejected(self):
        raw = '{"version":1,"msg_type":"COMMAND","msg_id":-5,"timestamp_ms":100,"payload":{"command":"PING"}}\n'
        with self.assertRaises(ProtocolError):
            ProtocolMessage.deserialize(raw)

    def test_boolean_timestamp_rejected(self):
        raw = '{"version":1,"msg_type":"COMMAND","msg_id":10,"timestamp_ms":true,"payload":{"command":"PING"}}\n'
        with self.assertRaises(ProtocolError):
            ProtocolMessage.deserialize(raw)

    def test_negative_timestamp_rejected(self):
        raw = '{"version":1,"msg_type":"COMMAND","msg_id":10,"timestamp_ms":-100,"payload":{"command":"PING"}}\n'
        with self.assertRaises(ProtocolError):
            ProtocolMessage.deserialize(raw)

    def test_invalid_msg_type_rejected(self):
        raw = '{"version":1,"msg_type":"UNKNOWN_TYPE_XYZ","msg_id":1,"timestamp_ms":100,"payload":{"command":"PING"}}\n'
        with self.assertRaises(ProtocolError):
            ProtocolMessage.deserialize(raw)

    def test_malformed_json_rejected(self):
        raw = '{"version": 1, msg_type: COMMAND}\n'
        with self.assertRaises(ProtocolError):
            ProtocolMessage.deserialize(raw)

    def test_oversized_message_rejected(self):
        huge_payload = {"command": "PING", "data": "x" * (MAX_MESSAGE_BYTES + 50)}
        msg = ProtocolMessage(msg_type=MsgType.COMMAND, msg_id=1, payload=huge_payload)
        with self.assertRaises(ProtocolError):
            msg.serialize()

    def test_speed_pct_range_validation(self):
        # speed_pct > 100 rejected
        raw_over = '{"version":1,"msg_type":"COMMAND","msg_id":1,"timestamp_ms":100,"payload":{"command":"MOVE","action":"TEST_FORWARD","speed_pct":105}}\n'
        with self.assertRaises(ProtocolError):
            ProtocolMessage.deserialize(raw_over)

        # speed_pct < 0 rejected
        raw_under = '{"version":1,"msg_type":"COMMAND","msg_id":1,"timestamp_ms":100,"payload":{"command":"MOVE","action":"TEST_FORWARD","speed_pct":-10}}\n'
        with self.assertRaises(ProtocolError):
            ProtocolMessage.deserialize(raw_under)

        # speed_pct boolean rejected
        raw_bool = '{"version":1,"msg_type":"COMMAND","msg_id":1,"timestamp_ms":100,"payload":{"command":"MOVE","action":"TEST_FORWARD","speed_pct":true}}\n'
        with self.assertRaises(ProtocolError):
            ProtocolMessage.deserialize(raw_bool)


    def test_heartbeat_ack_accepted_and_rejected_parsing(self):
        # 1. Accepted heartbeat ack
        raw_ok = json.dumps({
            "version": 1,
            "msg_type": "HEARTBEAT",
            "msg_id": 101,
            "timestamp_ms": 1000,
            "payload": {
                "seq": 10,
                "ack": True,
                "status": "ACCEPTED",
                "reason": "HEARTBEAT_OK",
                "state": "READY"
            }
        })
        msg_ok = ProtocolMessage.deserialize(raw_ok)
        self.assertEqual(msg_ok.msg_type, MsgType.HEARTBEAT)
        self.assertTrue(msg_ok.payload["ack"])
        self.assertEqual(msg_ok.payload["status"], "ACCEPTED")

        # 2. Rejected heartbeat ack (e.g. duplicate or out-of-order)
        raw_rej = json.dumps({
            "version": 1,
            "msg_type": "HEARTBEAT",
            "msg_id": 102,
            "timestamp_ms": 1050,
            "payload": {
                "seq": 10,
                "ack": False,
                "status": "REJECTED",
                "reason": "STALE_OR_DUPLICATE_HEARTBEAT",
                "state": "READY"
            }
        })
        msg_rej = ProtocolMessage.deserialize(raw_rej)
        self.assertFalse(msg_rej.payload["ack"])
        self.assertEqual(msg_rej.payload["status"], "REJECTED")

        # 3. Invalid non-bool ack rejected
        raw_invalid_ack = json.dumps({
            "version": 1,
            "msg_type": "HEARTBEAT",
            "msg_id": 103,
            "timestamp_ms": 1100,
            "payload": {"seq": 10, "ack": "true"}
        })
        with self.assertRaises(ProtocolError):
            ProtocolMessage.deserialize(raw_invalid_ack)

class TestFrameBuffer(unittest.TestCase):
    def setUp(self):
        self.fb = FrameBuffer()

    def test_single_complete_message(self):
        line = '{"version":1,"msg_type":"COMMAND","msg_id":1,"timestamp_ms":100,"payload":{"command":"PING"}}\n'
        lines = self.fb.feed(line.encode('utf-8'))
        self.assertEqual(len(lines), 1)
        self.assertEqual(lines[0], line.strip())

    def test_partial_reads(self):
        chunk1 = b'{"version":1,"msg_type":'
        chunk2 = b'"COMMAND","msg_id":2,"timestamp_ms":200,'
        chunk3 = b'"payload":{"command":"PING"}}\n'

        self.assertEqual(self.fb.feed(chunk1), [])
        self.assertEqual(self.fb.feed(chunk2), [])
        lines = self.fb.feed(chunk3)
        self.assertEqual(len(lines), 1)
        self.assertIn('"msg_id":2', lines[0])

    def test_multiple_messages_in_one_read(self):
        raw = b'{"msg_id":1}\n{"msg_id":2}\n{"msg_id":3}\n'
        lines = self.fb.feed(raw)
        self.assertEqual(len(lines), 3)
        self.assertEqual(lines[0], '{"msg_id":1}')
        self.assertEqual(lines[1], '{"msg_id":2}')
        self.assertEqual(lines[2], '{"msg_id":3}')

    def test_oversized_frame_discarded_followed_by_valid_frame(self):
        # Stream contains oversized garbage (>512 bytes) ending in newline, immediately followed by valid frame
        oversized = (b'x' * 600) + b'\n'
        valid = b'{"version":1,"msg_type":"COMMAND","msg_id":99,"timestamp_ms":100,"payload":{"command":"PING"}}\n'
        combined = oversized + valid

        lines = self.fb.feed(combined)
        self.assertEqual(len(lines), 1)
        self.assertIn('"msg_id":99', lines[0])

    def test_invalid_utf8_discarded(self):
        invalid_bytes = b'\xff\xfe\xfd\n'
        valid = b'{"version":1,"msg_type":"COMMAND","msg_id":42,"timestamp_ms":100,"payload":{"command":"PING"}}\n'
        lines = self.fb.feed(invalid_bytes + valid)
        self.assertEqual(len(lines), 1)
        self.assertIn('"msg_id":42', lines[0])

if __name__ == '__main__':
    unittest.main()
