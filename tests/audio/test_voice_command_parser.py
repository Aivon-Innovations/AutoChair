"""
Tests for VoiceCommandParser — English and Hindi/Hinglish voice commands.

Verifies:
- Text normalization (case, whitespace)
- All recognized START phrases (start, chalo, etc.)
- All recognized STOP phrases (stop, ruko, rukko, etc.)
- All recognized FORWARD phrases (forward, move forward, aage chalo, aage jao, etc.)
- All recognized REVERSE phrases (backward, move backward, reverse, piche chalo, peeche chalo, piche jao, peeche jao, etc.)
- All recognized LEFT phrases (left, turn left, left chalo, etc.)
- All recognized RIGHT phrases (right, turn right, right chalo, etc.)
- Unknown phrases and [unk] safely rejected
- InputCommand source is always InputSource.VOICE for motion intents
- InputCommand command values match existing VALID_COMMANDS (FORWARD, REVERSE, LEFT, RIGHT, STOP)
"""

import pytest

from autochair.audio.commands.parser import (
    ParseResult,
    VoiceCommandParser,
    VoiceIntent,
)
from autochair.input.command import InputSource
from autochair.input.validator import InputCommandValidator, VALID_COMMANDS


@pytest.fixture()
def parser() -> VoiceCommandParser:
    return VoiceCommandParser()


# ---------------------------------------------------------------------------
# START intent (English + Hindi/Hinglish)
# ---------------------------------------------------------------------------

@pytest.mark.parametrize("phrase", [
    "start",
    "chalo",
    "begin",
    "activate",
    "start the chair",
    "please start",
])
def test_start_phrases_recognized(parser: VoiceCommandParser, phrase: str):
    result = parser.parse(phrase)
    assert result.intent == VoiceIntent.START
    assert result.input_command is None  # System intent, no motion command


def test_start_hindi_alias_chalo(parser: VoiceCommandParser):
    result = parser.parse("chalo")
    assert result.intent == VoiceIntent.START
    assert result.input_command is None


def test_start_case_and_whitespace(parser: VoiceCommandParser):
    assert parser.parse("  CHALO  ").intent == VoiceIntent.START
    assert parser.parse("  START  ").intent == VoiceIntent.START


# ---------------------------------------------------------------------------
# STOP intent (English + Hindi/Hinglish)
# ---------------------------------------------------------------------------

@pytest.mark.parametrize("phrase", [
    "stop",
    "ruko",
    "rukko",
    "halt",
    "brake",
    "please stop",
    "stop the chair",
    "please stop the chair",
])
def test_stop_phrases_recognized(parser: VoiceCommandParser, phrase: str):
    result = parser.parse(phrase)
    assert result.intent == VoiceIntent.STOP
    assert result.input_command is not None
    assert result.input_command.command == "STOP"
    assert result.input_command.source == InputSource.VOICE


def test_stop_hindi_aliases(parser: VoiceCommandParser):
    for phrase in ["ruko", "rukko"]:
        result = parser.parse(phrase)
        assert result.intent == VoiceIntent.STOP
        assert result.input_command is not None
        assert result.input_command.command == "STOP"


def test_stop_case_and_whitespace(parser: VoiceCommandParser):
    assert parser.parse("  RUKO  ").intent == VoiceIntent.STOP
    assert parser.parse("  Rukko  ").intent == VoiceIntent.STOP


# ---------------------------------------------------------------------------
# FORWARD intent (English + Hindi/Hinglish)
# ---------------------------------------------------------------------------

@pytest.mark.parametrize("phrase", [
    "forward",
    "move forward",
    "aage chalo",
    "aage jao",
    "go forward",
    "go ahead",
    "move ahead",
    "drive forward",
])
def test_forward_phrases_recognized(parser: VoiceCommandParser, phrase: str):
    result = parser.parse(phrase)
    assert result.intent == VoiceIntent.MOVE_FORWARD
    assert result.input_command is not None
    assert result.input_command.command == "FORWARD"
    assert result.input_command.source == InputSource.VOICE


def test_forward_hindi_aliases(parser: VoiceCommandParser):
    for phrase in ["aage chalo", "aage jao"]:
        result = parser.parse(phrase)
        assert result.intent == VoiceIntent.MOVE_FORWARD
        assert result.input_command is not None
        assert result.input_command.command == "FORWARD"


def test_forward_case_and_whitespace(parser: VoiceCommandParser):
    assert parser.parse("  AAGE CHALO  ").intent == VoiceIntent.MOVE_FORWARD
    assert parser.parse("  Aage Jao  ").intent == VoiceIntent.MOVE_FORWARD


# ---------------------------------------------------------------------------
# REVERSE intent (English + Hindi/Hinglish)
# ---------------------------------------------------------------------------

