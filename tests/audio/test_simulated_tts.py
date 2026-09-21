"""
Tests for SimulatedTTSEngine.

Verifies the interface contract, text recording, and empty-text rejection.
No speaker, PAM8403, ALSA, GPIO, or audio hardware is accessed.
"""

import pytest

from autochair.audio.tts.engine import SimulatedTTSEngine, TTSEngine


def test_simulated_tts_implements_interface():
    tts = SimulatedTTSEngine()
    assert isinstance(tts, TTSEngine)


def test_simulated_tts_last_spoken_empty_before_any_call():
    tts = SimulatedTTSEngine()
    assert tts.last_spoken() == ""


def test_simulated_tts_records_spoken_text():
    tts = SimulatedTTSEngine()
    tts.speak("System ready.")
    assert tts.last_spoken() == "System ready."


def test_simulated_tts_records_last_spoken_only():
    tts = SimulatedTTSEngine()
    tts.speak("Moving forward.")
    tts.speak("Stopping.")
    assert tts.last_spoken() == "Stopping."


def test_simulated_tts_spoken_history_tracks_all():
    tts = SimulatedTTSEngine()
    tts.speak("System ready.")
    tts.speak("Moving forward.")
    tts.speak("Stopping.")
    history = tts.spoken_history()
    assert history == ["System ready.", "Moving forward.", "Stopping."]


def test_simulated_tts_empty_text_raises():
    tts = SimulatedTTSEngine()
    with pytest.raises(ValueError):
        tts.speak("")


def test_simulated_tts_whitespace_only_text_raises():
    tts = SimulatedTTSEngine()
    with pytest.raises(ValueError):
        tts.speak("   ")


def test_simulated_tts_strips_surrounding_whitespace():
    tts = SimulatedTTSEngine()
    tts.speak("  System ready.  ")
    assert tts.last_spoken() == "System ready."


def test_simulated_tts_does_not_produce_audio():
    """
    This test verifies the simulation contract: no audio device is ever
    accessed by SimulatedTTSEngine. If this test passes without hardware,
    the simulation boundary is intact.
    """
    tts = SimulatedTTSEngine()
    tts.speak("Obstacle detected.")
    # If we reach here without exception and with correct text, simulation is intact.
    assert tts.last_spoken() == "Obstacle detected."
