"""
Tests for VoiceCommandParser.

Verifies:
- Text normalization (case, whitespace)
- All recognized FORWARD phrase variants
- All recognized STOP phrase variants
- START intent recognized and produces NO InputCommand
- Unknown phrases safely rejected
- InputCommand source is always InputSource.VOICE for motion intents
- InputCommand command values match existing VALID_COMMANDS

No hardware required. No audio devices accessed.
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
# FORWARD intent
# ---------------------------------------------------------------------------

@pytest.mark.parametrize("phrase", [
    "forward",
    "move forward",
    "go forward",
    "go ahead",
    "move ahead",
    "drive forward",
])
def test_forward_phrases_recognized(parser: VoiceCommandParser, phrase: str):
    result = parser.parse(phrase)
    assert result.intent == VoiceIntent.MOVE_FORWARD


def test_forward_produces_input_command(parser: VoiceCommandParser):
    result = parser.parse("move forward")
    assert result.input_command is not None
    assert result.input_command.command == "FORWARD"


def test_forward_source_is_voice(parser: VoiceCommandParser):
    result = parser.parse("forward")
    assert result.input_command is not None
    assert result.input_command.source == InputSource.VOICE


def test_forward_command_is_in_valid_commands(parser: VoiceCommandParser):
    """FORWARD must pass the existing InputCommandValidator unchanged."""
    result = parser.parse("move forward")
    assert result.input_command is not None
    assert result.input_command.command in VALID_COMMANDS


def test_forward_case_insensitive(parser: VoiceCommandParser):
    result = parser.parse("MOVE FORWARD")
    assert result.intent == VoiceIntent.MOVE_FORWARD


def test_forward_trims_whitespace(parser: VoiceCommandParser):
    result = parser.parse("  move forward  ")
    assert result.intent == VoiceIntent.MOVE_FORWARD


# ---------------------------------------------------------------------------
# STOP intent
# ---------------------------------------------------------------------------

@pytest.mark.parametrize("phrase", [
    "stop",
    "halt",
    "brake",
    "please stop",
    "stop the chair",
    "please stop the chair",
])
def test_stop_phrases_recognized(parser: VoiceCommandParser, phrase: str):
    result = parser.parse(phrase)
    assert result.intent == VoiceIntent.STOP


def test_stop_produces_input_command(parser: VoiceCommandParser):
    result = parser.parse("stop")
    assert result.input_command is not None
    assert result.input_command.command == "STOP"


def test_stop_source_is_voice(parser: VoiceCommandParser):
    result = parser.parse("halt")
    assert result.input_command is not None
    assert result.input_command.source == InputSource.VOICE


def test_stop_command_is_in_valid_commands(parser: VoiceCommandParser):
    """STOP must pass the existing InputCommandValidator unchanged."""
    result = parser.parse("stop")
    assert result.input_command is not None
    assert result.input_command.command in VALID_COMMANDS


def test_stop_case_insensitive(parser: VoiceCommandParser):
    result = parser.parse("STOP")
    assert result.intent == VoiceIntent.STOP


# ---------------------------------------------------------------------------
# START intent — design gap verification
# ---------------------------------------------------------------------------

@pytest.mark.parametrize("phrase", [
    "start",
    "begin",
    "activate",
    "start the chair",
    "please start",
])
def test_start_phrases_recognized(parser: VoiceCommandParser, phrase: str):
    result = parser.parse(phrase)
    assert result.intent == VoiceIntent.START


def test_start_produces_no_input_command(parser: VoiceCommandParser):
    """
    START must NOT produce an InputCommand.

    This is the documented design gap: START has no motion mapping in the
    existing architecture. Producing an InputCommand would inject an invalid
    command into the pipeline.
    """
    result = parser.parse("start")
    assert result.input_command is None


def test_start_command_not_silently_aliased_to_forward(parser: VoiceCommandParser):
    """START must not silently become FORWARD."""
    result = parser.parse("start")
    assert result.intent != VoiceIntent.MOVE_FORWARD


def test_start_parse_result_raw_text(parser: VoiceCommandParser):
    result = parser.parse("start")
    assert result.raw_text == "start"


# ---------------------------------------------------------------------------
# UNKNOWN intent
# ---------------------------------------------------------------------------

@pytest.mark.parametrize("phrase", [
    "fly to the moon",
    "open the pod bay doors",
    "",
    "   ",
    "blah blah",
    "move backward",   # not in Phase 1 — must be rejected, not invented
])
def test_unknown_phrases_rejected(parser: VoiceCommandParser, phrase: str):
    result = parser.parse(phrase)
    assert result.intent == VoiceIntent.UNKNOWN


def test_unknown_produces_no_input_command(parser: VoiceCommandParser):
    result = parser.parse("fly to the moon")
    assert result.input_command is None


def test_unknown_does_not_raise(parser: VoiceCommandParser):
    """Unknown commands must fail gracefully, never raise."""
    result = parser.parse("xyzzy nonsense phrase")
    assert result.intent == VoiceIntent.UNKNOWN


# ---------------------------------------------------------------------------
# ParseResult structure
# ---------------------------------------------------------------------------

def test_parse_result_raw_text_is_normalized(parser: VoiceCommandParser):
    result = parser.parse("  MOVE FORWARD  ")
    assert result.raw_text == "move forward"


def test_parse_result_has_all_fields(parser: VoiceCommandParser):
    result = parser.parse("stop")
    assert hasattr(result, "intent")
    assert hasattr(result, "input_command")
    assert hasattr(result, "raw_text")


# ---------------------------------------------------------------------------
# Integration: voice command enters existing validation chain
# ---------------------------------------------------------------------------

def test_forward_command_passes_existing_validator(parser: VoiceCommandParser):
    """
    An InputCommand produced by the parser must pass InputCommandValidator
    without modification. This verifies the voice pipeline integrates
    correctly with the existing architecture.
    """
    validator = InputCommandValidator()
    result = parser.parse("move forward")
    assert result.input_command is not None
    # Must not raise
    validator.validate(result.input_command)


def test_stop_command_passes_existing_validator(parser: VoiceCommandParser):
    validator = InputCommandValidator()
    result = parser.parse("stop")
    assert result.input_command is not None
    validator.validate(result.input_command)
