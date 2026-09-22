"""
AudioPreprocessor — converts raw MSM261S4030H0R I2S capture to Vosk-ready PCM.

The physical microphone produces S32_LE stereo at 48 kHz.
Vosk (vosk-model-small-en-us-0.15) requires mono 16-bit PCM at 16 kHz.

This module implements the conversion as a clean, testable pipeline:

  raw bytes (S32_LE stereo, 48 kHz)
      │
      ▼ extract_channel()
  left-channel S32 samples (48 kHz)
      │
      ▼ convert_s32_to_s16()
  left-channel S16 samples (48 kHz)
      │
      ▼ resample()
  mono S16 samples (16 kHz)
      │
      ▼
  bytes → Vosk KaldiRecognizer

Each step is exposed as a standalone function so it can be tested
independently without constructing the full pipeline.

The combined convenience method process() runs all three steps.

Resampling
----------
The MSM261S4030H0R experiment used simple decimation (every 3rd sample)
for rapid diagnosis.  That approach works acceptably for speech because
human speech energy is concentrated well below the Nyquist of 16 kHz
(8 kHz), and the ratio 48000→16000 is an exact integer 3:1.

This implementation uses a short Hamming-windowed sinc (FIR) low-pass
filter before decimation to provide proper anti-aliasing.  The filter is
pre-computed at module load time from stdlib math — no NumPy required.

The filter preserves the following:
  - Audio frequencies below 6.4 kHz (pass-band) with minimal distortion
  - Frequencies above 8 kHz (stop-band) are attenuated before decimation
  - Speech intelligibility is fully preserved (voice: 300 Hz – 3.4 kHz)

To replace the resampler with a different implementation (e.g. NumPy
polyphase, SoxR, or libresample), subclass AudioPreprocessor and
override the resample() method.

Hardware status: NO HARDWARE ACCESS
All operations are pure Python arithmetic on in-memory bytes/arrays.
Fully testable without the physical microphone.

Dependencies: stdlib only (struct, array, math, itertools)
"""

from __future__ import annotations

import array
import math
import struct

from autochair.audio.config import AudioConfig
from autochair.utils.logger import get_logger


logger = get_logger("autochair.audio.devices.preprocessor")


# ---------------------------------------------------------------------------
# Pre-computed anti-aliasing FIR filter for 3:1 decimation (48→16 kHz)
# ---------------------------------------------------------------------------

def _design_hamming_sinc_fir(taps: int, cutoff_normalized: float) -> list[float]:
    """
    Design a Hamming-windowed sinc low-pass FIR filter.

    Args:
        taps:              Number of filter taps (odd preferred for symmetry).
        cutoff_normalized: Normalized cut-off frequency in [0, 0.5].
                           0.5 = Nyquist of the input signal.

    Returns:
        list[float]: FIR coefficients, length == taps.

    The Hamming window provides ~41 dB stop-band attenuation, which is
    adequate for 16-bit speech audio.
    """
    N = taps - 1
    h: list[float] = []
    for i in range(taps):
        n = i - N / 2
        if n == 0.0:
            sinc_val = 2.0 * cutoff_normalized
        else:
            sinc_val = math.sin(2.0 * math.pi * cutoff_normalized * n) / (math.pi * n)
        window = 0.54 - 0.46 * math.cos(2.0 * math.pi * i / N)
        h.append(sinc_val * window)
    return h


# 15-tap Hamming-windowed sinc for 3:1 decimation (48 kHz → 16 kHz).
# Cutoff = 0.5 / 3 ≈ 0.1667 (normalised to input Nyquist = 0.5).
# Pre-computed once at module import — no runtime overhead per call.
_FIR_48K_TO_16K: list[float] = _design_hamming_sinc_fir(taps=15, cutoff_normalized=1.0 / 6.0)
_FIR_DECIMATION: int = 3  # 48000 / 16000 = 3


# ---------------------------------------------------------------------------
# Public functions (stateless, testable independently)
# ---------------------------------------------------------------------------

