"""
AudioManager — orchestrates the AutoChair audio/voice subsystem.

Manages the full voice input lifecycle:

    IDLE
      → start_listening()
    LISTENING
      → process()  (microphone capture → speech recognition)
    PROCESSING
      → parse result
    COMMAND_RECOGNIZED  (motion intent) or ERROR (unknown/rejected)
      → TTS feedback via TTSEngine
    SPEAKING
      → reset
    IDLE

For system intents (START), the AudioManager logs the event and returns
to IDLE without producing any motion command.

Integration with existing AutoChair pipeline:
    When a motion ParseResult is produced, the AudioManager passes the
    InputCommand through:
        InputCommandValidator  (existing, unmodified)
        CommandMapper          (existing, unmodified)
        SafetyManager          (existing, unmodified)
    Motion execution is simulated (logged) in this phase.

Hardware status: SIMULATED
All sub-modules use simulated implementations. No real microphone,
speaker, TTS engine, or buzzer hardware is accessed.
"""

from autochair.audio.alerts.manager import (
    AlertEvent,
    AlertManager,
    AlertType,
    SimulatedAlertManager,
)
from autochair.audio.commands.parser import ParseResult, VoiceCommandParser, VoiceIntent
from autochair.audio.devices.microphone import (
    MicrophoneManager,
    SimulatedMicrophoneManager,
)
from autochair.audio.speech.recognizer import (
    SimulatedSpeechRecognizer,
    SpeechRecognizer,
)
from autochair.audio.state import AudioState
from autochair.audio.tts.engine import SimulatedTTSEngine, TTSEngine
from autochair.audio.voice_session import RealVoiceSession
from autochair.core.command_mapper import CommandMapper
from autochair.input.validator import InputCommandValidator
from autochair.safety.manager import SafetyManager
from autochair.sensors.data import SensorSnapshot, UltrasonicData
from autochair.utils.logger import get_logger


logger = get_logger("autochair.audio.manager")