@pytest.mark.parametrize("phrase", [
    "backward",
    "move backward",
    "reverse",
    "piche chalo",
    "peeche chalo",
    "piche jao",
    "peeche jao",
    "go backward",
    "move reverse",
])
def test_reverse_phrases_recognized(parser: VoiceCommandParser, phrase: str):
    result = parser.parse(phrase)
    assert result.intent == VoiceIntent.REVERSE
    assert result.input_command is not None
    assert result.input_command.command == "REVERSE"
    assert result.input_command.source == InputSource.VOICE


def test_reverse_hindi_aliases(parser: VoiceCommandParser):
    for phrase in ["piche chalo", "peeche chalo", "piche jao", "peeche jao"]:
        result = parser.parse(phrase)
        assert result.intent == VoiceIntent.REVERSE
        assert result.input_command is not None
        assert result.input_command.command == "REVERSE"


def test_reverse_case_and_whitespace(parser: VoiceCommandParser):
    assert parser.parse("  PEECHE CHALO  ").intent == VoiceIntent.REVERSE
    assert parser.parse("  Piche Jao  ").intent == VoiceIntent.REVERSE


# ---------------------------------------------------------------------------
# LEFT intent (English + Hindi/Hinglish)
# ---------------------------------------------------------------------------

@pytest.mark.parametrize("phrase", [
    "left",
    "turn left",
    "left chalo",
    "go left",
    "move left",
])
def test_left_phrases_recognized(parser: VoiceCommandParser, phrase: str):
    result = parser.parse(phrase)
    assert result.intent == VoiceIntent.LEFT
    assert result.input_command is not None
    assert result.input_command.command == "LEFT"
    assert result.input_command.source == InputSource.VOICE


def test_left_hindi_alias(parser: VoiceCommandParser):
    result = parser.parse("left chalo")
    assert result.intent == VoiceIntent.LEFT
    assert result.input_command is not None
    assert result.input_command.command == "LEFT"


def test_left_case_and_whitespace(parser: VoiceCommandParser):
    assert parser.parse("  LEFT CHALO  ").intent == VoiceIntent.LEFT


# ---------------------------------------------------------------------------
# RIGHT intent (English + Hindi/Hinglish)
# ---------------------------------------------------------------------------

@pytest.mark.parametrize("phrase", [
    "right",
    "turn right",
    "right chalo",
    "go right",
    "move right",
])
def test_right_phrases_recognized(parser: VoiceCommandParser, phrase: str):
    result = parser.parse(phrase)
    assert result.intent == VoiceIntent.RIGHT
    assert result.input_command is not None
    assert result.input_command.command == "RIGHT"
    assert result.input_command.source == InputSource.VOICE


def test_right_hindi_alias(parser: VoiceCommandParser):
    result = parser.parse("right chalo")
    assert result.intent == VoiceIntent.RIGHT
    assert result.input_command is not None
    assert result.input_command.command == "RIGHT"


def test_right_case_and_whitespace(parser: VoiceCommandParser):
    assert parser.parse("  RIGHT CHALO  ").intent == VoiceIntent.RIGHT


# ---------------------------------------------------------------------------
# UNKNOWN intent & [unk] handling
# ---------------------------------------------------------------------------

@pytest.mark.parametrize("phrase", [
    "fly to the moon",
    "open the pod bay doors",
    "",
    "   ",
    "blah blah",
    "[unk]",
    "jump",
    "dance",
])
def test_unknown_phrases_rejected(parser: VoiceCommandParser, phrase: str):
    result = parser.parse(phrase)
    assert result.intent == VoiceIntent.UNKNOWN
    assert result.input_command is None


def test_unknown_does_not_raise(parser: VoiceCommandParser):
    result = parser.parse("xyzzy nonsense phrase")
    assert result.intent == VoiceIntent.UNKNOWN


# ---------------------------------------------------------------------------
# Validator Integration
# ---------------------------------------------------------------------------

@pytest.mark.parametrize("phrase", [
    "forward",
    "move forward",
    "aage chalo",
    "aage jao",
    "backward",
    "move backward",
    "reverse",
    "piche chalo",
    "peeche chalo",
    "piche jao",
    "peeche jao",
    "left",
    "turn left",
    "left chalo",
    "right",
    "turn right",
    "right chalo",
    "stop",
    "ruko",
    "rukko",
])
def test_all_motion_commands_pass_validator(parser: VoiceCommandParser, phrase: str):
    validator = InputCommandValidator()
    result = parser.parse(phrase)
    assert result.input_command is not None
    assert result.input_command.command in VALID_COMMANDS
    # Must not raise
    validator.validate(result.input_command)