def extract_channel(raw_bytes: bytes, channel: int, num_channels: int) -> list[int]:
    """
    Extract one channel from interleaved S32_LE PCM bytes.

    Args:
        raw_bytes:    Raw ALSA capture bytes (S32_LE, interleaved channels).
        channel:      Zero-based channel index to extract (0 = left).
        num_channels: Total number of interleaved channels (e.g. 2 for stereo).

    Returns:
        list[int]: Signed 32-bit integer samples for the selected channel.
                   Empty list if raw_bytes is empty or too short for one frame.

    Raises:
        ValueError: If channel index is out of range.

    Example:
        # Stereo S32_LE: [L0, R0, L1, R1, ...]
        left = extract_channel(raw_bytes, channel=0, num_channels=2)
    """
    if channel < 0 or channel >= num_channels:
        raise ValueError(
            f"channel {channel} is out of range for {num_channels} channels"
        )
    bytes_per_sample = 4  # S32_LE → 4 bytes
    frame_size = bytes_per_sample * num_channels
    n_frames = len(raw_bytes) // frame_size
    if n_frames == 0:
        return []

    # Unpack all samples at once (fast path via struct)
    total_samples = n_frames * num_channels
    all_samples: tuple[int, ...] = struct.unpack(f"<{total_samples}i", raw_bytes[:total_samples * 4])
    return list(all_samples[channel::num_channels])


def convert_s32_to_s16(samples: list[int]) -> list[int]:
    """
    Convert signed 32-bit I2S samples to signed 16-bit PCM.

    The MSM261S4030H0R outputs 24-bit audio data in 32-bit I2S words.
    The data occupies bits 31..8 (left-justified in the 32-bit slot);
    bits 7..0 are padding zeros.

    Right-shifting by 16 extracts the top 16 bits of the 24-bit audio
    data, providing 16-bit resolution with minimal quantisation loss.

    The result is clamped to [-32768, 32767] to guard against any
    unexpected overflow from hardware or simulated test data.

    Args:
        samples: Signed 32-bit integer samples.

    Returns:
        list[int]: Signed 16-bit integer samples.
    """
    return [max(-32768, min(32767, s >> 16)) for s in samples]


def resample_3to1(samples: list[int]) -> list[int]:
    """
    Resample a 48 kHz mono sample list to 16 kHz using FIR + decimation.

    Applies the pre-computed 15-tap Hamming-windowed anti-aliasing FIR
    filter (_FIR_48K_TO_16K) and then decimates by factor 3.

    The FIR pass-band (< 6.4 kHz) fully covers speech frequencies
    (300 Hz – 3.4 kHz for telephone-quality, up to ~8 kHz for wideband).

    Args:
        samples: Mono signed integer samples at 48 kHz.

    Returns:
        list[int]: Resampled samples at 16 kHz.

    Note:
        Only valid for exact 3:1 decimation (48000 → 16000 Hz).
        Use resample() for a configurable-ratio version.
    """
    if not samples:
        return []
    return _apply_fir_decimate(samples, _FIR_48K_TO_16K, _FIR_DECIMATION)


def resample(
    samples: list[int],
    input_rate: int,
    output_rate: int,
) -> list[int]:
    """
    Resample mono samples from input_rate to output_rate.

    Currently supports only integer decimation (input_rate / output_rate
    must be a positive integer).  This covers the primary AutoChair use
    case: 48000 / 16000 = 3.

    Args:
        samples:     Mono signed integer samples at input_rate.
        input_rate:  Input sample rate in Hz.
        output_rate: Target output sample rate in Hz.

    Returns:
        list[int]: Resampled samples at output_rate.

    Raises:
        ValueError: If the ratio is not a positive integer.

    To add support for fractional resampling (e.g. 44100 → 16000),
    override this function or replace it with a SoxR/libresample binding.
    """
    if input_rate == output_rate:
        return list(samples)
    if input_rate <= 0 or output_rate <= 0:
        raise ValueError(f"Rates must be positive: {input_rate}, {output_rate}")
    ratio = input_rate / output_rate
    if not ratio.is_integer() or int(ratio) < 1:
        raise ValueError(
            f"resample() currently supports only integer decimation ratios. "
            f"Got {input_rate}/{output_rate} = {ratio:.4f}. "
            f"Implement fractional resampling if required."
        )
    factor = int(ratio)
    if factor == 3 and input_rate == 48000 and output_rate == 16000:
        # Use the pre-computed 48→16 kHz coefficients (fast path)
        return _apply_fir_decimate(samples, _FIR_48K_TO_16K, factor)
    # Generic: design an on-the-fly anti-aliasing filter
    fir = _design_hamming_sinc_fir(taps=15, cutoff_normalized=0.5 / factor)
    return _apply_fir_decimate(samples, fir, factor)


