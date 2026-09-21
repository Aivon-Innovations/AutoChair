"""
VoiceCommandParser — converts recognized speech text into AutoChair InputCommands.

This is the critical safety boundary between the voice pipeline and the
existing AutoChair input architecture. Its responsibility is:

  1. Normalize raw recognized text (lowercase, strip whitespace)
  2. Map natural language phrases to typed VoiceIntent values
  3. For motion intents: construct a valid InputCommand(source=VOICE, ...)
     that can enter the existing InputCommandValidator → CommandMapper →
     SafetyManager chain.
  4. For system intents (START): recognize them explicitly but produce NO
     InputCommand. The START gap is documented here — it is a system-level
     command that does not yet have a corresponding motion architecture.
  5. For unknown phrases: reject safely, produce no InputCommand, raise an alert.

IMPORTANT — Safety contract:
  Voice NEVER produces a MotionCommand directly.
  Voice NEVER bypasses InputCommandValidator.
  Voice NEVER bypasses SafetyManager.
  Voice produces ONLY an InputCommand, which then flows through the
  existing validated pipeline.

START command design gap:
  The PRD defines START as a Phase 1 voice command. However, the existing
  VALID_COMMANDS set does not include START, and CommandMapper has no
  mapping for it. Silently aliasing START → FORWARD would be architecturally
  dishonest and potentially unsafe (the chair might not be in a state where
  forward motion is appropriate).

  Resolution: START is represented as VoiceIntent.START. ParseResult.input_command
  is None for this intent. The AudioManager logs a START event and may in
  future trigger a system-level "activate" state change, but it will never
  produce motion without passing through the safety layer.
"""

from __future__ import annotations

from dataclasses import dataclass
from enum import Enum

from autochair.input.command import InputCommand, InputSource
from autochair.utils.logger import get_logger


logger = get_logger("autochair.audio.commands.parser")


class VoiceIntent(str, Enum):
    """
    Typed representation of a user's voice intent.

    MOVE_FORWARD    — user wants the chair to move forward
                      maps to InputCommand(command="FORWARD")
    STOP            — user wants the chair to stop
                      maps to InputCommand(command="STOP")
    START           — user wants to activate/start the system
                      DESIGN GAP: no corresponding motion command exists yet.
                      Produces no InputCommand. Logged as a system intent.
    UNKNOWN         — utterance was not recognized as any valid intent
                      Produces no InputCommand. Triggers a COMMAND_REJECTED alert.
    """

    MOVE_FORWARD = "MOVE_FORWARD"
    STOP = "STOP"
    START = "START"
    UNKNOWN = "UNKNOWN"


@dataclass
class ParseResult:
    """
    Result of parsing a recognized speech utterance.

    intent:        The classified voice intent.
    input_command: The AutoChair InputCommand to inject into the existing
                   input pipeline, or None if this intent does not produce
                   a motion command (START, UNKNOWN).
    raw_text:      The normalized text that was parsed.

    Usage:
        result = parser.parse("move forward")
        if result.input_command is not None:
            validator.validate(result.input_command)
            motion = mapper.map(result.input_command)
    """

    intent: VoiceIntent
    input_command: InputCommand | None
    raw_text: str


# ---------------------------------------------------------------------------
# Phrase → intent mapping tables
# ---------------------------------------------------------------------------

_FORWARD_PHRASES: frozenset[str] = frozenset({
    "forward",
    "move forward",
    "go forward",
    "go ahead",
    "move ahead",
    "drive forward",
})

_STOP_PHRASES: frozenset[str] = frozenset({
    "stop",
    "halt",
    "brake",
    "please stop",
    "stop the chair",
    "please stop the chair",
})

_START_PHRASES: frozenset[str] = frozenset({
    "start",
    "begin",
    "activate",
    "start the chair",
    "please start",
})


def _normalize(text: str) -> str:
    """Lowercase and strip all surrounding whitespace from text."""
    return text.strip().lower()


class VoiceCommandParser:
    """
    Converts recognized speech text to a typed ParseResult.

    The parser normalizes input, applies phrase matching, and returns
    a ParseResult that either contains an InputCommand (for motion intents)
    or None (for system intents and unknown phrases).

    This class has no dependency on hardware and is fully testable
    without any audio devices.
    """

    def parse(self, recognized_text: str) -> ParseResult:
        """
        Parse a recognized speech utterance into a ParseResult.

        Args:
            recognized_text: Raw text from the speech recognizer.
                             May be any case and may contain leading/trailing
                             whitespace.

        Returns:
            ParseResult with the matched intent and, where applicable,
            an InputCommand ready for the existing input validation pipeline.
        """
        normalized = _normalize(recognized_text)

        if not normalized:
            logger.warning("VoiceCommandParser: received empty text — UNKNOWN")
            return ParseResult(
                intent=VoiceIntent.UNKNOWN,
                input_command=None,
                raw_text=normalized,
            )

        if normalized in _FORWARD_PHRASES:
            logger.info(f"VoiceCommandParser: '{normalized}' → MOVE_FORWARD → FORWARD")
            return ParseResult(
                intent=VoiceIntent.MOVE_FORWARD,
                input_command=InputCommand(
                    source=InputSource.VOICE,
                    command="FORWARD",
                ),
                raw_text=normalized,
            )

        if normalized in _STOP_PHRASES:
            logger.info(f"VoiceCommandParser: '{normalized}' → STOP")
            return ParseResult(
                intent=VoiceIntent.STOP,
                input_command=InputCommand(
                    source=InputSource.VOICE,
                    command="STOP",
                ),
                raw_text=normalized,
            )

        if normalized in _START_PHRASES:
            # START is a recognized intent but has no motion command mapping.
            # See module docstring for the design rationale.
            logger.info(
                f"VoiceCommandParser: '{normalized}' → START "
                f"(system intent — no InputCommand produced)"
            )
            return ParseResult(
                intent=VoiceIntent.START,
                input_command=None,
                raw_text=normalized,
            )

        logger.warning(
            f"VoiceCommandParser: '{normalized}' → UNKNOWN "
            f"(not recognized as any valid command)"
        )
        return ParseResult(
            intent=VoiceIntent.UNKNOWN,
            input_command=None,
            raw_text=normalized,
        )
