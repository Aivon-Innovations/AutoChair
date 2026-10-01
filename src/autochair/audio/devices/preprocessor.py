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
      ▼ _resample_stream() (stateful) / resample() (stateless batch)
  mono S16 samples (16 kHz)
      │
      ▼ samples_to_bytes()
  bytes → Vosk KaldiRecognizer

Streaming Decimation & Statefulness
-----------------------------------
ALSA captures audio in discrete chunks (typically 4096 frames at 48 kHz).
Because 4096 is not a multiple of the 3:1 decimation factor (4096 % 3 == 1)
and the 15-tap anti-aliasing FIR filter requires boundary history and lookahead,
stateless per-chunk processing introduces:
  1. Decimation phase slips across chunk boundaries.
  2. Boundary impulse transients (clicks/dips) from zero-padding filter edges.

The AudioPreprocessor class maintains stateful streaming filter history and phase
alignment across successive process() calls. This ensures mathematical equivalence
between streaming chunked processing and continuous single-pass processing.

Call reset() between distinct audio sessions to reinitialize streaming state.
Call flush() at the end of a stream to retrieve any remaining buffered samples.

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
# Public functions (stateless batch helpers, testable independently)
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

    Stateless batch helper for single complete arrays.

    Applies the pre-computed 15-tap Hamming-windowed anti-aliasing FIR
    filter (_FIR_48K_TO_16K) and then decimates by factor 3.

    Args:
        samples: Mono signed integer samples at 48 kHz.

    Returns:
        list[int]: Resampled samples at 16 kHz.
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
    Resample mono samples from input_rate to output_rate (batch / stateless).

    Currently supports integer decimation (input_rate / output_rate
    must be a positive integer). Covers the primary AutoChair use
    case: 48000 / 16000 = 3.

    Args:
        samples:     Mono signed integer samples at input_rate.
        input_rate:  Input sample rate in Hz.
        output_rate: Target output sample rate in Hz.

    Returns:
        list[int]: Resampled samples at output_rate.

    Raises:
        ValueError: If the ratio is not a positive integer.
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
# Internal helper (batch convolution)
# ---------------------------------------------------------------------------

def _apply_fir_decimate(
    samples: list[int],
    fir: list[float],
    factor: int,
) -> list[int]:
    """
    Apply a FIR filter to samples and decimate by factor (batch mode).

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
# Preprocessor class — stateful streaming pipeline
# ---------------------------------------------------------------------------

class AudioPreprocessor:
    """
    Converts raw MSM261S4030H0R capture bytes to Vosk-ready PCM with
    stateful streaming FIR filtering and decimation.

    Combines extract_channel → convert_s32_to_s16 → _resample_stream into a
    single process() call. All steps are configurable via AudioConfig.

    Stateful Streaming
    ------------------
    Maintains filter history buffer and decimation phase alignment across
    successive process() calls so that streaming ALSA chunks (e.g. 4096 frames)
    produce mathematically continuous audio without boundary clicks or phase slips.

    Call reset() to clear state between separate speech sessions.
    Call flush() to retrieve any remaining trailing samples at stream end.

    Hardware status: NO HARDWARE ACCESS.
    Fully testable on Mac without any audio devices.
    """

    def __init__(self, config: AudioConfig) -> None:
        """
        Args:
            config: AudioConfig supplying channel index, format, and rates.
        """
        self._config = config
        ratio = config.resample_input_rate / config.resample_output_rate
        if not ratio.is_integer() or int(ratio) < 1:
            raise ValueError(
                f"AudioPreprocessor currently supports only integer decimation ratios. "
                f"Got {config.resample_input_rate}/{config.resample_output_rate} = {ratio:.4f}."
            )
        self._factor = int(ratio)

        if (
            self._factor == 3
            and config.resample_input_rate == 48000
            and config.resample_output_rate == 16000
        ):
            self._fir = _FIR_48K_TO_16K
        else:
            self._fir = _design_hamming_sinc_fir(
                taps=15, cutoff_normalized=0.5 / self._factor
            )
        self._taps = len(self._fir)
        self._half = self._taps // 2
        self._buffer: list[int] = [0] * self._half

        logger.info(
            f"AudioPreprocessor initialised "
            f"(channel={config.mic_channel}, "
            f"{config.resample_input_rate} Hz → "
            f"{config.resample_output_rate} Hz, "
            f"S{config.capture_format_bits}_LE → S16_LE, "
            f"streaming FIR taps={self._taps})"
        )

    def reset(self) -> None:
        """Reset the streaming filter history buffer."""
        self._buffer = [0] * self._half
        logger.debug("AudioPreprocessor reset: buffer reinitialised to start-of-stream state")

    def process(self, raw_bytes: bytes) -> bytes:
        """
        Convert raw S32_LE stereo capture bytes to mono S16_LE at target rate.

        Maintains FIR filter state across calls for seamless streaming.

        Steps:
          1. extract_channel — select mic_channel from interleaved stereo
          2. convert_s32_to_s16 — right-shift 32-bit → 16-bit
          3. _resample_stream — stateful anti-aliased FIR + decimation to vosk_sample_rate
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

        # Step 3: Stateful resample
        resampled = self._resample_stream(s16_samples)

        # Step 4: Pack to bytes
        result = samples_to_bytes(resampled)
        logger.debug(
            f"AudioPreprocessor: {len(raw_bytes)} B raw → "
            f"{len(result)} B S16_LE@{self._config.resample_output_rate} Hz"
        )
        return result

    def flush(self) -> bytes:
        """
        Flush remaining buffered samples at the end of an audio stream.

        Zero-pads the trailing filter window to emit final samples,
        then resets the buffer state.

        Returns:
            bytes: S16_LE PCM bytes for any final buffered samples.
        """
        if len(self._buffer) <= self._half:
            self.reset()
            return b""

        trailing_zeros = [0] * self._half
        self._buffer.extend(trailing_zeros)

        half = self._half
        fir = self._fir
        factor = self._factor
        buf = self._buffer
        n = len(buf)

        out: list[int] = []
        idx = half
        while idx + half < n:
            acc = 0.0
            for j, h in enumerate(fir):
                acc += h * buf[idx - half + j]
            out.append(int(acc))
            idx += factor

        self.reset()
        return samples_to_bytes(out)

    def _resample_stream(self, samples: list[int]) -> list[int]:
        """
        Stateful streaming FIR decimation.

        Evaluates FIR convolution only when a complete filter window
        (centered at idx, spanning idx-half to idx+half) is available.
        Retains trailing samples in self._buffer for the next chunk call.
        """
        if not samples:
            return []

        if self._config.resample_input_rate == self._config.resample_output_rate:
            return list(samples)

        self._buffer.extend(samples)
        half = self._half
        fir = self._fir
        factor = self._factor
        buf = self._buffer
        n = len(buf)

        out: list[int] = []
        idx = half
        while idx + half < n:
            acc = 0.0
            for j, h in enumerate(fir):
                acc += h * buf[idx - half + j]
            out.append(int(acc))
            idx += factor

        discard = idx - half
        self._buffer = buf[discard:]
        return out