def samples_to_bytes(samples: list[int]) -> bytes:
    """
    Pack signed 16-bit integer samples into little-endian bytes.

    Args:
        samples: Signed 16-bit integer samples.

    Returns:
        bytes: Little-endian S16_LE PCM bytes suitable for Vosk.
    """
    if not samples:
        return b""
    arr = array.array("h", samples)
    return arr.tobytes()


# ---------------------------------------------------------------------------
# Internal helper
# ---------------------------------------------------------------------------

def _apply_fir_decimate(
    samples: list[int],
    fir: list[float],
    factor: int,
) -> list[int]:
    """
    Apply a FIR filter to samples and decimate by factor.

    Uses direct-form FIR convolution evaluated only at output sample
    positions (every factor-th input sample) to avoid computing discarded
    intermediate samples.

    Args:
        samples: Input integer samples.
        fir:     FIR filter coefficient list.
        factor:  Decimation factor.

    Returns:
        list[int]: Filtered and decimated integer samples.
    """
    n = len(samples)
    taps = len(fir)
    half = taps // 2
    out: list[int] = []
    # Evaluate filter only at output positions (every factor-th sample)
    for i in range(0, n, factor):
        acc = 0.0
        for j, h in enumerate(fir):
            k = i + j - half
            if 0 <= k < n:
                acc += h * samples[k]
        out.append(int(acc))
    return out


# ---------------------------------------------------------------------------
# Preprocessor class — combines all steps
# ---------------------------------------------------------------------------

class AudioPreprocessor:
    """
    Converts raw MSM261S4030H0R capture bytes to Vosk-ready PCM.

    Combines extract_channel → convert_s32_to_s16 → resample into a
    single process() call.  All steps are configurable via AudioConfig.

    Thread safety: instances are stateless across calls; process() does
    not mutate any instance state.  Multiple threads can safely share
    one AudioPreprocessor instance.

    Hardware status: NO HARDWARE ACCESS.
    Fully testable on Mac without any audio devices.
    """

    def __init__(self, config: AudioConfig) -> None:
        """
        Args:
            config: AudioConfig supplying channel index, format, and rates.
        """
        self._config = config
        logger.info(
            f"AudioPreprocessor initialised "
            f"(channel={config.mic_channel}, "
            f"{config.resample_input_rate} Hz → "
            f"{config.resample_output_rate} Hz, "
            f"S{config.capture_format_bits}_LE → S16_LE)"
        )

    def process(self, raw_bytes: bytes) -> bytes:
        """
        Convert raw S32_LE stereo capture bytes to mono S16_LE at target rate.

        Steps:
          1. extract_channel — select mic_channel from interleaved stereo
          2. convert_s32_to_s16 — right-shift 32-bit → 16-bit
          3. resample — anti-aliased FIR + decimation to vosk_sample_rate
          4. samples_to_bytes — pack as little-endian S16_LE bytes

        Args:
            raw_bytes: Raw ALSA PCM bytes from AudioCapture.read_chunk().
                       Expected format: S32_LE, capture_channels, capture_sample_rate.

        Returns:
            bytes: Mono S16_LE PCM at resample_output_rate, ready for Vosk.
                   Empty bytes if raw_bytes is empty.
        """
        if not raw_bytes:
            logger.debug("AudioPreprocessor.process(): empty input — returning b\"\"")
            return b""

        # Step 1: Extract microphone channel
        s32_samples = extract_channel(
            raw_bytes,
            channel=self._config.mic_channel,
            num_channels=self._config.capture_channels,
        )

        # Step 2: S32 → S16
        s16_samples = convert_s32_to_s16(s32_samples)

        # Step 3: Resample
        resampled = resample(
            s16_samples,
            input_rate=self._config.resample_input_rate,
            output_rate=self._config.resample_output_rate,
        )

        # Step 4: Pack to bytes
        result = samples_to_bytes(resampled)
        logger.debug(
            f"AudioPreprocessor: {len(raw_bytes)} B raw → "
            f"{len(result)} B S16_LE@{self._config.resample_output_rate} Hz"
        )
        return result
