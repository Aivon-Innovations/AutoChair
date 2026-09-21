"""
AudioState — lifecycle states of the AutoChair audio/voice subsystem.

State transitions (normal flow):

    IDLE
      ↓  (start_listening)
    LISTENING
      ↓  (audio captured)
    PROCESSING
      ↓  (speech recognized, command parsed)
    COMMAND_RECOGNIZED
      ↓  (TTS feedback begins)
    SPEAKING
      ↓  (feedback complete)
    IDLE

Error path (from any state):

    any state
      ↓  (recognition failure / hardware error / unknown command)
    ERROR
      ↓  (reset)
    IDLE

This state enum is used by AudioManager and is queryable by external
modules (e.g. a future UI module) to display voice status.
"""

from enum import Enum


class AudioState(str, Enum):
    """
    Lifecycle states of the AutoChair audio/voice subsystem.

    IDLE             — subsystem ready, not actively listening
    LISTENING        — microphone active, capturing audio
    PROCESSING       — audio captured, speech recognition in progress
    COMMAND_RECOGNIZED — valid AutoChair command extracted from speech
    SPEAKING         — TTS feedback is being delivered
    ERROR            — an error occurred (recognition failure, unknown command, etc.)
    """

    IDLE = "IDLE"
    LISTENING = "LISTENING"
    PROCESSING = "PROCESSING"
    COMMAND_RECOGNIZED = "COMMAND_RECOGNIZED"
    SPEAKING = "SPEAKING"
    ERROR = "ERROR"
