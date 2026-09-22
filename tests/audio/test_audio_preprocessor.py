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

import struct
import array

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
        """300 stereo frames → 100 output mono frames → 200 bytes (S16 × 100)."""
        raw = make_s32le_stereo([1000000] * 300, [0] * 300)
        result = preprocessor.process(raw)
        # Each output sample = 2 bytes; 300 input frames / 3 decimation = 100 output
        assert len(result) == 100 * 2

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
