"""
Tests for SimulatedMicrophoneManager.

Verifies the interface contract without any hardware access.
No ALSA, I2S, PortAudio, GPIO, or audio hardware is accessed.
"""

from autochair.audio.devices.microphone import (
    MicrophoneManager,
    SimulatedMicrophoneManager,
)


def test_simulated_microphone_implements_interface():
    """SimulatedMicrophoneManager must satisfy the MicrophoneManager interface."""
    mic = SimulatedMicrophoneManager()
    assert isinstance(mic, MicrophoneManager)


def test_simulated_microphone_starts_not_listening():
    mic = SimulatedMicrophoneManager()
    assert mic.is_listening() is False


def test_simulated_microphone_start_listening():
    mic = SimulatedMicrophoneManager()
    mic.start_listening()
    assert mic.is_listening() is True


def test_simulated_microphone_stop_listening():
    mic = SimulatedMicrophoneManager()
    mic.start_listening()
    mic.stop_listening()
    assert mic.is_listening() is False


def test_simulated_microphone_capture_returns_bytes():
    """capture_audio() must return bytes (empty in simulation)."""
    mic = SimulatedMicrophoneManager()
    mic.start_listening()
    data = mic.capture_audio()
    assert isinstance(data, bytes)


def test_simulated_microphone_capture_returns_empty_bytes():
    """Simulated capture must return empty bytes — no audio is generated."""
    mic = SimulatedMicrophoneManager()
    mic.start_listening()
    data = mic.capture_audio()
    assert data == b""
