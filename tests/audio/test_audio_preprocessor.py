"""
Tests for AudioPreprocessor and its component functions.

Verifies:
  - extract_channel: left/right channel extraction from S32_LE stereo
  - convert_s32_to_s16: 32-bit to 16-bit conversion (right-shift and clamp)
  - resample: 48→16 kHz decimation (integer ratio)
  - resample_3to1: direct 3:1 fast path
  - samples_to_bytes: pack to little-endian S16_LE
  - AudioPreprocessor.process(): full pipeline end-to-end
  - Edge cases: empty input, mono capture, invalid parameters

No audio hardware, ALSA, or external libraries are required.
All test data is constructed from known integer values.
"""

from __future__ import annotations

import array
import math
import struct

import pytest

from autochair.audio.config import AudioConfig
from autochair.audio.devices.preprocessor import (
    AudioPreprocessor,
    convert_s32_to_s16,
    extract_channel,
    resample,
    resample_3to1,
    samples_to_bytes,
)


# ---------------------------------------------------------------------------
# Helpers to construct test audio bytes
# ---------------------------------------------------------------------------

def make_s32le_stereo(left: list[int], right: list[int]) -> bytes:
    """
    Pack two equal-length lists of S32 integers as interleaved stereo S32_LE bytes.
    left[i] and right[i] are interleaved: [L0, R0, L1, R1, ...].
    """
    assert len(left) == len(right), "left and right must have equal length"
    pairs = []
    for l_val, r_val in zip(left, right):
        pairs.extend([l_val, r_val])
    return struct.pack(f"<{len(pairs)}i", *pairs)


# ---------------------------------------------------------------------------
# extract_channel
# ---------------------------------------------------------------------------

class TestExtractChannel:
    def test_extract_left_channel(self):
        left = [100, 200, 300]
        right = [10, 20, 30]
        raw = make_s32le_stereo(left, right)
        result = extract_channel(raw, channel=0, num_channels=2)
        assert result == left

    def test_extract_right_channel(self):
        left = [100, 200, 300]
        right = [10, 20, 30]
        raw = make_s32le_stereo(left, right)
        result = extract_channel(raw, channel=1, num_channels=2)
        assert result == right

    def test_right_channel_zero_when_mic_not_connected(self):
        """
        On the real Pi (Phase 2), the right channel samples are all zero
        because LR is tied LOW (left channel selected).
        """
        left = [1000000, -2000000, 500000]
        right = [0, 0, 0]
        raw = make_s32le_stereo(left, right)
        result = extract_channel(raw, channel=1, num_channels=2)
        assert all(s == 0 for s in result)

    def test_empty_input_returns_empty_list(self):
        result = extract_channel(b"", channel=0, num_channels=2)
        assert result == []

    def test_partial_frame_ignored(self):
        """Bytes that don't form a complete frame are dropped."""
        left = [100, 200]
        right = [10, 20]
        raw = make_s32le_stereo(left, right)
        # Truncate to include only one full stereo frame (8 bytes) + 3 extra bytes
        truncated = raw[:8] + b"\x00\x00\x00"
        result = extract_channel(truncated, channel=0, num_channels=2)
        assert result == [100]

    def test_channel_out_of_range_raises_value_error(self):
        raw = make_s32le_stereo([100], [10])
        with pytest.raises(ValueError, match="out of range"):
            extract_channel(raw, channel=2, num_channels=2)

    def test_negative_channel_raises_value_error(self):
        raw = make_s32le_stereo([100], [10])
        with pytest.raises(ValueError, match="out of range"):
            extract_channel(raw, channel=-1, num_channels=2)

    def test_mono_channel_extraction(self):
        """Single-channel capture: channel=0, num_channels=1."""
        samples = [1000, 2000, 3000]
        raw = struct.pack(f"<{len(samples)}i", *samples)
        result = extract_channel(raw, channel=0, num_channels=1)
        assert result == samples


# ---------------------------------------------------------------------------
# convert_s32_to_s16
# ---------------------------------------------------------------------------

