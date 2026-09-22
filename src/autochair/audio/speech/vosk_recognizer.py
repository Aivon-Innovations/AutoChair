"""
VoskSpeechRecognizer — real speech recognition using the Vosk offline engine.

Implements the existing SpeechRecognizer ABC using Vosk's KaldiRecognizer
with a restricted grammar.

Hardware status: PI_PENDING — not yet deployed/tested in this repository
Requires:
  - vosk >= 0.3.45  (install: pip install vosk)
  - A downloaded Vosk model directory
    Verified Pi model: vosk-model-small-en-us-0.15

IMPORTANT:
  vosk is an optional dependency.  This module can be imported on Mac
  without vosk installed; however, instantiating VoskSpeechRecognizer
  will raise ImportError if vosk is not available.

  Use SimulatedSpeechRecognizer (from recognizer.py) for all tests and
  Mac development.  No test should instantiate VoskSpeechRecognizer
  without providing a mock vosk via sys.modules.

Architecture position:

  AudioCapture.read_chunk()
      → AudioPreprocessor.process()    (S32_LE stereo → S16_LE mono 16 kHz)
      → VoskSpeechRecognizer.recognize()  (bytes → recognized text)
      → VoiceCommandParser.parse()     (text → ParseResult/InputCommand)
      → existing validation/safety pipeline (unchanged)

Vosk grammar (restricted)
-------------------------
Vosk's KaldiRecognizer accepts a JSON array of strings that define the
ONLY output phrases it will ever return.  Any other utterance maps to
"[unk]".  This is the safety mechanism that prevents arbitrary speech
from being treated as a command.

Initial Phase 3 grammar (verified on Pi):
  ["start", "stop", "move forward", "[unk]"]

Do NOT add movement commands without corresponding updates to
VoiceCommandParser and InputCommandValidator.

Stateful streaming
------------------
Vosk's KaldiRecognizer accumulates context across calls.  A single
instance is created at __init__() and reused across recognize() calls.
This is correct for streaming audio from a single continuous source.

Call reset() to clear accumulated state between utterances if needed.
"""

from __future__ import annotations

import json

from autochair.audio.config import AudioConfig
from autochair.audio.speech.recognizer import SpeechRecognizer
from autochair.utils.logger import get_logger


logger = get_logger("autochair.audio.speech.vosk_recognizer")

# Sentinel — set to the imported vosk module at first instantiation,
# or left as None if vosk is unavailable.
_vosk_module: object | None = None


def _require_vosk() -> object:
    """
    Lazily import vosk and return the module.

    Raises:
        ImportError: If vosk is not installed in the current environment.
    """
    global _vosk_module
    if _vosk_module is None:
        try:
            import vosk  # type: ignore[import-not-found]
            _vosk_module = vosk
        except ImportError as exc:
            raise ImportError(
                "vosk is required for VoskSpeechRecognizer.\n"
                "Install on Raspberry Pi with:\n"
                "  pip install vosk\n"
                "Download a model from https://alphacephei.com/vosk/models\n"
                f"Original error: {exc}"
            ) from exc
    return _vosk_module


