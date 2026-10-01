"""
AudioConfig — central configuration for the AutoChair audio/voice subsystem.

All hardware-specific values (ALSA device, sample rates, model paths,
grammar) are gathered here so they are not scattered across modules.

Hardware status: SIMULATED (default)
When DEVELOPMENT_MODE is True, AudioConfig defaults target the simulated
pipeline (empty model path, SimulatedAudioCapture).  On the Raspberry Pi
the caller supplies an AudioConfig with real hardware values.

Verified Pi hardware parameters (MSM261S4030H0R, Phase 2):
  capture_sample_rate = 48000   (verified ALSA parameter)
  capture_channels    = 2       (stereo I2S frame; left = microphone, right = zero)
  capture_format_bits = 32      (S32_LE — Pi I2S pads 24-bit data to 32-bit words)
  mic_channel         = 0       (left channel, LR pin tied to GND)
  alsa_device         = "hw:CARD=MSM261S4030H0,DEV=0"  (stable ALSA name)
  vosk_sample_rate    = 16000   (Vosk small-en-us model; verified Pi experiment)
"""

from __future__ import annotations

from dataclasses import dataclass, field


# ---------------------------------------------------------------------------
# Default grammar — restricted command vocabulary (English + Hindi/Hinglish).
# Extend this list in future phases; VoskSpeechRecognizer will pick up
# changes automatically from the AudioConfig it receives.
# ---------------------------------------------------------------------------
_DEFAULT_GRAMMAR: list[str] = [
    # START
    "start",
    "chalo",
    # STOP
    "stop",
    "ruko",
    "rukko",
    # FORWARD
    "forward",
    "move forward",
    "aage chalo",
    "aage jao",
    # REVERSE
    "backward",
    "move backward",
    "reverse",
    "piche chalo",
    "peeche chalo",
    "piche jao",
    "peeche jao",
    # LEFT
    "left",
    "turn left",
    "baaye chalo",
    "baaya chalo",
    # RIGHT
    "right",
    "turn right",
    "daaye chalo",
    "daaya chalo",
    # UNKNOWN sentinel
    "[unk]",
]


@dataclass
class AudioConfig:
    """
    Central configuration object for the AutoChair audio subsystem.

    Passed to AudioCapture, AudioPreprocessor, and VoskSpeechRecognizer
    so that no hardware constant is hard-coded inside the implementation
    classes.  Changing a value here propagates to all consumers.

    Default values target the simulated development environment (Mac).
    Supply concrete values for the Raspberry Pi production environment.

    Fields
    ------
    alsa_device : str
        Stable ALSA device identifier.  Prefer the CARD= form over a
        numeric card index (e.g. "hw:3,0") because ALSA card numbers
        change when other audio hardware is added or removed.
        Verified value: "hw:CARD=MSM261S4030H0,DEV=0"

    capture_sample_rate : int
        Hardware capture sample rate in Hz.  MSM261S4030H0R verified at
        48000 Hz.

    capture_channels : int
        Number of interleaved channels returned by the ALSA device.
        MSM261S4030H0R stereo I2S frame → 2 channels.

    capture_format_bits : int
        Bit depth of raw ALSA samples.  Pi I2S pads 24-bit data to
        32-bit words → 32.

    capture_chunk_frames : int
        Number of audio frames per ALSA read() call.  Tune for latency
        vs. overhead.  Default 4096 frames at 48 kHz ≈ 85 ms per chunk.

    mic_channel : int
        Index of the channel that carries the real microphone signal.
        0 = left (LR pin LOW, MSM261S4030H0R Phase 2 wiring).

    vosk_model_path : str
        Filesystem path to the Vosk model directory on the target device.
        Empty string → model not configured (VoskSpeechRecognizer will
        raise RuntimeError if instantiated without a valid path).
        Verified Pi model: "vosk-model-small-en-us-0.15"

    vosk_sample_rate : int
        Sample rate expected by the Vosk model.
        vosk-model-small-en-us-0.15 → 16000 Hz.

    vosk_grammar : list[str]
        Restricted grammar for KaldiRecognizer.  Only phrases listed here
        (plus "[unk]") will ever be returned by Vosk.
        "[unk]" must always be included so unknown speech is labelled
        rather than silently assigned to the nearest grammar match.

    vosk_suppress_logs : bool
        If True, suppress Vosk's verbose C++ log output.  Recommended
        True for all normal operation.

    resample_input_rate : int
        Input sample rate for the AudioPreprocessor resampler.
        Should match capture_sample_rate.

    resample_output_rate : int
        Target sample rate for the AudioPreprocessor resampler.
        Should match vosk_sample_rate.

    voice_session_duration_seconds : float
        Maximum duration in seconds for a single voice capture session.
        Phase 4A fixed-duration session policy (default: 5.0 s).
    """

    # ------------------------------------------------------------------ #
    # ALSA capture                                                         #
    # ------------------------------------------------------------------ #
    alsa_device: str = "hw:CARD=MSM261S4030H0,DEV=0"
    capture_sample_rate: int = 48000
    capture_channels: int = 2
    capture_format_bits: int = 32
    capture_chunk_frames: int = 4096
    mic_channel: int = 0

    # ------------------------------------------------------------------ #
    # Vosk                                                                 #
    # ------------------------------------------------------------------ #
    vosk_model_path: str = ""
    vosk_sample_rate: int = 16000
    vosk_grammar: list[str] = field(
        default_factory=lambda: list(_DEFAULT_GRAMMAR)
    )
    vosk_suppress_logs: bool = True

    # ------------------------------------------------------------------ #
    # Resampling                                                           #
    # ------------------------------------------------------------------ #
    resample_input_rate: int = 48000
    resample_output_rate: int = 16000

    # ------------------------------------------------------------------ #
    # Voice Session                                                        #
    # ------------------------------------------------------------------ #
    voice_session_duration_seconds: float = 5.0

    # ------------------------------------------------------------------ #
    # Derived helpers                                                       #
    # ------------------------------------------------------------------ #

    @property
    def bytes_per_capture_sample(self) -> int:
        """Bytes per raw ALSA sample (all channels, one frame)."""
        return (self.capture_format_bits // 8) * self.capture_channels

    @property
    def resample_ratio(self) -> float:
        """Ratio input_rate / output_rate (e.g. 3.0 for 48 kHz → 16 kHz)."""
        return self.resample_input_rate / self.resample_output_rate

    def __post_init__(self) -> None:
        if self.capture_channels < 1:
            raise ValueError(
                f"capture_channels must be >= 1, got {self.capture_channels}"
            )
        if not 0 <= self.mic_channel < self.capture_channels:
            raise ValueError(
                f"mic_channel {self.mic_channel} out of range for "
                f"{self.capture_channels} capture channels"
            )
        if self.capture_format_bits not in (16, 24, 32):
            raise ValueError(
                f"capture_format_bits must be 16, 24, or 32, "
                f"got {self.capture_format_bits}"
            )
        if self.resample_input_rate <= 0 or self.resample_output_rate <= 0:
            raise ValueError("resample rates must be positive integers")
        if self.voice_session_duration_seconds <= 0:
            raise ValueError(
                f"voice_session_duration_seconds must be > 0, got {self.voice_session_duration_seconds}"
            )
        if "[unk]" not in self.vosk_grammar:
            self.vosk_grammar = list(self.vosk_grammar) + ["[unk]"]