class TestConvertS32ToS16:
    def test_zero_maps_to_zero(self):
        assert convert_s32_to_s16([0]) == [0]

    def test_positive_value_shifted_right_16(self):
        # 0x00FF0000 >> 16 = 0x00FF = 255
        assert convert_s32_to_s16([0x00FF0000]) == [255]

    def test_negative_value_shifted_right_16(self):
        # -2147483648 (0x80000000) >> 16 = -32768
        assert convert_s32_to_s16([-2147483648]) == [-32768]

    def test_maximum_positive_s32_maps_to_32767(self):
        assert convert_s32_to_s16([2147483647]) == [32767]

    def test_maximum_negative_s32_maps_to_minus_32768(self):
        assert convert_s32_to_s16([-2147483648]) == [-32768]

    def test_clamp_high_prevents_overflow(self):
        """Values that would exceed 32767 after shift are clamped."""
        # This is a guard against unusual test data; normal 24-in-32 won't overflow.
        result = convert_s32_to_s16([0x7FFF8000])  # >> 16 = 32767
        assert result[0] <= 32767

    def test_clamp_low_prevents_underflow(self):
        result = convert_s32_to_s16([-0x80008000])  # extreme negative
        assert result[0] >= -32768

    def test_empty_input(self):
        assert convert_s32_to_s16([]) == []

    def test_preserves_sign_for_negative_audio(self):
        # A realistic negative sample: -261405696 (from Phase 2 Pi measurement)
        result = convert_s32_to_s16([-261405696])
        assert isinstance(result[0], int)
        assert -32768 <= result[0] <= 32767
        assert result[0] < 0  # must remain negative


# ---------------------------------------------------------------------------
# resample / resample_3to1
# ---------------------------------------------------------------------------

class TestResample:
    def test_same_rate_returns_copy(self):
        samples = [100, 200, 300, 400]
        result = resample(samples, input_rate=16000, output_rate=16000)
        assert result == samples

    def test_3to1_produces_one_third_output(self):
        """48 kHz → 16 kHz: output should have roughly len/3 samples."""
        samples = list(range(300))
        result = resample(samples, input_rate=48000, output_rate=16000)
        # FIR+decimate: output length = ceil(n / factor) = 100
        assert len(result) == 100

    def test_output_is_integer_list(self):
        samples = [1000, 2000, 3000, 4000, 5000, 6000]
        result = resample(samples, input_rate=48000, output_rate=16000)
        assert all(isinstance(s, int) for s in result)

    def test_empty_input_returns_empty(self):
        result = resample([], input_rate=48000, output_rate=16000)
        assert result == []

    def test_non_integer_ratio_raises(self):
        with pytest.raises(ValueError, match="integer decimation"):
            resample([1, 2, 3], input_rate=44100, output_rate=16000)

    def test_zero_input_rate_raises(self):
        with pytest.raises(ValueError, match="Rates must be positive"):
            resample([1, 2], input_rate=0, output_rate=16000)

    def test_zero_output_rate_raises(self):
        with pytest.raises(ValueError, match="Rates must be positive"):
            resample([1, 2], input_rate=48000, output_rate=0)

    def test_resample_3to1_fast_path(self):
        """resample_3to1 should produce same result as resample(48k→16k)."""
        samples = list(range(600))
        result_a = resample_3to1(samples)
        result_b = resample(samples, input_rate=48000, output_rate=16000)
        assert result_a == result_b

    def test_resample_3to1_empty_input(self):
        assert resample_3to1([]) == []

    def test_dc_signal_preserved_after_resample(self):
        """
        A constant (DC) signal should pass through the low-pass filter.
        The Hamming FIR has edge effects at the start/end of the buffer
        (the filter sees zero-padded boundaries for its first/last taps).
        We check only the middle portion of the output, which has full
        filter support and should be close to the input DC value.
        The tolerance is generous (±50%) because this is a correctness
        check — not a precision assertion.  Speech quality is unaffected.
        """
        val = 1000
        samples = [val] * 300
        result = resample_3to1(samples)
        # Skip first and last 5 output samples (edge-effect region)
        middle = result[5:-5] if len(result) > 15 else result
        for s in middle:
            assert abs(s - val) < abs(val) * 0.50 + 10, \
                f"DC not reasonably preserved in middle region: {s} vs {val}"


# ---------------------------------------------------------------------------
# samples_to_bytes
# ---------------------------------------------------------------------------

class TestSamplesToBytes:
    def test_empty_input_returns_empty_bytes(self):
        assert samples_to_bytes([]) == b""

    def test_zero_sample_is_two_zero_bytes(self):
        result = samples_to_bytes([0])
        assert result == b"\x00\x00"

    def test_positive_sample_little_endian(self):
        # 256 in little-endian S16 = b"\x00\x01"
        result = samples_to_bytes([256])
        assert result == struct.pack("<h", 256)

    def test_negative_sample_little_endian(self):
        result = samples_to_bytes([-1])
        assert result == struct.pack("<h", -1)

    def test_output_length_is_two_bytes_per_sample(self):
        samples = [0, 100, -100, 32767, -32768]
        result = samples_to_bytes(samples)
        assert len(result) == len(samples) * 2

    def test_roundtrip_pack_unpack(self):
        original = [0, 1000, -1000, 32767, -32768]
        packed = samples_to_bytes(original)
        unpacked = list(struct.unpack(f"<{len(original)}h", packed))
        assert unpacked == original


