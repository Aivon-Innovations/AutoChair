"""
MicrophoneManager — abstract interface and simulated implementation.

Hardware status: SIMULATED
The real implementation (DFRobot I2S microphone via ALSA on Raspberry Pi 4)
will replace SimulatedMicrophoneManager in a future hardware integration phase.
That phase requires:
  - Raspberry Pi 4 with I2S kernel overlay configured in /boot/config.txt
  - DFRobot I2S MEMS microphone physically connected
  - ALSA device (e.g. hw:1,0) verified via `arecord -l`
  - PyAudio or sounddevice installed in the Pi environment

DO NOT attempt real hardware access from this module.
"""

from abc import ABC, abstractmethod

from autochair.utils.logger import get_logger


logger = get_logger("autochair.audio.devices.microphone")


class MicrophoneManager(ABC):
    """
    Abstract interface for microphone management.

    Implementations must be able to start/stop listening and provide
    captured audio data. The concrete data format is implementation-defined
    (bytes for real hardware; str/stub for simulation).

    The rest of the audio subsystem interacts only with this interface,
    so the real I2S implementation can replace SimulatedMicrophoneManager
    without changing the command pipeline.
    """

    @abstractmethod
    def start_listening(self) -> None:
        """Initialise microphone and begin audio capture."""

    @abstractmethod
    def stop_listening(self) -> None:
        """Stop audio capture and release hardware resources."""

    @abstractmethod
    def is_listening(self) -> bool:
        """Return True if the microphone is currently capturing audio."""

    @abstractmethod
    def capture_audio(self) -> bytes:
        """
        Return a chunk of captured audio as raw bytes (PCM).

        For the simulated implementation this returns an empty bytes object.
        For the real I2S implementation this will read from the ALSA capture
        device at the configured sample rate and bit depth.
        """


class SimulatedMicrophoneManager(MicrophoneManager):
    """
    Software-only stub for the microphone manager.

    Used during development on Mac and for all unit tests.
    Does not access any audio hardware, ALSA, I2S, or GPIO.

    Hardware status: SIMULATED — not hardware-integrated or validated.
    """

    def __init__(self) -> None:
        self._listening: bool = False
        logger.info("SimulatedMicrophoneManager initialised (no hardware access)")

    def start_listening(self) -> None:
        """Simulate starting the microphone (no hardware action)."""
        self._listening = True
        logger.info("[SIMULATED] Microphone: start_listening")

    def stop_listening(self) -> None:
        """Simulate stopping the microphone (no hardware action)."""
        self._listening = False
        logger.info("[SIMULATED] Microphone: stop_listening")

    def is_listening(self) -> bool:
        """Return the simulated listening state."""
        return self._listening

    def capture_audio(self) -> bytes:
        """
        Return empty bytes — no real audio is captured.

        In the real implementation this will block and return PCM audio
        data from the DFRobot I2S microphone via the ALSA capture device.
        """
        logger.info("[SIMULATED] Microphone: capture_audio → returning empty bytes")
        return b""
