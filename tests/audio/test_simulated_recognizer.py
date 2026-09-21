"""
Tests for SimulatedSpeechRecognizer.

Verifies the interface contract and injectable response behaviour.
No Vosk, Whisper, or any speech recognition library is accessed.
No audio hardware is accessed.
"""

from autochair.audio.speech.recognizer import (
    SimulatedSpeechRecognizer,
    SpeechRecognizer,
)


def test_simulated_recognizer_implements_interface():
    recognizer = SimulatedSpeechRecognizer()
    assert isinstance(recognizer, SpeechRecognizer)


def test_simulated_recognizer_default_response_is_empty():
    recognizer = SimulatedSpeechRecognizer()
    result = recognizer.recognize(b"")
    assert result == ""


def test_simulated_recognizer_returns_configured_response():
    recognizer = SimulatedSpeechRecognizer(response="move forward")
    result = recognizer.recognize(b"")
    assert result == "move forward"


def test_simulated_recognizer_normalizes_response_lowercase():
    recognizer = SimulatedSpeechRecognizer(response="MOVE FORWARD")
    result = recognizer.recognize(b"")
    assert result == "move forward"


def test_simulated_recognizer_strips_whitespace():
    recognizer = SimulatedSpeechRecognizer(response="  stop  ")
    result = recognizer.recognize(b"")
    assert result == "stop"


def test_simulated_recognizer_set_response():
    """set_response() allows changing response between recognize() calls."""
    recognizer = SimulatedSpeechRecognizer(response="forward")
    recognizer.set_response("stop")
    result = recognizer.recognize(b"")
    assert result == "stop"


def test_simulated_recognizer_ignores_audio_data_content():
    """The simulated recognizer must not process audio_data at all."""
    recognizer = SimulatedSpeechRecognizer(response="stop")
    # Pass arbitrary garbage bytes — result must be fixed response
    result = recognizer.recognize(b"\x00\xff\xab\xcd" * 100)
    assert result == "stop"


def test_simulated_recognizer_recognize_returns_str():
    recognizer = SimulatedSpeechRecognizer(response="halt")
    result = recognizer.recognize(b"")
    assert isinstance(result, str)