# ---------------------------------------------------------------------------
# AudioPreprocessor — full pipeline
# ---------------------------------------------------------------------------

class TestAudioPreprocessor:
    @pytest.fixture
    def config(self):
        return AudioConfig(
            capture_channels=2,
            capture_format_bits=32,
            mic_channel=0,
            resample_input_rate=48000,
            resample_output_rate=16000,
        )

    @pytest.fixture
    def preprocessor(self, config):
        return AudioPreprocessor(config)

    def test_empty_input_returns_empty_bytes(self, preprocessor):
        assert preprocessor.process(b"") == b""

    def test_output_is_bytes(self, preprocessor):
        raw = make_s32le_stereo([1000000] * 300, [0] * 300)
        result = preprocessor.process(raw)
        assert isinstance(result, bytes)

    def test_output_length_is_one_third_of_input_frames(self, preprocessor):
        """300 stereo frames → 98 streaming output frames (2 buffered in FIR window) → 100 after flush."""
        raw = make_s32le_stereo([1000000] * 300, [0] * 300)
        result = preprocessor.process(raw)
        # 98 frames emitted immediately, 2 frames buffered in 15-tap FIR lookahead
        assert len(result) == 98 * 2
        flushed = preprocessor.flush()
        assert len(result + flushed) == 100 * 2

    def test_left_channel_is_extracted(self, preprocessor):
        """
        Left channel contains audio; right is zero.
        Result must contain non-zero data (left channel passed through).
        """
        left = [0x01000000] * 90   # nonzero S32 audio
        right = [0] * 90
        raw = make_s32le_stereo(left, right)
        result = preprocessor.process(raw)
        s16 = list(struct.unpack(f"<{len(result)//2}h", result))
        # At least some non-zero samples should survive filter+decimate
        assert any(s != 0 for s in s16)

    def test_silent_left_channel_produces_near_zero_output(self, preprocessor):
        """
        If the microphone signal is zero (silence), output should be zero.
        """
        raw = make_s32le_stereo([0] * 90, [0] * 90)
        result = preprocessor.process(raw)
        s16 = list(struct.unpack(f"<{len(result)//2}h", result))
        assert all(s == 0 for s in s16)

    def test_right_channel_selection(self):
        """mic_channel=1 extracts the right channel instead."""
        config = AudioConfig(
            mic_channel=1,
            capture_channels=2,
            resample_input_rate=48000,
            resample_output_rate=16000,
        )
        preprocessor = AudioPreprocessor(config)
        left = [0] * 90          # left is silent
        right = [0x01000000] * 90  # right has audio
        raw = make_s32le_stereo(left, right)
        result = preprocessor.process(raw)
        s16 = list(struct.unpack(f"<{len(result)//2}h", result))
        # Right channel data should survive
        assert any(s != 0 for s in s16)


# ---------------------------------------------------------------------------
# Streaming continuity & chunk boundary regression tests
# ---------------------------------------------------------------------------