class AudioManager:
    """
    Central coordinator for the AutoChair audio/voice subsystem.

    Accepts concrete implementations of all sub-module interfaces so
    that simulated versions can be used in tests and real hardware
    drivers / RealVoiceSession can be injected when operating with real
    I2S microphone hardware on Raspberry Pi.

    The AudioManager always passes voice-generated InputCommands through
    the existing input validation, command mapping, and safety layers.
    It never produces MotionCommands directly.
    """

    def __init__(
        self,
        microphone: MicrophoneManager | None = None,
        recognizer: SpeechRecognizer | None = None,
        tts: TTSEngine | None = None,
        alerts: AlertManager | None = None,
        parser: VoiceCommandParser | None = None,
        validator: InputCommandValidator | None = None,
        mapper: CommandMapper | None = None,
        safety: SafetyManager | None = None,
        voice_session: RealVoiceSession | None = None,
    ) -> None:
        """
        Args:
            microphone:    Microphone capture implementation (used in simulated mode).
            recognizer:    Speech recognition implementation (used in simulated mode).
            tts:           Text-to-speech output implementation.
            alerts:        Alert routing implementation.
            parser:        VoiceCommandParser instance (created if not provided).
            validator:     InputCommandValidator (created if not provided).
            mapper:        CommandMapper (created if not provided).
            safety:        SafetyManager (created if not provided).
            voice_session: Optional RealVoiceSession instance for real microphone streaming.
        """
        self._microphone = microphone or SimulatedMicrophoneManager()
        self._recognizer = recognizer or SimulatedSpeechRecognizer()
        self._tts = tts or SimulatedTTSEngine()
        self._alerts = alerts or SimulatedAlertManager()
        self._parser = parser or VoiceCommandParser()
        self._validator = validator or InputCommandValidator()
        self._mapper = mapper or CommandMapper()
        self._safety = safety or SafetyManager()
        self._voice_session = voice_session

        self._state: AudioState = AudioState.IDLE
        self._last_parse_result: ParseResult | None = None

        mode_str = "REAL_HARDWARE" if voice_session is not None else "SIMULATED"
        logger.info(f"AUDIO_READY — AudioManager initialised ({mode_str})")

    # ------------------------------------------------------------------
    # State access
    # ------------------------------------------------------------------

    @property
    def state(self) -> AudioState:
        """Current lifecycle state of the audio subsystem."""
        return self._state

    @property
    def is_real_voice_mode(self) -> bool:
        """Return True if configured with a RealVoiceSession."""
        return self._voice_session is not None

    @property
    def voice_session(self) -> RealVoiceSession | None:
        """Return the configured RealVoiceSession instance, if any."""
        return self._voice_session

    def _set_state(self, new_state: AudioState) -> None:
        logger.info(f"[STATE] {self._state.value} → {new_state.value}")
        self._state = new_state

    # ------------------------------------------------------------------
    # Lifecycle
    # ------------------------------------------------------------------

    def start_listening(self) -> None:
        """
        Transition from IDLE to LISTENING and activate the microphone (if simulated).

        Raises:
            RuntimeError: If called when not in IDLE state.
        """
        if self._state != AudioState.IDLE:
            raise RuntimeError(
                f"start_listening() called in unexpected state: {self._state.value}. "
                f"Expected IDLE."
            )
        if self._voice_session is None:
            self._microphone.start_listening()
        self._set_state(AudioState.LISTENING)

    def process(self, sensor_snapshot: SensorSnapshot | None = None) -> ParseResult:
        """
        Capture audio, recognize speech, parse the command, and handle the result.

        This method drives the full single-utterance cycle:
          LISTENING → PROCESSING → COMMAND_RECOGNIZED / ERROR → SPEAKING → IDLE

        In real voice mode (voice_session configured), audio capture,
        preprocessing, and streaming recognition are handled by RealVoiceSession.
        In simulated mode, MicrophoneManager and SpeechRecognizer are used.

        Args:
            sensor_snapshot: Current sensor state for safety evaluation.
                             If not provided, a clear (no-obstacle) snapshot
                             is used. This simulates a safe environment.

        Returns:
            ParseResult representing what was recognized and (if applicable)
            the InputCommand that was validated against the existing pipeline.
        """
        if self._state != AudioState.LISTENING:
            raise RuntimeError(
                f"process() called in unexpected state: {self._state.value}. "
                f"Expected LISTENING."
            )

        self._set_state(AudioState.PROCESSING)

        # --- Capture & Recognize ---
        try:
            if self._voice_session is not None:
                recognized_text = self._voice_session.start()
            else:
                audio_data = self._microphone.capture_audio()
                recognized_text = self._recognizer.recognize(audio_data)
        except Exception as exc:
            logger.error(f"AUDIO_PROCESSING_ERROR: {exc}")
            self._alerts.raise_alert(AlertEvent(
                alert_type=AlertType.COMMAND_REJECTED,
                message=f"Audio processing error: {exc}",
            ))
            self._set_state(AudioState.ERROR)
            self._speak_and_return_idle("Command not recognized.")
            if self._microphone.is_listening():
                try:
                    self._microphone.stop_listening()
                except Exception as mic_exc:
                    logger.warning(f"Failed to stop microphone during error cleanup: {mic_exc}")
            result = self._parser.parse("")
            self._last_parse_result = result
            return result
        finally:
            if self._microphone.is_listening():
                try:
                    self._microphone.stop_listening()
                except Exception as mic_exc:
                    logger.warning(f"Failed to stop microphone: {mic_exc}")

        logger.info(f"SPEECH_RECEIVED: '{recognized_text}'")

        # --- Parse ---
        result = self._parser.parse(recognized_text)
        self._last_parse_result = result

        # --- Route result ---
        if result.input_command is not None:
            self._handle_motion_intent(result, sensor_snapshot)

        elif result.intent == VoiceIntent.START:
            self._handle_start_intent(result)

        else:
            self._handle_unknown_intent(result)

        return result

    def _handle_motion_intent(
        self,
        result: ParseResult,
        sensor_snapshot: SensorSnapshot | None,
    ) -> None:
        """
        Validate and map a motion intent through the existing safety pipeline.

        The InputCommand flows:
          InputCommandValidator → CommandMapper → SafetyManager → (simulated motion)

        Voice NEVER produces a MotionCommand without passing through this chain.
        """
        assert result.input_command is not None

        # Validate through existing InputCommandValidator (unchanged)
        try:
            self._validator.validate(result.input_command)
        except ValueError as exc:
            logger.error(f"COMMAND_REJECTED (validation failed): {exc}")
            self._alerts.raise_alert(AlertEvent(
                alert_type=AlertType.COMMAND_REJECTED,
                message=f"Command validation failed: {exc}",
            ))
            self._set_state(AudioState.ERROR)
            self._speak_and_return_idle("Command not recognized.")
            return

        # Map through existing CommandMapper (unchanged)
        motion_command = self._mapper.map(result.input_command)

        # Evaluate through existing SafetyManager (unchanged)
        snapshot = sensor_snapshot or SensorSnapshot(ultrasonic=UltrasonicData())
        safety_status = self._safety.evaluate(snapshot)

        if safety_status.requires_stop():
            logger.warning("COMMAND_REJECTED (safety requires stop)")
            self._alerts.raise_alert(AlertEvent(
                alert_type=AlertType.OBSTACLE,
                message="Safety layer requires stop — command suppressed.",
            ))
            self._set_state(AudioState.ERROR)
            self._speak_and_return_idle("Obstacle detected. Stopping.")
            return

        # Simulated motion execution — log only, no hardware
        logger.info(
            f"COMMAND_RECOGNIZED: {result.input_command.command} "
            f"→ MotionCommand({motion_command}) [SIMULATED — no hardware]"
        )
        self._set_state(AudioState.COMMAND_RECOGNIZED)
        self._alerts.raise_alert(AlertEvent(
            alert_type=AlertType.COMMAND_CONFIRMED,
            message=f"Command accepted: {result.input_command.command}",
        ))

        # TTS confirmation feedback
        feedback_text = self._command_feedback(result.input_command.command)
        self._speak_and_return_idle(feedback_text)

    def _handle_start_intent(self, result: ParseResult) -> None:
        """
        Handle the START system intent.

        START is recognized but does not produce a motion command.
        See audio/commands/parser.py module docstring for full rationale.
        """
        logger.info(
            "COMMAND_RECOGNIZED: START (system intent — no motion command produced). "
            "START is not yet mapped to a system-level action. "
            "This is a documented design gap pending Phase 2 architecture."
        )
        self._set_state(AudioState.COMMAND_RECOGNIZED)
        self._alerts.raise_alert(AlertEvent(
            alert_type=AlertType.COMMAND_CONFIRMED,
            message="START intent recognized (system command — not yet implemented).",
        ))
        self._speak_and_return_idle("Starting.")

    def _handle_unknown_intent(self, result: ParseResult) -> None:
        """Handle an unrecognized utterance safely."""
        logger.warning(f"COMMAND_REJECTED: '{result.raw_text}' — not recognized")
        self._alerts.raise_alert(AlertEvent(
            alert_type=AlertType.COMMAND_REJECTED,
            message=f"Unknown voice command: '{result.raw_text}'",
        ))
        self._set_state(AudioState.ERROR)
        self._speak_and_return_idle("Command not recognized.")

    def _speak_and_return_idle(self, text: str) -> None:
        """Deliver TTS feedback and return to IDLE."""
        self._set_state(AudioState.SPEAKING)
        self._tts.speak(text)
        logger.info(f"SPEAKING: '{text}'")
        self._set_state(AudioState.IDLE)

    def _command_feedback(self, command: str) -> str:
        """Return a human-friendly TTS string for a given command string."""
        _feedback: dict[str, str] = {
            "FORWARD": "Moving forward.",
            "REVERSE": "Moving in reverse.",
            "LEFT": "Turning left.",
            "RIGHT": "Turning right.",
            "STOP": "Stopping.",
        }
        return _feedback.get(command, "Command accepted.")

    def reset(self) -> None:
        """
        Force the AudioManager back to IDLE and stop the microphone.

        Can be called from any state to recover from an error condition.
        """
        if self._microphone.is_listening():
            self._microphone.stop_listening()
        self._set_state(AudioState.IDLE)
        logger.info("AudioManager reset to IDLE")

    @property
    def last_parse_result(self) -> ParseResult | None:
        """Return the ParseResult from the most recent process() call."""
        return self._last_parse_result
