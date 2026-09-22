"""
AudioCapture — abstract interface and concrete implementations for
raw audio capture from a physical or simulated device.

Hierarchy
---------
AudioCapture (ABC)
  ├── SimulatedAudioCapture   — returns configurable bytes; no hardware
  └── ALSAAudioCapture        — reads from ALSA via pyalsaaudio (Pi only)

Hardware status
---------------
SimulatedAudioCapture : SIMULATED — no hardware access whatsoever
ALSAAudioCapture      : PI_PENDING — requires pyalsaaudio; not yet deployed/tested
                        in this repository. The physical MSM261S4030H0R microphone
                        wiring and ALSA capture were validated in Phase 2, but this
                        production class has not been exercised on Pi hardware.

Usage
-----
Development / tests — use SimulatedAudioCapture (default).
Raspberry Pi        — use ALSAAudioCapture(config) after verifying that
                      pyalsaaudio is installed and the ALSA device is present
                      (arecord -l shows MSM261S4030H0).

IMPORTANT: ALSAAudioCapture is guarded by a lazy import of pyalsaaudio.
Importing this module does NOT require pyalsaaudio to be installed.
Only instantiating ALSAAudioCapture triggers the import.
"""

from __future__ import annotations

from abc import ABC, abstractmethod

from autochair.audio.config import AudioConfig
from autochair.utils.logger import get_logger


logger = get_logger("autochair.audio.devices.capture")


# ---------------------------------------------------------------------------
# Abstract interface
# ---------------------------------------------------------------------------

class AudioCapture(ABC):
    """
    Abstract base class for audio capture.

    Provides a device-independent interface over:
      - open()       — initialise the capture device / session
      - read_chunk() — return one chunk of raw PCM bytes
      - close()      — release the device / session

    Implementations must be context-manager safe (open on __enter__,
    close on __exit__).

    The format of the bytes returned by read_chunk() is determined by
    the AudioConfig supplied at construction:
      - capture_sample_rate
      - capture_channels
      - capture_format_bits
    """

    @abstractmethod
    def open(self) -> None:
        """Initialise and open the audio capture device."""

    @abstractmethod
    def read_chunk(self) -> bytes:
        """
        Return one chunk of raw PCM audio bytes.

        Returns:
            bytes: A non-empty chunk on success.
                   An empty bytes object if no data is available (simulated
                   or overrun).
        """

    @abstractmethod
    def close(self) -> None:
        """Release the capture device and free resources."""

    @abstractmethod
    def is_open(self) -> bool:
        """Return True if the capture device is currently open."""

    def __enter__(self) -> "AudioCapture":
        self.open()
        return self

    def __exit__(self, *_: object) -> None:
        self.close()


# ---------------------------------------------------------------------------
# Simulated implementation — no hardware
# ---------------------------------------------------------------------------

class SimulatedAudioCapture(AudioCapture):
    """
    Software-only stub for audio capture.

    Used during development on Mac and for all unit tests.
    No ALSA, I2S, PortAudio, or audio hardware is accessed.

    Accepts a queue of pre-configured byte chunks at construction time.
    Each call to read_chunk() returns the next chunk in the sequence,
    cycling back to the start when exhausted.  This allows tests to
    inject known audio payloads and verify preprocessing/recognition
    behaviour deterministically.

    Hardware status: SIMULATED — not hardware-integrated or validated.
    """

    def __init__(
        self,
        chunks: list[bytes] | None = None,
        config: AudioConfig | None = None,
    ) -> None:
        """
        Args:
            chunks: Ordered list of byte payloads that read_chunk() will
                    return in sequence (cycling).  Defaults to [b""] (one
                    empty chunk — simulates silence).
            config: AudioConfig; stored for reference but not used by the
                    simulation itself.
        """
        self._chunks: list[bytes] = chunks if chunks is not None else [b""]
        self._config: AudioConfig = config or AudioConfig()
        self._index: int = 0
        self._open: bool = False
        logger.info(
            f"SimulatedAudioCapture initialised "
            f"(chunks={len(self._chunks)}, no hardware access)"
        )

    # ------------------------------------------------------------------ #

    def open(self) -> None:
        """Mark the simulated device as open."""
        self._open = True
        self._index = 0
        logger.info("[SIMULATED] AudioCapture: open()")

    def read_chunk(self) -> bytes:
        """
        Return the next pre-configured chunk, cycling through the list.

        Returns:
            bytes: The next configured chunk (may be empty).
        """
        if not self._chunks:
            return b""
        chunk = self._chunks[self._index % len(self._chunks)]
        self._index += 1
        logger.debug(f"[SIMULATED] AudioCapture: read_chunk() → {len(chunk)} bytes")
        return chunk

    def close(self) -> None:
        """Mark the simulated device as closed."""
        self._open = False
        logger.info("[SIMULATED] AudioCapture: close()")

    def is_open(self) -> bool:
        """Return whether the simulated device is currently open."""
        return self._open

    def set_chunks(self, chunks: list[bytes]) -> None:
        """Replace the chunk sequence (useful between test cases)."""
        self._chunks = chunks
        self._index = 0

    @property
    def read_count(self) -> int:
        """Number of read_chunk() calls made since last open()."""
        return self._index


