"""
Tests for AudioState enum.

Verifies that all required state values exist, that the enum is a str
subclass (allowing safe string comparisons), and that the full expected
lifecycle set is present.

No hardware required. No audio devices accessed.
"""

from autochair.audio.state import AudioState


def test_audio_state_idle_exists():
    assert AudioState.IDLE == "IDLE"


def test_audio_state_listening_exists():
    assert AudioState.LISTENING == "LISTENING"


def test_audio_state_processing_exists():
    assert AudioState.PROCESSING == "PROCESSING"


def test_audio_state_command_recognized_exists():
    assert AudioState.COMMAND_RECOGNIZED == "COMMAND_RECOGNIZED"


def test_audio_state_speaking_exists():
    assert AudioState.SPEAKING == "SPEAKING"


def test_audio_state_error_exists():
    assert AudioState.ERROR == "ERROR"


def test_audio_state_is_str_enum():
    """AudioState members must be string-comparable for external consumers (e.g. UI)."""
    assert isinstance(AudioState.IDLE, str)
    assert isinstance(AudioState.ERROR, str)


def test_audio_state_complete_lifecycle_set():
    """All required lifecycle states are present."""
    required = {
        "IDLE", "LISTENING", "PROCESSING",
        "COMMAND_RECOGNIZED", "SPEAKING", "ERROR",
    }
    actual = {state.value for state in AudioState}
    assert required == actual


def test_audio_state_values_are_unique():
    values = [state.value for state in AudioState]
    assert len(values) == len(set(values))
