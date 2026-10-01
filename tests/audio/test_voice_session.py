"""
Unit tests for RealVoiceSession (Audio Phase 4A).

All tests run deterministically with fakes / mocks:
- NO real ALSA hardware
- NO real I2S microphone
- NO real Vosk model loading
- NO real time sleeps (injected clock/time_fn)
"""

from __future__ import annotations

from unittest.mock import MagicMock

import pytest

from autochair.audio.config import AudioConfig
from autochair.audio.devices.capture import SimulatedAudioCapture
from autochair.audio.devices.preprocessor import AudioPreprocessor
from autochair.audio.voice_session import RealVoiceSession


class FakeClock:
    """Deterministic time source that advances by step_seconds on each call."""

    def __init__(self, start: float = 0.0, step: float = 0.1) -> None:
        self.current = start
        self.step = step

    def __call__(self) -> float:
        val = self.current
        self.current += self.step
        return val


@pytest.fixture
def config() -> AudioConfig:
    return AudioConfig(
        voice_session_duration_seconds=5.0,
        vosk_grammar=["start", "stop", "move forward", "[unk]"],
    )


@pytest.fixture
def mock_capture() -> MagicMock:
    capture = MagicMock(spec=SimulatedAudioCapture)
    capture.read_chunk.return_value = b"\x00\x00\x00\x00" * 2
    return capture


@pytest.fixture
def mock_preprocessor() -> MagicMock:
    preprocessor = MagicMock(spec=AudioPreprocessor)
    preprocessor.process.return_value = b"\x00\x00"
    return preprocessor


@pytest.fixture
def mock_recognizer() -> MagicMock:
    recognizer = MagicMock()
    recognizer.recognize.return_value = ""
    recognizer.finalize.return_value = ""
    return recognizer


# ---------------------------------------------------------------------------
# Configuration & Validation Tests
# ---------------------------------------------------------------------------

class TestVoiceSessionConfiguration:
    def test_default_duration_from_config(
        self, mock_capture, mock_preprocessor, mock_recognizer, config
    ):
        config.voice_session_duration_seconds = 4.2
        session = RealVoiceSession(
            capture=mock_capture,
            preprocessor=mock_preprocessor,
            recognizer=mock_recognizer,
            config=config,
        )
        assert session.duration == 4.2

    def test_explicit_duration_override(
        self, mock_capture, mock_preprocessor, mock_recognizer, config
    ):
        session = RealVoiceSession(
            capture=mock_capture,
            preprocessor=mock_preprocessor,
            recognizer=mock_recognizer,
            config=config,
            duration=3.0,
        )
        assert session.duration == 3.0

    def test_invalid_negative_duration_raises_error(
        self, mock_capture, mock_preprocessor, mock_recognizer, config
    ):
        with pytest.raises(ValueError, match="duration must be > 0"):
            RealVoiceSession(
                capture=mock_capture,
                preprocessor=mock_preprocessor,
                recognizer=mock_recognizer,
                config=config,
                duration=-1.0,
            )

    def test_invalid_zero_duration_raises_error(
        self, mock_capture, mock_preprocessor, mock_recognizer, config
    ):
        with pytest.raises(ValueError, match="duration must be > 0"):
            RealVoiceSession(
                capture=mock_capture,
                preprocessor=mock_preprocessor,
                recognizer=mock_recognizer,
                config=config,
                duration=0.0,
            )

    def test_invalid_config_duration_raises_error(self):
        with pytest.raises(ValueError, match="voice_session_duration_seconds must be > 0"):
            AudioConfig(voice_session_duration_seconds=0.0)

        with pytest.raises(ValueError, match="voice_session_duration_seconds must be > 0"):
            AudioConfig(voice_session_duration_seconds=-2.5)


# ---------------------------------------------------------------------------
# Lifecycle & Flow Tests
# ---------------------------------------------------------------------------