# ---------------------------------------------------------------------------
# ALSA implementation — Raspberry Pi only
# ---------------------------------------------------------------------------

class ALSAAudioCapture(AudioCapture):
    """
    Real audio capture from the ALSA subsystem using pyalsaaudio.

    Targets the DFRobot MSM261S4030H0R I2S MEMS microphone as configured
    in AudioConfig.  The ALSA device identifier is taken from
    config.alsa_device (default: "hw:CARD=MSM261S4030H0,DEV=0").

    Hardware status: PI_PENDING — class is correct and ready; not yet deployed/tested on Pi
    Requires:
      - pyalsaaudio >= 0.11.0  (install: pip install pyalsaaudio)
      - Physical MSM261S4030H0R microphone wired and overlay loaded
      - ALSA device visible in `arecord -l`

    This class is NOT usable on macOS.  Import of pyalsaaudio is deferred
    to __init__() so that importing this module on Mac does not fail.

    Verified Pi parameters (Phase 2):
      device  = "hw:CARD=MSM261S4030H0,DEV=0"
      rate    = 48000
      channels = 2
      format  = S32_LE
      period  = 4096 frames

    IMPORTANT: Do NOT instantiate this class in tests or on Mac.
    Use SimulatedAudioCapture instead.
    """

    def __init__(self, config: AudioConfig) -> None:
        """
        Args:
            config: AudioConfig containing all capture parameters.

        Raises:
            ImportError: If pyalsaaudio is not installed.
        """
        try:
            import alsaaudio as _aa  # type: ignore[import-not-found]
            self._aa = _aa
        except ImportError as exc:
            raise ImportError(
                "pyalsaaudio is required for ALSAAudioCapture. "
                "Install on Raspberry Pi with: pip install pyalsaaudio\n"
                f"Original error: {exc}"
            ) from exc

        self._config = config
        self._pcm: object | None = None
        self._open_flag: bool = False
        logger.info(
            f"ALSAAudioCapture created "
            f"(device='{config.alsa_device}', "
            f"rate={config.capture_sample_rate} Hz, "
            f"channels={config.capture_channels}, "
            f"bits={config.capture_format_bits})"
        )

    def open(self) -> None:
        """
        Open the ALSA PCM capture device.

        Raises:
            RuntimeError: If the ALSA device cannot be opened (e.g. the
                          microphone is not connected or the overlay is not
                          loaded).
        """
        _format_map: dict[int, int] = {
            16: self._aa.PCM_FORMAT_S16_LE,
            32: self._aa.PCM_FORMAT_S32_LE,
        }
        alsa_format = _format_map.get(self._config.capture_format_bits)
        if alsa_format is None:
            raise ValueError(
                f"Unsupported capture_format_bits: "
                f"{self._config.capture_format_bits}"
            )

        try:
            self._pcm = self._aa.PCM(
                type=self._aa.PCM_CAPTURE,
                mode=self._aa.PCM_NORMAL,
                device=self._config.alsa_device,
                channels=self._config.capture_channels,
                rate=self._config.capture_sample_rate,
                format=alsa_format,
                periodsize=self._config.capture_chunk_frames,
            )
        except self._aa.ALSAAudioError as exc:
            raise RuntimeError(
                f"Failed to open ALSA device '{self._config.alsa_device}': {exc}\n"
                "Verify the microphone is connected and the overlay is loaded "
                "('arecord -l' should show MSM261S4030H0)."
            ) from exc

        self._open_flag = True
        logger.info(
            f"ALSA capture opened: '{self._config.alsa_device}' "
            f"@ {self._config.capture_sample_rate} Hz"
        )

    def read_chunk(self) -> bytes:
        """
        Read one period of audio from the ALSA device.

        Returns:
            bytes: Raw PCM audio data (S32_LE stereo at capture_sample_rate).
                   Empty bytes if the read returned no frames (overrun).

        Raises:
            RuntimeError: If called before open().
        """
        if self._pcm is None:
            raise RuntimeError(
                "ALSAAudioCapture.read_chunk() called before open(). "
                "Call open() first."
            )
        length, data = self._pcm.read()  # type: ignore[union-attr]
        if length <= 0:
            logger.warning(f"ALSA read returned length={length} (overrun or underrun)")
            return b""
        return data

    def close(self) -> None:
        """Close the ALSA PCM device and free the hardware resource."""
        if self._pcm is not None:
            self._pcm.close()  # type: ignore[union-attr]
            self._pcm = None
        self._open_flag = False
        logger.info("ALSA capture closed")

    def is_open(self) -> bool:
        """Return True if the ALSA PCM device is currently open."""
        return self._open_flag
