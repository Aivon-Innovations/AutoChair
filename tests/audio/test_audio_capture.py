"""
Tests for AudioCapture — SimulatedAudioCapture.

Verifies the simulation contract:
  - open/close lifecycle
  - read_chunk returns configured bytes
  - cycling through multi-chunk sequences
  - read_count tracking
  - context-manager protocol
  - empty chunk handling
  - ALSAAudioCapture ImportError on missing pyalsaaudio (guarded)

No ALSA, audio hardware, or pyalsaaudio is required.
"""

from __future__ import annotations

import pytest

from autochair.audio.config import AudioConfig
from autochair.audio.devices.capture import AudioCapture, SimulatedAudioCapture


# ---------------------------------------------------------------------------
# SimulatedAudioCapture — lifecycle
# ---------------------------------------------------------------------------

class TestSimulatedAudioCaptureLifecycle:
    def test_initially_closed(self):
        cap = SimulatedAudioCapture()
        assert not cap.is_open()

    def test_open_sets_open_flag(self):
        cap = SimulatedAudioCapture()
        cap.open()
        assert cap.is_open()

    def test_close_clears_open_flag(self):
        cap = SimulatedAudioCapture()
        cap.open()
        cap.close()
        assert not cap.is_open()

    def test_context_manager_opens_on_enter(self):
        cap = SimulatedAudioCapture()
        with cap:
            assert cap.is_open()

    def test_context_manager_closes_on_exit(self):
        cap = SimulatedAudioCapture()
        with cap:
            pass
        assert not cap.is_open()

    def test_context_manager_closes_on_exception(self):
        cap = SimulatedAudioCapture()
        try:
            with cap:
                raise ValueError("boom")
        except ValueError:
            pass
        assert not cap.is_open()


# ---------------------------------------------------------------------------
# SimulatedAudioCapture — chunk behaviour
# ---------------------------------------------------------------------------

class TestSimulatedAudioCaptureChunks:
    def test_default_chunk_is_empty_bytes(self):
        cap = SimulatedAudioCapture()
        assert cap.read_chunk() == b""

    def test_single_configured_chunk_returned(self):
        chunk = b"\x01\x02\x03\x04"
        cap = SimulatedAudioCapture(chunks=[chunk])
        assert cap.read_chunk() == chunk

    def test_chunks_cycle_when_exhausted(self):
        chunks = [b"\xAA", b"\xBB", b"\xCC"]
        cap = SimulatedAudioCapture(chunks=chunks)
        assert cap.read_chunk() == b"\xAA"
        assert cap.read_chunk() == b"\xBB"
        assert cap.read_chunk() == b"\xCC"
        assert cap.read_chunk() == b"\xAA"  # back to start

    def test_empty_chunk_list_returns_empty_bytes(self):
        cap = SimulatedAudioCapture(chunks=[])
        assert cap.read_chunk() == b""

    def test_read_count_increments(self):
        cap = SimulatedAudioCapture(chunks=[b"\x01"])
        cap.open()
        assert cap.read_count == 0
        cap.read_chunk()
        assert cap.read_count == 1
        cap.read_chunk()
        assert cap.read_count == 2

    def test_read_count_resets_on_open(self):
        cap = SimulatedAudioCapture(chunks=[b"\x01"])
        cap.read_chunk()
        cap.read_chunk()
        cap.open()
        assert cap.read_count == 0

    def test_set_chunks_replaces_sequence(self):
        cap = SimulatedAudioCapture(chunks=[b"\x01"])
        assert cap.read_chunk() == b"\x01"
        cap.set_chunks([b"\xFF"])
        assert cap.read_chunk() == b"\xFF"


# ---------------------------------------------------------------------------
# SimulatedAudioCapture — interface compliance
# ---------------------------------------------------------------------------

class TestSimulatedAudioCaptureInterface:
    def test_is_subclass_of_audio_capture(self):
        assert issubclass(SimulatedAudioCapture, AudioCapture)

    def test_accepts_audio_config(self):
        cfg = AudioConfig()
        cap = SimulatedAudioCapture(config=cfg)
        assert cap is not None

    def test_read_chunk_returns_bytes(self):
        cap = SimulatedAudioCapture(chunks=[b"\x00\x01"])
        result = cap.read_chunk()
        assert isinstance(result, bytes)


# ---------------------------------------------------------------------------
# ALSAAudioCapture — ImportError guard (no real hardware needed)
# ---------------------------------------------------------------------------

class TestALSAAudioCaptureImportGuard:
    def test_missing_pyalsaaudio_raises_import_error(self, monkeypatch):
        """
        Simulate a missing pyalsaaudio by removing it from sys.modules
        and ensuring builtins.__import__ raises ImportError for 'alsaaudio'.
        """
        import sys
        import builtins

        original_import = builtins.__import__

        def blocking_import(name, *args, **kwargs):
            if name == "alsaaudio":
                raise ImportError("No module named 'alsaaudio'")
            return original_import(name, *args, **kwargs)

        monkeypatch.setattr(builtins, "__import__", blocking_import)
        # Remove cached module if present
        sys.modules.pop("alsaaudio", None)

        from autochair.audio.devices.capture import ALSAAudioCapture
        with pytest.raises(ImportError, match="pyalsaaudio"):
            ALSAAudioCapture(AudioConfig())