class TestVoiceSessionLifecycle:
    def test_capture_lifecycle_on_immediate_recognition(
        self, mock_capture, mock_preprocessor, mock_recognizer, config
    ):
        mock_recognizer.recognize.return_value = "stop"

        session = RealVoiceSession(
            capture=mock_capture,
            preprocessor=mock_preprocessor,
            recognizer=mock_recognizer,
            config=config,
        )
        result = session.start()

        assert result == "stop"
        mock_capture.open.assert_called_once()
        mock_capture.read_chunk.assert_called_once()
        mock_capture.close.assert_called_once()
        # finalize should NOT be called after immediate recognition
        mock_recognizer.finalize.assert_not_called()

    def test_audio_data_flows_through_preprocessor_to_recognizer(
        self, mock_capture, mock_preprocessor, mock_recognizer, config
    ):
        raw_bytes = b"raw_hardware_pcm"
        processed_bytes = b"clean_s16_pcm"

        mock_capture.read_chunk.return_value = raw_bytes
        mock_preprocessor.process.return_value = processed_bytes
        mock_recognizer.recognize.return_value = "move forward"

        session = RealVoiceSession(
            capture=mock_capture,
            preprocessor=mock_preprocessor,
            recognizer=mock_recognizer,
            config=config,
        )
        result = session.start()

        assert result == "move forward"
        mock_preprocessor.process.assert_called_with(raw_bytes)
        mock_recognizer.recognize.assert_called_with(processed_bytes)

    def test_empty_raw_chunk_does_not_call_preprocessor_or_recognizer(
        self, mock_capture, mock_preprocessor, mock_recognizer, config
    ):
        clock = FakeClock(start=0.0, step=3.0)  # Exceeds 5s on 2nd step
        mock_capture.read_chunk.return_value = b""

        session = RealVoiceSession(
            capture=mock_capture,
            preprocessor=mock_preprocessor,
            recognizer=mock_recognizer,
            config=config,
            time_fn=clock,
        )
        result = session.start()

        assert result == ""
        mock_preprocessor.process.assert_not_called()
        mock_recognizer.recognize.assert_not_called()
        mock_recognizer.finalize.assert_called_once()
        mock_capture.close.assert_called_once()

    def test_timeout_calls_finalize_and_returns_finalized_text(
        self, mock_capture, mock_preprocessor, mock_recognizer, config
    ):
        # Clock advances 1.0s each call. With duration=5.0s, loop runs multiple times
        clock = FakeClock(start=0.0, step=1.0)
        mock_recognizer.recognize.return_value = ""  # keeps waiting
        mock_recognizer.finalize.return_value = "move forward"

        session = RealVoiceSession(
            capture=mock_capture,
            preprocessor=mock_preprocessor,
            recognizer=mock_recognizer,
            config=config,
            time_fn=clock,
        )
        result = session.start()

        assert result == "move forward"
        mock_capture.open.assert_called_once()
        mock_capture.close.assert_called_once()
        mock_recognizer.finalize.assert_called_once()

    def test_timeout_with_empty_finalize_returns_empty_string(
        self, mock_capture, mock_preprocessor, mock_recognizer, config
    ):
        clock = FakeClock(start=0.0, step=2.0)
        mock_recognizer.recognize.return_value = ""
        mock_recognizer.finalize.return_value = ""

        session = RealVoiceSession(
            capture=mock_capture,
            preprocessor=mock_preprocessor,
            recognizer=mock_recognizer,
            config=config,
            time_fn=clock,
        )
        result = session.start()

        assert result == ""
        mock_recognizer.finalize.assert_called_once()
        mock_capture.close.assert_called_once()

    def test_run_is_alias_for_start(
        self, mock_capture, mock_preprocessor, mock_recognizer, config
    ):
        mock_recognizer.recognize.return_value = "start"

        session = RealVoiceSession(
            capture=mock_capture,
            preprocessor=mock_preprocessor,
            recognizer=mock_recognizer,
            config=config,
        )
        result = session.run()

        assert result == "start"
        mock_capture.open.assert_called_once()
        mock_capture.close.assert_called_once()


