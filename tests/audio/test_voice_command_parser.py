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
# START intent (English + Hindi/Hinglish + Devanagari)
# ---------------------------------------------------------------------------

@pytest.mark.parametrize("phrase", [
    "start",
    "chalo",
    "चलो",
    "begin",
    "activate",
    "start the chair",
    "please start",
])
def test_start_phrases_recognized(parser: VoiceCommandParser, phrase: str):
    result = parser.parse(phrase)
    assert result.intent == VoiceIntent.START
    assert result.input_command is None  # System intent, no motion command


def test_start_hindi_aliases(parser: VoiceCommandParser):
    for phrase in ["chalo", "चलो"]:
        result = parser.parse(phrase)
        assert result.intent == VoiceIntent.START
        assert result.input_command is None


def test_start_case_and_whitespace(parser: VoiceCommandParser):
    assert parser.parse("  CHALO  ").intent == VoiceIntent.START
    assert parser.parse("  START  ").intent == VoiceIntent.START
    assert parser.parse("  चलो  ").intent == VoiceIntent.START


# ---------------------------------------------------------------------------
# STOP intent (English + Hindi/Hinglish + Devanagari)
# ---------------------------------------------------------------------------

@pytest.mark.parametrize("phrase", [
    "stop",
    "ruko",
    "rukko",
    "रुको",
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
    for phrase in ["ruko", "rukko", "रुको"]:
        result = parser.parse(phrase)
        assert result.intent == VoiceIntent.STOP
        assert result.input_command is not None
        assert result.input_command.command == "STOP"


def test_stop_case_and_whitespace(parser: VoiceCommandParser):
    assert parser.parse("  RUKO  ").intent == VoiceIntent.STOP
    assert parser.parse("  Rukko  ").intent == VoiceIntent.STOP
    assert parser.parse("  रुको  ").intent == VoiceIntent.STOP


# ---------------------------------------------------------------------------
# FORWARD intent (English + Hindi/Hinglish + Devanagari)
# ---------------------------------------------------------------------------

@pytest.mark.parametrize("phrase", [
    "forward",
    "move forward",
    "aage chalo",
    "aage jao",
    "आगे चल",
    "आगे चलो",
    "आगे जाओ",
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
    for phrase in ["aage chalo", "aage jao", "आगे चल", "आगे चलो", "आगे जाओ"]:
        result = parser.parse(phrase)
        assert result.intent == VoiceIntent.MOVE_FORWARD
        assert result.input_command is not None
        assert result.input_command.command == "FORWARD"


def test_forward_case_and_whitespace(parser: VoiceCommandParser):
    assert parser.parse("  AAGE CHALO  ").intent == VoiceIntent.MOVE_FORWARD
    assert parser.parse("  Aage Jao  ").intent == VoiceIntent.MOVE_FORWARD
    assert parser.parse("  आगे चल  ").intent == VoiceIntent.MOVE_FORWARD


# ---------------------------------------------------------------------------
# REVERSE intent (English + Hindi/Hinglish + Devanagari)
# ---------------------------------------------------------------------------

@pytest.mark.parametrize("phrase", [
    "backward",
    "move backward",
    "reverse",
    "piche chalo",
    "peeche chalo",
    "piche jao",
    "peeche jao",
    "पीछे चलो",
    "पीछे जाओ",
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
    for phrase in ["piche chalo", "peeche chalo", "piche jao", "peeche jao", "पीछे चलो", "पीछे जाओ"]:
        result = parser.parse(phrase)
        assert result.intent == VoiceIntent.REVERSE
        assert result.input_command is not None
        assert result.input_command.command == "REVERSE"


def test_reverse_case_and_whitespace(parser: VoiceCommandParser):
    assert parser.parse("  PEECHE CHALO  ").intent == VoiceIntent.REVERSE
    assert parser.parse("  Piche Jao  ").intent == VoiceIntent.REVERSE
    assert parser.parse("  पीछे चलो  ").intent == VoiceIntent.REVERSE


# ---------------------------------------------------------------------------
# LEFT intent (English + Hindi/Hinglish + Devanagari)
# ---------------------------------------------------------------------------

@pytest.mark.parametrize("phrase", [
    "left",
    "turn left",
    "go left",
    "move left",
    "left chlo",
    "left chalo",
    "baaye chalo",
    "baaya chalo",
    "बायें चलो",
    "बायाँ चलो",
])
def test_left_phrases_recognized(parser: VoiceCommandParser, phrase: str):
    result = parser.parse(phrase)
    assert result.intent == VoiceIntent.LEFT
    assert result.input_command is not None
    assert result.input_command.command == "LEFT"
    assert result.input_command.source == InputSource.VOICE


def test_left_hindi_aliases(parser: VoiceCommandParser):
    for phrase in ["left chlo", "left chalo", "baaye chalo", "baaya chalo", "बायें चलो", "बायाँ चलो"]:
        result = parser.parse(phrase)
        assert result.intent == VoiceIntent.LEFT
        assert result.input_command is not None
        assert result.input_command.command == "LEFT"


def test_left_case_and_whitespace(parser: VoiceCommandParser):
    assert parser.parse("  LEFT CHLO  ").intent == VoiceIntent.LEFT
    assert parser.parse("  LEFT CHALO  ").intent == VoiceIntent.LEFT
    assert parser.parse("  BAAYE CHALO  ").intent == VoiceIntent.LEFT
    assert parser.parse("  Baaya Chalo  ").intent == VoiceIntent.LEFT
    assert parser.parse("  बायें चलो  ").intent == VoiceIntent.LEFT


# ---------------------------------------------------------------------------
# RIGHT intent (English + Hindi/Hinglish + Devanagari)
# ---------------------------------------------------------------------------

@pytest.mark.parametrize("phrase", [
    "right",
    "turn right",
    "go right",
    "move right",
    "right chlo",
    "right chalo",
    "daaye chalo",
    "daaya chalo",
    "दायें चलो",
    "दायाँ चलो",
    "दाएं चलो",
])
def test_right_phrases_recognized(parser: VoiceCommandParser, phrase: str):
    result = parser.parse(phrase)
    assert result.intent == VoiceIntent.RIGHT
    assert result.input_command is not None
    assert result.input_command.command == "RIGHT"
    assert result.input_command.source == InputSource.VOICE


def test_right_hindi_aliases(parser: VoiceCommandParser):
    for phrase in ["right chlo", "right chalo", "daaye chalo", "daaya chalo", "दायें चलो", "दायाँ चलो", "दाएं चलो"]:
        result = parser.parse(phrase)
        assert result.intent == VoiceIntent.RIGHT
        assert result.input_command is not None
        assert result.input_command.command == "RIGHT"


def test_right_case_and_whitespace(parser: VoiceCommandParser):
    assert parser.parse("  RIGHT CHLO  ").intent == VoiceIntent.RIGHT
    assert parser.parse("  RIGHT CHALO  ").intent == VoiceIntent.RIGHT
    assert parser.parse("  DAAYE CHALO  ").intent == VoiceIntent.RIGHT
    assert parser.parse("  Daaya Chalo  ").intent == VoiceIntent.RIGHT
    assert parser.parse("  दायें चलो  ").intent == VoiceIntent.RIGHT
    assert parser.parse("  दाएं चलो  ").intent == VoiceIntent.RIGHT


# ---------------------------------------------------------------------------
# UNKNOWN intent & [unk] handling (and misrecognition safety rejection)
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
    "बाईस चलो",     # Vosk misrecognition for "baaye chalo" — must NOT map to LEFT
])
def test_unknown_phrases_rejected(parser: VoiceCommandParser, phrase: str):
    result = parser.parse(phrase)
    assert result.intent == VoiceIntent.UNKNOWN
    assert result.input_command is None


def test_bais_chalo_misrecognition_is_safely_rejected(parser: VoiceCommandParser):
    """
    Vosk misrecognized 'baaye chalo' as 'बाईस चलो'.
    Safety rule: this must remain UNKNOWN and never trigger movement.
    """
    result = parser.parse("बाईस चलो")
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
    "आगे चल",
    "आगे चलो",
    "backward",
    "move backward",
    "reverse",
    "piche chalo",
    "peeche chalo",
    "piche jao",
    "peeche jao",
    "पीछे चलो",
    "left",
    "turn left",
    "left chlo",
    "left chalo",
    "baaye chalo",
    "baaya chalo",
    "बायें चलो",
    "right",
    "turn right",
    "right chlo",
    "right chalo",
    "daaye chalo",
    "daaya chalo",
    "दायें चलो",
    "दाएं चलो",
    "stop",
    "ruko",
    "rukko",
    "रुको",
])
def test_all_motion_commands_pass_validator(parser: VoiceCommandParser, phrase: str):
    validator = InputCommandValidator()
    result = parser.parse(phrase)
    assert result.input_command is not None
    assert result.input_command.command in VALID_COMMANDS
    # Must not raise
    validator.validate(result.input_command)
