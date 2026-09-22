"""
Tests for VoskSpeechRecognizer.

All tests run without the real vosk library or Raspberry Pi hardware.
The vosk module is replaced by a MagicMock in conftest.py (session scope).

Verifies:
  - Construction with valid AudioConfig
  - ImportError on missing vosk (simulated)
  - RuntimeError on empty model path
  - RuntimeError on model load failure
  - Grammar JSON construction
  - recognize() → correct text extraction
  - recognize() → [unk] returns empty string (safely ignored)
  - recognize() → empty audio returns empty string (no error)
  - recognize() → partial result handling
  - finalize() → final result extraction
  - reset() → creates a fresh KaldiRecognizer
  - JSON parse errors handled gracefully
  - START: recognized text flows to VoiceCommandParser correctly
  - STOP: recognized text flows to VoiceCommandParser correctly
  - FORWARD: recognized text flows to VoiceCommandParser correctly
  - UNKNOWN: [unk] produces no InputCommand
  - Voice source is always InputSource.VOICE
  - START produces no InputCommand (safety boundary)
  - Existing InputCommandValidator is used (FORWARD/STOP pass validation)
  - Model initialization failure is handled (RuntimeError)
"""

from __future__ import annotations

import json
import sys
from unittest.mock import MagicMock, patch

import pytest

from autochair.audio.config import AudioConfig
from autochair.audio.commands.parser import VoiceCommandParser, VoiceIntent
from autochair.audio.speech.vosk_recognizer import VoskSpeechRecognizer
from autochair.input.command import InputSource
from autochair.input.validator import InputCommandValidator


# conftest.py injects sys.modules['vosk'] = MagicMock() (session scope)
# Individual tests configure mock_rec behaviour per test.


def _get_mock_rec() -> MagicMock:
    """Return the mock KaldiRecognizer instance from the injected vosk mock."""
    vosk_mock = sys.modules["vosk"]
    return vosk_mock.KaldiRecognizer.return_value


@pytest.fixture
def config() -> AudioConfig:
    return AudioConfig(
        vosk_model_path="fake/model",
        vosk_sample_rate=16000,
        vosk_grammar=["start", "stop", "move forward", "[unk]"],
        vosk_suppress_logs=True,
    )


@pytest.fixture
def recognizer(config) -> VoskSpeechRecognizer:
    return VoskSpeechRecognizer(config)


# ---------------------------------------------------------------------------
# Construction
# ---------------------------------------------------------------------------

class TestVoskSpeechRecognizerConstruction:
    def test_construction_succeeds_with_valid_config(self, config):
        rec = VoskSpeechRecognizer(config)
        assert rec is not None

    def test_model_is_loaded_from_config_path(self, config):
        vosk_mock = sys.modules["vosk"]
        VoskSpeechRecognizer(config)
        vosk_mock.Model.assert_called_with(str(config.vosk_model_path))

    def test_kaldi_recognizer_created_with_grammar(self, config):
        vosk_mock = sys.modules["vosk"]
        VoskSpeechRecognizer(config)
        call_args = vosk_mock.KaldiRecognizer.call_args
        grammar_arg = call_args[0][2]  # third positional arg
        grammar = json.loads(grammar_arg)
        assert "start" in grammar
        assert "stop" in grammar
        assert "move forward" in grammar
        assert "[unk]" in grammar

    def test_kaldi_recognizer_receives_correct_sample_rate(self, config):
        vosk_mock = sys.modules["vosk"]
        VoskSpeechRecognizer(config)
        call_args = vosk_mock.KaldiRecognizer.call_args
        rate_arg = call_args[0][1]
        assert float(rate_arg) == pytest.approx(16000.0)

    def test_empty_model_path_raises_runtime_error(self, config):
        config.vosk_model_path = ""
        with pytest.raises(RuntimeError, match="vosk_model_path is empty"):
            VoskSpeechRecognizer(config)

    def test_model_load_failure_raises_runtime_error(self, config):
        vosk_mock = sys.modules["vosk"]
        vosk_mock.Model.side_effect = Exception("model not found")
        try:
            with pytest.raises(RuntimeError, match="Failed to load Vosk model"):
                VoskSpeechRecognizer(config)
        finally:
            # Reset side_effect for other tests
            vosk_mock.Model.side_effect = None
            vosk_mock.Model.return_value = MagicMock()

    def test_set_log_level_called_when_suppress_logs(self, config):
        vosk_mock = sys.modules["vosk"]
        VoskSpeechRecognizer(config)
        vosk_mock.SetLogLevel.assert_called_with(-1)

    def test_missing_vosk_raises_import_error(self, config):
        """Temporarily remove vosk from sys.modules to simulate missing install."""
        real_vosk = sys.modules.pop("vosk", None)
        # Also clear the module-level cache
        import autochair.audio.speech.vosk_recognizer as mod
        original = mod._vosk_module
        mod._vosk_module = None
        try:
            with pytest.raises(ImportError, match="vosk is required"):
                VoskSpeechRecognizer(config)
        finally:
            # Restore everything
            mod._vosk_module = original
            if real_vosk is not None:
                sys.modules["vosk"] = real_vosk