# ---------------------------------------------------------------------------
# Exception & Cleanup Tests
# ---------------------------------------------------------------------------

class TestVoiceSessionExceptionsAndCleanup:
    def test_capture_close_called_when_read_chunk_raises(
        self, mock_capture, mock_preprocessor, mock_recognizer, config
    ):
        mock_capture.read_chunk.side_effect = RuntimeError("ALSA buffer overrun")

        session = RealVoiceSession(
            capture=mock_capture,
            preprocessor=mock_preprocessor,
            recognizer=mock_recognizer,
            config=config,
        )
        with pytest.raises(RuntimeError, match="ALSA buffer overrun"):
            session.start()

        mock_capture.close.assert_called_once()

    def test_capture_close_called_when_preprocess_raises(
        self, mock_capture, mock_preprocessor, mock_recognizer, config
    ):
        mock_preprocessor.process.side_effect = ValueError("Corrupt audio frame")

        session = RealVoiceSession(
            capture=mock_capture,
            preprocessor=mock_preprocessor,
            recognizer=mock_recognizer,
            config=config,
        )
        with pytest.raises(ValueError, match="Corrupt audio frame"):
            session.start()

        mock_capture.close.assert_called_once()

    def test_capture_close_called_when_recognize_raises(
        self, mock_capture, mock_preprocessor, mock_recognizer, config
    ):
        mock_recognizer.recognize.side_effect = RuntimeError("Vosk engine error")

        session = RealVoiceSession(
            capture=mock_capture,
            preprocessor=mock_preprocessor,
            recognizer=mock_recognizer,
            config=config,
        )
        with pytest.raises(RuntimeError, match="Vosk engine error"):
            session.start()

        mock_capture.close.assert_called_once()

    def test_capture_close_called_when_open_raises(
        self, mock_capture, mock_preprocessor, mock_recognizer, config
    ):
        mock_capture.open.side_effect = RuntimeError("Device busy")

        session = RealVoiceSession(
            capture=mock_capture,
            preprocessor=mock_preprocessor,
            recognizer=mock_recognizer,
            config=config,
        )
        with pytest.raises(RuntimeError, match="Device busy"):
            session.start()

        mock_capture.close.assert_called_once()


# ---------------------------------------------------------------------------
# Session Isolation & Multi-session Tests
# ---------------------------------------------------------------------------

class TestVoiceSessionIsolation:
    def test_multiple_sequential_sessions_isolate_capture_lifecycles(
        self, mock_capture, mock_preprocessor, mock_recognizer, config
    ):
        clock = FakeClock(start=0.0, step=2.0)
        mock_recognizer.recognize.side_effect = ["stop", "move forward"]

        session = RealVoiceSession(
            capture=mock_capture,
            preprocessor=mock_preprocessor,
            recognizer=mock_recognizer,
            config=config,
            time_fn=clock,
        )

        # Session 1
        res1 = session.start()
        assert res1 == "stop"
        assert mock_capture.open.call_count == 1
        assert mock_capture.close.call_count == 1

        # Session 2
        res2 = session.start()
        assert res2 == "move forward"
        assert mock_capture.open.call_count == 2
        assert mock_capture.close.call_count == 2

    def test_recognizer_reset_can_be_invoked_between_sessions(
        self, mock_capture, mock_preprocessor, mock_recognizer, config
    ):
        mock_recognizer.recognize.return_value = "stop"

        session = RealVoiceSession(
            capture=mock_capture,
            preprocessor=mock_preprocessor,
            recognizer=mock_recognizer,
            config=config,
        )
        res = session.start()
        assert res == "stop"

        # Explicit reset through existing recognizer interface
        mock_recognizer.reset()
        mock_recognizer.reset.assert_called_once()
