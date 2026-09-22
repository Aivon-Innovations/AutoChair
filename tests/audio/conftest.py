"""
Shared fixtures for tests/audio/.

This conftest provides:
  - vosk_mock: injects a MagicMock into sys.modules['vosk'] so that
    VoskSpeechRecognizer can be instantiated on Mac without vosk
    installed.  The mock satisfies all vosk API calls in tests.
  - default_audio_config: a standard AudioConfig suitable for most tests.
"""

from __future__ import annotations

import sys
from unittest.mock import MagicMock

import pytest

from autochair.audio.config import AudioConfig


# ---------------------------------------------------------------------------
# Vosk module mock
# ---------------------------------------------------------------------------

def _make_vosk_mock() -> MagicMock:
    """
    Build a MagicMock that satisfies the VoskSpeechRecognizer API.

    The mock covers:
      vosk.SetLogLevel(n)
      vosk.Model(path)                 → MagicMock model object
      vosk.KaldiRecognizer(model, rate, grammar_json)
        .AcceptWaveform(bytes)         → bool
        .Result()                      → JSON string
        .PartialResult()               → JSON string
        .FinalResult()                 → JSON string
    """
    mock = MagicMock()
    mock.SetLogLevel = MagicMock()

    # Model
    mock_model = MagicMock()
    mock.Model.return_value = mock_model

    # KaldiRecognizer — default: AcceptWaveform returns True, Result → stop
    mock_rec = MagicMock()
    mock_rec.AcceptWaveform.return_value = True
    mock_rec.Result.return_value = '{"text": "stop"}'
    mock_rec.PartialResult.return_value = '{"partial": ""}'
    mock_rec.FinalResult.return_value = '{"text": ""}'
    mock.KaldiRecognizer.return_value = mock_rec

    return mock


@pytest.fixture(scope="session", autouse=True)
def inject_vosk_mock() -> None:
    """
    Session-scoped fixture that injects the vosk mock into sys.modules.

    Runs automatically for all tests in tests/audio/.  Does NOT affect
    tests that import from outside this directory.

    If vosk is genuinely installed (e.g. on the Pi), this fixture still
    replaces it with a mock to keep tests deterministic and fast.
    """
    sys.modules["vosk"] = _make_vosk_mock()


# ---------------------------------------------------------------------------
# Shared config fixture
# ---------------------------------------------------------------------------

@pytest.fixture
def default_config() -> AudioConfig:
    """Return a standard AudioConfig for unit tests."""
    return AudioConfig(
        vosk_model_path="fake/model/path",
        vosk_sample_rate=16000,
        vosk_grammar=["start", "stop", "move forward", "[unk]"],
        vosk_suppress_logs=True,
        capture_sample_rate=48000,
        capture_channels=2,
        capture_format_bits=32,
        capture_chunk_frames=4096,
        mic_channel=0,
        resample_input_rate=48000,
        resample_output_rate=16000,
    )
