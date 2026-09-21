"""
TTSEngine — abstract interface and simulated implementation.

Hardware status: SIMULATED
The real implementation will use a TTS library (e.g. pyttsx3 or espeak)
on the Raspberry Pi 4, routing audio through the Raspberry Pi audio output
(3.5mm / HDMI / I2S) to the PAM8403 amplifier and speakers.

DO NOT install any TTS library or attempt any audio output in this phase.
DO NOT access GPIO, ALSA, PAM8403, or speaker hardware.
"""

from abc import ABC, abstractmethod

from autochair.utils.logger import get_logger


logger = get_logger("autochair.audio.tts.engine")


class TTSEngine(ABC):
    """
    Abstract interface for text-to-speech output.

    Receives a text message and speaks it via whatever output mechanism
    the concrete implementation uses (real speakers, log, etc.).
    """

    @abstractmethod
    def speak(self, text: str) -> None:
        """
        Convert text to speech and deliver it to the audio output.

        Args:
            text: The message to speak. Must not be empty.

        Raises:
            ValueError: If text is empty or whitespace-only.
        """

    @abstractmethod
    def last_spoken(self) -> str:
        """
        Return the most recent text passed to speak().

        Returns empty string if speak() has never been called.
        Useful for verifying TTS output in tests without audio hardware.
        """


class SimulatedTTSEngine(TTSEngine):
    """
    Software-only stub for the TTS engine.

    Records spoken text in memory and logs it. No audio is produced.
    Suitable for all unit tests and Mac development.

    Usage in tests:
        tts = SimulatedTTSEngine()
        tts.speak("System ready.")
        assert tts.last_spoken() == "System ready."

    Hardware status: SIMULATED — not hardware-integrated or validated.
    No PAM8403, ALSA, speaker, or GPIO access.
    """

    def __init__(self) -> None:
        self._last_spoken: str = ""
        self._history: list[str] = []
        logger.info("SimulatedTTSEngine initialised (no hardware access)")

    def speak(self, text: str) -> None:
        """
        Log and record the text instead of producing audio output.

        Raises:
            ValueError: If text is empty or whitespace-only.
        """
        if not text or not text.strip():
            raise ValueError("TTS text cannot be empty.")
        self._last_spoken = text.strip()
        self._history.append(self._last_spoken)
        logger.info(f"[SIMULATED] TTS: speak → '{self._last_spoken}'")

    def last_spoken(self) -> str:
        """Return the most recent text passed to speak()."""
        return self._last_spoken

    def spoken_history(self) -> list[str]:
        """
        Return all utterances spoken since initialisation.

        Note: The base interface only requires last_spoken(). This method
        is an extension available on SimulatedTTSEngine only, for richer
        test assertions.
        """
        return list(self._history)
