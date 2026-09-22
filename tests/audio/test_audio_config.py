"""
Tests for AudioConfig.

Verifies:
  - Default values match the verified MSM261S4030H0R Pi hardware parameters.
  - Derived properties (bytes_per_capture_sample, resample_ratio).
  - Custom values are accepted and stored correctly.
  - Validation errors are raised for invalid parameters.
  - vosk_grammar always contains "[unk]".
  - [unk] is auto-appended if missing from the grammar.
"""

from __future__ import annotations

import pytest

from autochair.audio.config import AudioConfig


# ---------------------------------------------------------------------------
# Default values
# ---------------------------------------------------------------------------

class TestAudioConfigDefaults:
    def test_default_alsa_device(self):
        cfg = AudioConfig()
        assert cfg.alsa_device == "hw:CARD=MSM261S4030H0,DEV=0"

    def test_default_capture_sample_rate(self):
        assert AudioConfig().capture_sample_rate == 48000

    def test_default_capture_channels(self):
        assert AudioConfig().capture_channels == 2

    def test_default_capture_format_bits(self):
        assert AudioConfig().capture_format_bits == 32

    def test_default_mic_channel(self):
        """Left channel (LR LOW) is default — matches Phase 2 wiring."""
        assert AudioConfig().mic_channel == 0

    def test_default_vosk_model_path_is_empty(self):
        """Empty path signals model not yet configured."""
        assert AudioConfig().vosk_model_path == ""

    def test_default_vosk_sample_rate(self):
        assert AudioConfig().vosk_sample_rate == 16000

    def test_default_resample_input_rate(self):
        assert AudioConfig().resample_input_rate == 48000

    def test_default_resample_output_rate(self):
        assert AudioConfig().resample_output_rate == 16000

    def test_default_grammar_contains_required_commands(self):
        grammar = AudioConfig().vosk_grammar
        assert "start" in grammar
        assert "stop" in grammar
        assert "move forward" in grammar

    def test_default_grammar_contains_unk(self):
        """[unk] must always be present so unknown speech is labelled."""
        assert "[unk]" in AudioConfig().vosk_grammar

    def test_default_suppress_logs(self):
        assert AudioConfig().vosk_suppress_logs is True


# ---------------------------------------------------------------------------
# Derived properties
# ---------------------------------------------------------------------------

class TestAudioConfigDerivedProperties:
    def test_bytes_per_capture_sample_default(self):
        # 32 bits / 8 * 2 channels = 8 bytes per stereo frame
        cfg = AudioConfig()
        assert cfg.bytes_per_capture_sample == 8

    def test_bytes_per_capture_sample_mono_16bit(self):
        cfg = AudioConfig(capture_channels=1, capture_format_bits=16)
        assert cfg.bytes_per_capture_sample == 2

    def test_resample_ratio_default(self):
        # 48000 / 16000 = 3.0
        cfg = AudioConfig()
        assert cfg.resample_ratio == pytest.approx(3.0)

    def test_resample_ratio_same_rate(self):
        cfg = AudioConfig(resample_input_rate=16000, resample_output_rate=16000)
        assert cfg.resample_ratio == pytest.approx(1.0)


# ---------------------------------------------------------------------------
# Custom values accepted
# ---------------------------------------------------------------------------

class TestAudioConfigCustomValues:
    def test_custom_alsa_device(self):
        cfg = AudioConfig(alsa_device="hw:1,0")
        assert cfg.alsa_device == "hw:1,0"

    def test_custom_capture_sample_rate(self):
        cfg = AudioConfig(capture_sample_rate=44100)
        assert cfg.capture_sample_rate == 44100

    def test_custom_vosk_model_path(self):
        cfg = AudioConfig(vosk_model_path="vosk-model-small-en-us-0.15")
        assert cfg.vosk_model_path == "vosk-model-small-en-us-0.15"

    def test_custom_grammar(self):
        g = ["stop", "[unk]"]
        cfg = AudioConfig(vosk_grammar=g)
        assert "stop" in cfg.vosk_grammar
        assert "[unk]" in cfg.vosk_grammar

    def test_right_channel_selection(self):
        """mic_channel=1 is valid for stereo capture."""
        cfg = AudioConfig(mic_channel=1, capture_channels=2)
        assert cfg.mic_channel == 1


# ---------------------------------------------------------------------------
# Grammar auto-correction
# ---------------------------------------------------------------------------

class TestAudioConfigGrammarAutoCorrection:
    def test_unk_auto_appended_when_missing(self):
        cfg = AudioConfig(vosk_grammar=["start", "stop"])
        assert "[unk]" in cfg.vosk_grammar

    def test_unk_not_duplicated_when_already_present(self):
        cfg = AudioConfig(vosk_grammar=["start", "[unk]"])
        assert cfg.vosk_grammar.count("[unk]") == 1


# ---------------------------------------------------------------------------
# Validation errors
# ---------------------------------------------------------------------------

class TestAudioConfigValidationErrors:
    def test_zero_capture_channels_rejected(self):
        with pytest.raises(ValueError, match="capture_channels"):
            AudioConfig(capture_channels=0)

    def test_mic_channel_out_of_range_rejected(self):
        with pytest.raises(ValueError, match="mic_channel"):
            AudioConfig(mic_channel=2, capture_channels=2)

    def test_invalid_format_bits_rejected(self):
        with pytest.raises(ValueError, match="capture_format_bits"):
            AudioConfig(capture_format_bits=8)

    def test_zero_resample_rate_rejected(self):
        with pytest.raises(ValueError, match="resample rates"):
            AudioConfig(resample_input_rate=0, resample_output_rate=16000)

    def test_negative_resample_rate_rejected(self):
        with pytest.raises(ValueError, match="resample rates"):
            AudioConfig(resample_input_rate=48000, resample_output_rate=-1)
