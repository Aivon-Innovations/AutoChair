"""
SpeechRecognizer — abstract interface and simulated implementation.

Hardware status: SIMULATED
The real implementation will use an offline speech recognition engine
(Vosk or Whisper) on the Raspberry Pi 4. Engine selection has not yet
been finalized; both require separate evaluation for Pi CPU/RAM constraints.

DO NOT install Vosk, Whisper, or any speech recognition library in this phase.
DO NOT connect to any microphone hardware in this module.
"""

from abc import ABC, abstractmethod

from autochair.utils.logger import get_logger


logger = get_logger("autochair.audio.speech.recognizer")


class SpeechRecognizer(ABC):
    """
    Abstract interface for speech recognition.

    Receives raw audio data (bytes) and returns recognized text (str).
    Returns an empty string when recognition fails or produces no result.

    The concrete implementation (Vosk/Whisper on Pi) replaces
    SimulatedSpeechRecognizer without changing any downstream code.
    """

    @abstractmethod
    def recognize(self, audio_data: bytes) -> str:
        """
        Convert raw audio bytes to recognized text.

        Returns:
            str: The recognized utterance, lowercased and stripped.
                 Empty string if recognition failed or produced no output.
        """


class SimulatedSpeechRecognizer(SpeechRecognizer):
    """
    Software-only stub for speech recognition.

    Accepts a pre-configured response text at construction time so that
    tests can inject specific utterances without any audio hardware or
    speech recognition library.

    Usage in tests:
        recognizer = SimulatedSpeechRecognizer(response="move forward")
        result = recognizer.recognize(b"")
        # result == "move forward"

    Hardware status: SIMULATED — not hardware-integrated or validated.
    No Vosk/Whisper model is loaded. No audio data is processed.
    """

    def __init__(self, response: str = "") -> None:
        """
        Args:
            response: The text that recognize() will always return.
                      Defaults to empty string (recognition failure).
        """
        self._response = response.strip().lower()
        logger.info(
            f"SimulatedSpeechRecognizer initialised "
            f"(response='{self._response}', no hardware access)"
        )

    def set_response(self, response: str) -> None:
        """Update the simulated recognition response between calls."""
        self._response = response.strip().lower()

    def recognize(self, audio_data: bytes) -> str:
        """
        Return the pre-configured response text regardless of audio_data content.

        In the real implementation this will pass audio_data through
        the speech recognition engine and return the transcription.
        """
        logger.info(f"[SIMULATED] SpeechRecognizer: recognize → '{self._response}'")
        return self._response
