"""
RealVoiceSession — single-session runtime bridging audio capture to speech recognition.

Orchestrates the Phase 4A verified real audio streaming chain:
  AudioCapture (ALSA / Simulated)
      │  (raw S32_LE stereo @ 48 kHz)
      ▼
  AudioPreprocessor
      │  (converted mono S16_LE @ 16 kHz)
      ▼
  VoskSpeechRecognizer
      │  (streaming KaldiRecognizer.recognize())
      ▼
  Final utterance string (or "" on silence / [unk] / timeout)

Hardware status: PI_PENDING — not yet deployed/tested in this repository.
The underlying components (DFRobot MSM261S4030H0 I2S microphone, ALSA capture,
AudioPreprocessor, and Vosk small English model) were experimentally validated
on Raspberry Pi 4 in Phase 2 and Phase 3, but this session runtime coordinator
has not yet been deployed on Pi hardware.

Session Lifecycle:
  1. Open capture device (capture.open())
  2. In a loop until finalized utterance or session duration expires:
     - read raw audio chunk from capture
     - preprocess raw chunk to mono 16 kHz S16_LE PCM
     - feed chunk to recognizer.recognize()
     - if a finalized utterance is returned, return it immediately
  3. If duration expires without a finalized utterance:
     - call recognizer.finalize() to retrieve any remaining utterance
     - return the finalized text or ""
  4. Always close capture device in finally block (capture.close())
"""

from __future__ import annotations

import time
from typing import Callable

from autochair.audio.config import AudioConfig
from autochair.audio.devices.capture import AudioCapture
from autochair.audio.devices.preprocessor import AudioPreprocessor
from autochair.audio.speech.recognizer import SpeechRecognizer
from autochair.audio.speech.vosk_recognizer import VoskSpeechRecognizer
from autochair.utils.logger import get_logger


logger = get_logger("autochair.audio.voice_session")


class RealVoiceSession:
    """
    Coordinates a single voice capture and speech recognition session.

    Dependencies are injected at construction time to ensure complete
    testability without physical hardware, ALSA drivers, or live Vosk models.

    Phase 4A uses a fixed maximum duration policy (default: 5.0 s).
    VAD / dynamic silence detection is deferred to a future phase.
    """

    def __init__(
        self,
        capture: AudioCapture,
        preprocessor: AudioPreprocessor,
        recognizer: VoskSpeechRecognizer | SpeechRecognizer,
        config: AudioConfig | None = None,
        duration: float | None = None,
        time_fn: Callable[[], float] = time.monotonic,
    ) -> None:
        """
        Initialize RealVoiceSession.

        Args:
            capture: AudioCapture instance (ALSAAudioCapture on Pi,
                     SimulatedAudioCapture in tests).
            preprocessor: AudioPreprocessor instance for format conversion.
            recognizer: SpeechRecognizer instance (VoskSpeechRecognizer on Pi,
                        mock/stub in tests).
            config: AudioConfig instance. If duration is not explicitly passed,
                    config.voice_session_duration_seconds is used.
            duration: Optional session duration override in seconds (must be > 0).
            time_fn: Monotonic time function for tracking session duration.
                     Injectable for deterministic, non-sleeping unit tests.

        Raises:
            ValueError: If duration or config.voice_session_duration_seconds <= 0.
        """
        self._capture = capture
        self._preprocessor = preprocessor
        self._recognizer = recognizer
        self._config = config or AudioConfig()
        self._time_fn = time_fn

        if duration is not None:
            if duration <= 0:
                raise ValueError(f"duration must be > 0, got {duration}")
            self._duration = float(duration)
        else:
            self._duration = float(self._config.voice_session_duration_seconds)

        if self._duration <= 0:
            raise ValueError(f"Session duration must be > 0, got {self._duration}")

        logger.info(
            f"RealVoiceSession initialized (duration={self._duration:.1f}s, "
            f"capture={capture.__class__.__name__}, "
            f"recognizer={recognizer.__class__.__name__})"
        )

    @property
    def duration(self) -> float:
        """Configured maximum duration of the session in seconds."""
        return self._duration

    def start(self) -> str:
        """
        Execute one voice session and return the recognized text.

        Lifecycle:
          1. capture.open()
          2. Streaming chunk loop until recognized utterance or timeout
          3. recognizer.finalize() on timeout
          4. capture.close() in finally

        Returns:
            str: The recognized utterance (lowercased, stripped), or ""
                 if no command was recognized or timeout occurred with no speech.
        """
        logger.info(f"Starting voice session (max duration: {self._duration:.1f}s)")
        try:
            self._capture.open()
            start_time = self._time_fn()

            while (self._time_fn() - start_time) < self._duration:
                raw_chunk = self._capture.read_chunk()
                if not raw_chunk:
                    continue

                processed = self._preprocessor.process(raw_chunk)
                if not processed:
                    continue

                text = self._recognizer.recognize(processed)
                if text:
                    logger.info(f"Voice session recognized utterance: '{text}'")
                    return text

            # Session duration elapsed without a finalized utterance from recognize()
            logger.info("Voice session duration elapsed, finalizing recognizer...")
            final_text = ""
            if hasattr(self._recognizer, "finalize") and callable(self._recognizer.finalize):
                final_text = self._recognizer.finalize()

            if final_text:
                logger.info(f"Voice session finalize returned: '{final_text}'")
            else:
                logger.info("Voice session ended with no recognized speech.")

            return final_text
        finally:
            self._capture.close()
            logger.info("Voice session capture device closed.")

    def run(self) -> str:
        """Alias for start()."""
        return self.start()
