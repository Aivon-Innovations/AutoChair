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

    MOVE_FORWARD — user wants the chair to move forward
                  maps to InputCommand(command="FORWARD")
    REVERSE      — user wants the chair to move backward / reverse
                  maps to InputCommand(command="REVERSE")
    LEFT         — user wants the chair to turn left
                  maps to InputCommand(command="LEFT")
    RIGHT        — user wants the chair to turn right
                  maps to InputCommand(command="RIGHT")
    STOP         — user wants the chair to stop
                  maps to InputCommand(command="STOP")
    START        — user wants to activate/start the system
                  system intent with no motion command.
    UNKNOWN      — utterance was not recognized as any valid intent
                  produces no InputCommand. Triggers a COMMAND_REJECTED alert.
    """

    MOVE_FORWARD = "MOVE_FORWARD"
    REVERSE = "REVERSE"
    LEFT = "LEFT"
    RIGHT = "RIGHT"
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
# Maintainable command-alias mapping structure (English + Hindi / Hinglish)
# Maps normalized phrase → (VoiceIntent, normalized_command_string or None)
# ---------------------------------------------------------------------------

COMMAND_ALIASES: dict[str, tuple[VoiceIntent, str | None]] = {
    # ------------------------------------------------------------------ #
    # START (system intent — no motion InputCommand)                     #
    # ------------------------------------------------------------------ #
    "start": (VoiceIntent.START, None),
    "chalo": (VoiceIntent.START, None),
    "चलो": (VoiceIntent.START, None),
    "begin": (VoiceIntent.START, None),
    "activate": (VoiceIntent.START, None),
    "start the chair": (VoiceIntent.START, None),
    "please start": (VoiceIntent.START, None),

    # ------------------------------------------------------------------ #
    # STOP (maps to InputCommand(command="STOP"))                         #
    # ------------------------------------------------------------------ #
    "stop": (VoiceIntent.STOP, "STOP"),
    "ruko": (VoiceIntent.STOP, "STOP"),
    "rukko": (VoiceIntent.STOP, "STOP"),
    "रुको": (VoiceIntent.STOP, "STOP"),
    "halt": (VoiceIntent.STOP, "STOP"),
    "brake": (VoiceIntent.STOP, "STOP"),
    "please stop": (VoiceIntent.STOP, "STOP"),
    "stop the chair": (VoiceIntent.STOP, "STOP"),
    "please stop the chair": (VoiceIntent.STOP, "STOP"),

    # ------------------------------------------------------------------ #
    # FORWARD (maps to InputCommand(command="FORWARD"))                   #
    # ------------------------------------------------------------------ #
    "forward": (VoiceIntent.MOVE_FORWARD, "FORWARD"),
    "move forward": (VoiceIntent.MOVE_FORWARD, "FORWARD"),
    "aage chalo": (VoiceIntent.MOVE_FORWARD, "FORWARD"),
    "aage jao": (VoiceIntent.MOVE_FORWARD, "FORWARD"),
    "आगे चल": (VoiceIntent.MOVE_FORWARD, "FORWARD"),
    "आगे चलो": (VoiceIntent.MOVE_FORWARD, "FORWARD"),
    "आगे जाओ": (VoiceIntent.MOVE_FORWARD, "FORWARD"),
    "go forward": (VoiceIntent.MOVE_FORWARD, "FORWARD"),
    "go ahead": (VoiceIntent.MOVE_FORWARD, "FORWARD"),
    "move ahead": (VoiceIntent.MOVE_FORWARD, "FORWARD"),
    "drive forward": (VoiceIntent.MOVE_FORWARD, "FORWARD"),

    # ------------------------------------------------------------------ #
    # REVERSE (maps to InputCommand(command="REVERSE"))                   #
    # ------------------------------------------------------------------ #
    "backward": (VoiceIntent.REVERSE, "REVERSE"),
    "move backward": (VoiceIntent.REVERSE, "REVERSE"),
    "reverse": (VoiceIntent.REVERSE, "REVERSE"),
    "piche chalo": (VoiceIntent.REVERSE, "REVERSE"),
    "peeche chalo": (VoiceIntent.REVERSE, "REVERSE"),
    "piche jao": (VoiceIntent.REVERSE, "REVERSE"),
    "peeche jao": (VoiceIntent.REVERSE, "REVERSE"),
    "पीछे चलो": (VoiceIntent.REVERSE, "REVERSE"),
    "पीछे जाओ": (VoiceIntent.REVERSE, "REVERSE"),
    "go backward": (VoiceIntent.REVERSE, "REVERSE"),
    "move reverse": (VoiceIntent.REVERSE, "REVERSE"),

    # ------------------------------------------------------------------ #
    # LEFT (maps to InputCommand(command="LEFT"))                         #
    # ------------------------------------------------------------------ #
    "left": (VoiceIntent.LEFT, "LEFT"),
    "turn left": (VoiceIntent.LEFT, "LEFT"),
    "go left": (VoiceIntent.LEFT, "LEFT"),
    "move left": (VoiceIntent.LEFT, "LEFT"),
    "left chlo": (VoiceIntent.LEFT, "LEFT"),
    "left chalo": (VoiceIntent.LEFT, "LEFT"),
    "baaye chalo": (VoiceIntent.LEFT, "LEFT"),
    "baaya chalo": (VoiceIntent.LEFT, "LEFT"),
    "बायें चलो": (VoiceIntent.LEFT, "LEFT"),
    "बायाँ चलो": (VoiceIntent.LEFT, "LEFT"),
    "लेफ्ट चलो": (VoiceIntent.LEFT, "LEFT"),

    # ------------------------------------------------------------------ #
    # RIGHT (maps to InputCommand(command="RIGHT"))                       #
    # ------------------------------------------------------------------ #
    "right": (VoiceIntent.RIGHT, "RIGHT"),
    "turn right": (VoiceIntent.RIGHT, "RIGHT"),
    "go right": (VoiceIntent.RIGHT, "RIGHT"),
    "move right": (VoiceIntent.RIGHT, "RIGHT"),
    "right chlo": (VoiceIntent.RIGHT, "RIGHT"),
    "right chalo": (VoiceIntent.RIGHT, "RIGHT"),
    "daaye chalo": (VoiceIntent.RIGHT, "RIGHT"),
    "daaya chalo": (VoiceIntent.RIGHT, "RIGHT"),
    "दायें चलो": (VoiceIntent.RIGHT, "RIGHT"),
    "दायाँ चलो": (VoiceIntent.RIGHT, "RIGHT"),
    "दाएं चलो": (VoiceIntent.RIGHT, "RIGHT"),
    "राइट चलो": (VoiceIntent.RIGHT, "RIGHT"),
}


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

        if not normalized or normalized == "[unk]":
            logger.warning("VoiceCommandParser: received empty or unk text — UNKNOWN")
            return ParseResult(
                intent=VoiceIntent.UNKNOWN,
                input_command=None,
                raw_text=normalized,
            )

        match = COMMAND_ALIASES.get(normalized)
        if match is not None:
            intent, cmd_str = match
            input_cmd = None
            if cmd_str is not None:
                input_cmd = InputCommand(
                    source=InputSource.VOICE,
                    command=cmd_str,
                )
            logger.info(f"VoiceCommandParser: '{normalized}' → {intent.value} (command={cmd_str})")
            return ParseResult(
                intent=intent,
                input_command=input_cmd,
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