# ---------------------------------------------------------------------------
# recognize() — result parsing
# ---------------------------------------------------------------------------

class TestVoskRecognizeResults:
    def _set_result(self, text: str) -> None:
        """Configure mock to return AcceptWaveform=True with given text."""
        rec = _get_mock_rec()
        rec.AcceptWaveform.return_value = True
        rec.Result.return_value = json.dumps({"text": text})

    def test_recognize_returns_recognized_text(self, recognizer):
        self._set_result("stop")
        assert recognizer.recognize(b"\x00\x01") == "stop"

    def test_recognize_returns_start(self, recognizer):
        self._set_result("start")
        assert recognizer.recognize(b"\x00\x01") == "start"

    def test_recognize_returns_move_forward(self, recognizer):
        self._set_result("move forward")
        assert recognizer.recognize(b"\x00\x01") == "move forward"

    def test_recognize_unk_returns_empty_string(self, recognizer):
        """[unk] must become "" so VoiceCommandParser treats it as UNKNOWN."""
        self._set_result("[unk]")
        assert recognizer.recognize(b"\x00\x01") == ""

    def test_recognize_empty_text_returns_empty_string(self, recognizer):
        self._set_result("")
        assert recognizer.recognize(b"\x00\x01") == ""

    def test_recognize_empty_audio_returns_empty_string(self, recognizer):
        """Empty audio bytes must not cause an error."""
        assert recognizer.recognize(b"") == ""

    def test_recognize_partial_result_when_waveform_not_complete(self, recognizer):
        """When AcceptWaveform=False, returns partial text."""
        rec = _get_mock_rec()
        rec.AcceptWaveform.return_value = False
        rec.PartialResult.return_value = json.dumps({"partial": "stop"})
        result = recognizer.recognize(b"\x00\x01")
        # Partial "stop" should be returned
        assert result == "stop"

    def test_recognize_empty_partial_returns_empty(self, recognizer):
        rec = _get_mock_rec()
        rec.AcceptWaveform.return_value = False
        rec.PartialResult.return_value = json.dumps({"partial": ""})
        assert recognizer.recognize(b"\x00\x01") == ""

    def test_invalid_json_result_returns_empty(self, recognizer):
        rec = _get_mock_rec()
        rec.AcceptWaveform.return_value = True
        rec.Result.return_value = "not valid json {"
        assert recognizer.recognize(b"\x00\x01") == ""

    def test_accept_waveform_exception_returns_empty(self, recognizer):
        rec = _get_mock_rec()
        rec.AcceptWaveform.side_effect = RuntimeError("audio error")
        result = recognizer.recognize(b"\x00\x01")
        assert result == ""
        rec.AcceptWaveform.side_effect = None  # reset


# ---------------------------------------------------------------------------
# finalize()
# ---------------------------------------------------------------------------

class TestVoskFinalize:
    def test_finalize_returns_final_text(self, recognizer):
        rec = _get_mock_rec()
        rec.FinalResult.return_value = json.dumps({"text": "stop"})
        assert recognizer.finalize() == "stop"

    def test_finalize_returns_empty_for_unk(self, recognizer):
        rec = _get_mock_rec()
        rec.FinalResult.return_value = json.dumps({"text": "[unk]"})
        assert recognizer.finalize() == ""

    def test_finalize_returns_empty_on_exception(self, recognizer):
        rec = _get_mock_rec()
        rec.FinalResult.side_effect = RuntimeError("error")
        result = recognizer.finalize()
        assert result == ""
        rec.FinalResult.side_effect = None