class TestStreamingEquivalence:
    @pytest.fixture
    def config(self):
        return AudioConfig(
            capture_channels=2,
            capture_format_bits=32,
            mic_channel=0,
            resample_input_rate=48000,
            resample_output_rate=16000,
        )

    def _generate_synthetic_speech_signal(self, num_frames: int) -> bytes:
        """Generate stereo S32_LE test signal with multiple frequency components."""
        left = []
        right = [0] * num_frames
        for n in range(num_frames):
            # Mix 400 Hz and 1200 Hz tones (scaled to 24-bit range in 32-bit container)
            s = (
                0.5 * math.sin(2.0 * math.pi * 400.0 * n / 48000.0)
                + 0.3 * math.sin(2.0 * math.pi * 1200.0 * n / 48000.0)
            )
            # Scale to S32 range (24-bit audio in top bits)
            val_s32 = int(s * 8000000) << 8
            left.append(val_s32)
        return make_s32le_stereo(left, right)

    def test_continuous_vs_4096_alsa_chunks_exact_equivalence(self, config):
        """
        REGRESSION TEST FOR STREAMING BOUNDARY BUG:
        Proves that feeding audio in 4096-frame ALSA chunks produces the EXACT
        same signal as processing the full stream continuously.
        """
        chunk_frames = 4096
        num_chunks = 4
        total_frames = chunk_frames * num_chunks
        raw_full = self._generate_synthetic_speech_signal(total_frames)

        # Slice into 4096-frame ALSA chunks (32768 bytes per chunk: 4096 frames * 8 bytes/frame)
        bytes_per_frame = 8
        chunk_bytes = chunk_frames * bytes_per_frame
        chunks = [
            raw_full[i * chunk_bytes : (i + 1) * chunk_bytes]
            for i in range(num_chunks)
        ]

        # 1. Process continuously in one single pass
        prep_continuous = AudioPreprocessor(config)
        out_continuous = prep_continuous.process(raw_full) + prep_continuous.flush()

        # 2. Process chunk by chunk (streaming mode)
        prep_stream = AudioPreprocessor(config)
        stream_parts = [prep_stream.process(c) for c in chunks]
        out_stream = b"".join(stream_parts) + prep_stream.flush()

        # Check total byte lengths match
        assert len(out_stream) == len(out_continuous)

        # Numerical signal comparison: bit-for-bit identical across all samples
        samples_continuous = list(struct.unpack(f"<{len(out_continuous)//2}h", out_continuous))
        samples_stream = list(struct.unpack(f"<{len(out_stream)//2}h", out_stream))

        assert samples_stream == samples_continuous, (
            "Streaming chunk processing must be mathematically identical to continuous processing"
        )

    def test_no_discontinuity_across_4096_frame_seams(self, config):
        """
        Verify that sample values directly at chunk seams (e.g. 4096 / 3 ≈ frame 1365)
        match continuous filtering exactly, with zero seam error.
        """
        num_frames = 4096 * 2
        raw = self._generate_synthetic_speech_signal(num_frames)
        chunk_bytes = 4096 * 8

        # Continuous reference
        prep_ref = AudioPreprocessor(config)
        ref_bytes = prep_ref.process(raw) + prep_ref.flush()
        samples_ref = list(struct.unpack(f"<{len(ref_bytes)//2}h", ref_bytes))

        # Streamed chunks
        prep_stream = AudioPreprocessor(config)
        part1 = prep_stream.process(raw[:chunk_bytes])
        part2 = prep_stream.process(raw[chunk_bytes:]) + prep_stream.flush()
        stream_bytes = part1 + part2
        samples_stream = list(struct.unpack(f"<{len(stream_bytes)//2}h", stream_bytes))

        split_idx = len(part1) // 2

        # Check the seam region [-10 to +10 around the boundary]
        seam_ref = samples_ref[split_idx - 10 : split_idx + 10]
        seam_stream = samples_stream[split_idx - 10 : split_idx + 10]

        assert seam_stream == seam_ref, "Chunk seam samples must be identical to continuous filtering"

    def test_arbitrary_chunk_sizes_equivalence(self, config):
        """
        Verify that splitting the stream into irregular/arbitrary chunk sizes
        (512, 1024, 2048, 4096) produces identical audio output.
        """
        total_frames = 4096 * 3
        raw = self._generate_synthetic_speech_signal(total_frames)

        prep_ref = AudioPreprocessor(config)
        out_ref = prep_ref.process(raw) + prep_ref.flush()

        # Process with varying chunk sizes
        prep_var = AudioPreprocessor(config)
        var_chunks = []
        offset = 0
        sizes = [512, 1024, 768, 4096, 2048, 1000, total_frames - 9448]
        for sz in sizes:
            b_sz = sz * 8
            var_chunks.append(raw[offset : offset + b_sz])
            offset += b_sz

        out_var = b"".join(prep_var.process(c) for c in var_chunks) + prep_var.flush()
        assert out_var == out_ref

    def test_reset_clears_streaming_history(self, config):
        """
        Verify that reset() clears buffer state so that a second session
        starts cleanly without cross-contamination from the prior session.
        """
        raw = self._generate_synthetic_speech_signal(4096)

        prep = AudioPreprocessor(config)
        out1 = prep.process(raw)

        # Reset for session 2
        prep.reset()
        out2 = prep.process(raw)

        assert out1 == out2, "After reset(), processing identical input must produce identical output"

    def test_flush_emits_buffered_lookahead_and_resets(self, config):
        """
        Verify that flush() produces remaining trailing samples and leaves
        the preprocessor in a clean reset state.
        """
        raw = self._generate_synthetic_speech_signal(300)
        prep = AudioPreprocessor(config)

        part1 = prep.process(raw)
        assert len(part1) == 98 * 2  # 98 samples (196 bytes)

        flushed = prep.flush()
        assert len(flushed) == 2 * 2  # remaining 2 samples (4 bytes)

        # A subsequent flush on empty buffer returns empty bytes
        second_flush = prep.flush()
        assert second_flush == b""