class VoskSpeechRecognizer(SpeechRecognizer):
    """
    Offline speech recognizer using Vosk (KaldiRecognizer).

    Implements SpeechRecognizer.recognize(bytes) → str, returning
    only phrases from the configured grammar or an empty string when
    no complete utterance is ready.

    The recognizer is stateful — it accumulates audio context across
    calls to recognize().  Call reset() between distinct utterances if
    you need a clean slate.

    Grammar safety
    --------------
    Only phrases in AudioConfig.vosk_grammar are ever returned.
    "[unk]" is always included (enforced by AudioConfig.__post_init__).
    The VoiceCommandParser treats unrecognized text (including "[unk]")
    as VoiceIntent.UNKNOWN → no motion command is generated.

    Model loading
    -------------
    The Vosk model is loaded from config.vosk_model_path at __init__().
    Loading a model may take 0.5–3 s on the Raspberry Pi 4.
    Plan for this at application startup, not on the first audio frame.

    Hardware status: PI_PENDING — experimentally validated in pre-repository script;
    not yet deployed/tested via this codebase on Pi.
      model: vosk-model-small-en-us-0.15
      sample_rate: 16000 Hz
      grammar: ["start", "stop", "move forward", "[unk]"]
    Full repository integration not yet deployed/tested on Pi.
    """

    def __init__(self, config: AudioConfig) -> None:
        """
        Load the Vosk model and initialise the KaldiRecognizer.

        Args:
            config: AudioConfig supplying vosk_model_path, vosk_sample_rate,
                    and vosk_grammar.

        Raises:
            ImportError:  If vosk is not installed.
            RuntimeError: If the model path is empty or the model cannot
                          be loaded.
        """
        vosk = _require_vosk()

        # Suppress Vosk's verbose C++ output if requested
        if config.vosk_suppress_logs:
            vosk.SetLogLevel(-1)  # type: ignore[union-attr]

        if not config.vosk_model_path:
            raise RuntimeError(
                "AudioConfig.vosk_model_path is empty. "
                "Provide the path to a downloaded Vosk model directory, e.g.:\n"
                "  AudioConfig(vosk_model_path='vosk-model-small-en-us-0.15')"
            )

        logger.info(
            f"Loading Vosk model from '{config.vosk_model_path}' "
            f"(this may take 1–3 s on Raspberry Pi 4) …"
        )
        try:
            self._model = vosk.Model(str(config.vosk_model_path))  # type: ignore[union-attr]
        except Exception as exc:
            raise RuntimeError(
                f"Failed to load Vosk model from '{config.vosk_model_path}': {exc}\n"
                "Ensure the path points to a valid extracted Vosk model directory."
            ) from exc

        # Build restricted grammar JSON
        grammar_json = json.dumps(config.vosk_grammar)
        logger.info(f"Vosk grammar: {grammar_json}")

        self._rec = vosk.KaldiRecognizer(  # type: ignore[union-attr]
            self._model,
            float(config.vosk_sample_rate),
            grammar_json,
        )
        self._vosk = vosk   # keep reference for reset()
        self._sample_rate = config.vosk_sample_rate
        self._grammar = list(config.vosk_grammar)
        self._grammar_json = grammar_json
        logger.info(
            f"VoskSpeechRecognizer ready "
            f"(model='{config.vosk_model_path}', "
            f"rate={config.vosk_sample_rate} Hz, "
            f"grammar={config.vosk_grammar})"
        )

    # ------------------------------------------------------------------
    # SpeechRecognizer interface
    # ------------------------------------------------------------------

    def recognize(self, audio_data: bytes) -> str:
        """
        Feed audio bytes to the Vosk recognizer and return any result.

        The recognizer accumulates context across calls.  A non-empty
        string is returned only when Vosk considers an utterance complete
        (AcceptWaveform returns True).

        Args:
            audio_data: Mono S16_LE PCM bytes at vosk_sample_rate.
                        Produced by AudioPreprocessor.process().
                        Empty bytes are accepted (returns "").

        Returns:
            str: The recognized utterance, lowercased and stripped.
                 Returns "" if the recognizer has not yet completed an
                 utterance or if "[unk]" was received (unrecognized speech).
                 Returns "" on empty audio_data (no error raised).

        Note:
            "[unk]" from Vosk is treated as no-command: it is returned
            as "" from recognize() so that VoiceCommandParser receives
            empty input and produces VoiceIntent.UNKNOWN safely.
        """
        if not audio_data:
            logger.debug("VoskSpeechRecognizer.recognize(): empty audio — returning \"\"")
            return ""

        try:
            completed = self._rec.AcceptWaveform(audio_data)
        except Exception as exc:
            logger.error(f"Vosk AcceptWaveform error: {exc}")
            return ""

        if not completed:
            # Utterance not yet complete — return partial or empty
            return self._parse_partial()

        return self._parse_result()

    def finalize(self) -> str:
        """
        Flush any buffered audio and return the final recognition result.

        Should be called when the audio stream ends to retrieve any
        remaining utterance that has not yet been returned by recognize().

        Returns:
            str: The final recognized utterance, or "" if none.
        """
        try:
            raw = self._rec.FinalResult()
        except Exception as exc:
            logger.error(f"Vosk FinalResult error: {exc}")
            return ""
        return self._extract_text(raw)

    def reset(self) -> None:
        """
        Reset the recognizer state, discarding accumulated audio context.

        Use between distinct utterances or after an error to start fresh.
        """
        # Re-create the recognizer with the same model (Vosk has no Reset()).
        # Use the stored vosk module reference so this works with both the real
        # library and with MagicMock during testing.
        try:
            self._rec = self._vosk.KaldiRecognizer(  # type: ignore[union-attr]
                self._model,
                float(self._sample_rate),
                self._grammar_json,
            )
            logger.info("VoskSpeechRecognizer reset")
        except Exception as exc:
            logger.error(f"Failed to reset VoskSpeechRecognizer: {exc}")

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------

    def _parse_result(self) -> str:
        """Parse a completed AcceptWaveform result."""
        try:
            raw = self._rec.Result()
        except Exception as exc:
            logger.error(f"Vosk Result() error: {exc}")
            return ""
        return self._extract_text(raw)

    def _parse_partial(self) -> str:
        """Parse a partial result (utterance not yet complete)."""
        try:
            raw = self._rec.PartialResult()
        except Exception as exc:
            logger.debug(f"Vosk PartialResult() error: {exc}")
            return ""
        return self._extract_text(raw)

    @staticmethod
    def _extract_text(raw_json: str) -> str:
        """
        Extract the 'text' field from a Vosk JSON result string.

        Args:
            raw_json: JSON string from Vosk Result(), PartialResult(), or
                      FinalResult().  Expected format:
                      {"text": "move forward"} or {"partial": "move"}

        Returns:
            str: The recognized text, lowercased and stripped.
                 Returns "" if the text is missing, empty, or "[unk]".
        """
        try:
            data = json.loads(raw_json)
        except (json.JSONDecodeError, TypeError):
            logger.debug(f"VoskSpeechRecognizer: could not parse JSON: {raw_json!r}")
            return ""

        # Prefer 'text' (completed result) over 'partial'
        text = data.get("text", data.get("partial", "")).strip().lower()

        if not text or text == "[unk]":
            if text == "[unk]":
                logger.info("Vosk: [unk] — speech not in grammar, returning \"\"")
            return ""

        logger.info(f"Vosk recognized: '{text}'")
        return text