# ---------------------------------------------------------------------------
# reset()
# ---------------------------------------------------------------------------

class TestVoskReset:
    def test_reset_creates_new_kaldi_recognizer(self, config):
        vosk_mock = sys.modules["vosk"]
        rec = VoskSpeechRecognizer(config)
        initial_call_count = vosk_mock.KaldiRecognizer.call_count
        rec.reset()
        assert vosk_mock.KaldiRecognizer.call_count == initial_call_count + 1


# ---------------------------------------------------------------------------
# Integration: recognized text → VoiceCommandParser pipeline
# ---------------------------------------------------------------------------

class TestVoskToParserPipeline:
    """
    Verify that text returned by VoskSpeechRecognizer flows correctly
    through VoiceCommandParser into InputCommands.
    These tests do NOT require real audio hardware.
    """

    @pytest.fixture
    def parser(self):
        return VoiceCommandParser()

    @pytest.fixture
    def validator(self):
        return InputCommandValidator()

    def _configure_result(self, text: str) -> None:
        rec = _get_mock_rec()
        rec.AcceptWaveform.return_value = True
        rec.Result.return_value = json.dumps({"text": text})

    def test_stop_recognition_produces_stop_input_command(
        self, recognizer, parser
    ):
        self._configure_result("stop")
        text = recognizer.recognize(b"\x00\x01")
        result = parser.parse(text)
        assert result.intent == VoiceIntent.STOP
        assert result.input_command is not None
        assert result.input_command.command == "STOP"

    def test_move_forward_recognition_produces_forward_input_command(
        self, recognizer, parser
    ):
        self._configure_result("move forward")
        text = recognizer.recognize(b"\x00\x01")
        result = parser.parse(text)
        assert result.intent == VoiceIntent.MOVE_FORWARD
        assert result.input_command is not None
        assert result.input_command.command == "FORWARD"

    def test_start_recognition_produces_start_intent_no_input_command(
        self, recognizer, parser
    ):
        """START must NOT produce an InputCommand — safety boundary."""
        self._configure_result("start")
        text = recognizer.recognize(b"\x00\x01")
        result = parser.parse(text)
        assert result.intent == VoiceIntent.START
        assert result.input_command is None  # No motion command generated

    def test_unk_produces_unknown_intent_no_input_command(
        self, recognizer, parser
    ):
        """[unk] → empty string → UNKNOWN → no InputCommand."""
        self._configure_result("[unk]")
        text = recognizer.recognize(b"\x00\x01")  # returns ""
        result = parser.parse(text)
        assert result.intent == VoiceIntent.UNKNOWN
        assert result.input_command is None

    def test_voice_source_is_always_voice(self, recognizer, parser):
        self._configure_result("stop")
        text = recognizer.recognize(b"\x00\x01")
        result = parser.parse(text)
        assert result.input_command is not None
        assert result.input_command.source == InputSource.VOICE

    def test_forward_command_passes_existing_validator(
        self, recognizer, parser, validator
    ):
        """FORWARD InputCommand must pass the existing InputCommandValidator."""
        self._configure_result("move forward")
        text = recognizer.recognize(b"\x00\x01")
        result = parser.parse(text)
        assert result.input_command is not None
        # Should not raise
        validator.validate(result.input_command)

    def test_stop_command_passes_existing_validator(
        self, recognizer, parser, validator
    ):
        self._configure_result("stop")
        text = recognizer.recognize(b"\x00\x01")
        result = parser.parse(text)
        assert result.input_command is not None
        validator.validate(result.input_command)

    def test_unknown_speech_does_not_generate_motion_command(
        self, recognizer, parser
    ):
        """Random speech not in grammar must never produce a movement command."""
        self._configure_result("[unk]")
        text = recognizer.recognize(b"\x00\x01")
        result = parser.parse(text)
        # No InputCommand means no MotionCommand can be generated
        assert result.input_command is None

    def test_start_intent_does_not_bypass_safety(self, recognizer, parser):
        """
        START recognized → START intent → input_command is None.
        The existing safety layer is never bypassed because there is no
        InputCommand to pass to it.
        """
        self._configure_result("start")
        text = recognizer.recognize(b"\x00\x01")
        result = parser.parse(text)
        assert result.intent == VoiceIntent.START
        # No InputCommand → no route to SafetyManager → no MotionCommand
        assert result.input_command is None
