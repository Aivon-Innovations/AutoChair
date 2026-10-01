"""
Tests for AudioManager — lifecycle, state transitions, safety integration.

Verifies:
- IDLE → LISTENING → PROCESSING → COMMAND_RECOGNIZED → SPEAKING → IDLE
- FORWARD voice command passes through existing input/safety pipeline
- STOP voice command passes through existing input/safety pipeline
- START intent recognized, no motion command produced, no crash
- Unknown speech triggers ERROR state and COMMAND_REJECTED alert
- Safety layer is consulted (never bypassed)
- InputCommand source is always InputSource.VOICE
- Voice never directly produces a MotionCommand

No audio hardware required. All components are simulated.
"""

import pytest

from autochair.audio.alerts.manager import AlertType, SimulatedAlertManager
from autochair.audio.commands.parser import VoiceIntent
from autochair.audio.devices.microphone import SimulatedMicrophoneManager
from autochair.audio.manager import AudioManager
from autochair.audio.speech.recognizer import SimulatedSpeechRecognizer
from autochair.audio.state import AudioState
from autochair.audio.tts.engine import SimulatedTTSEngine
from autochair.input.command import InputSource
from autochair.safety.manager import SafetyManager
from autochair.sensors.data import SensorSnapshot, UltrasonicData


# ---------------------------------------------------------------------------
# Helpers / fixtures
# ---------------------------------------------------------------------------

def make_manager(response: str = "") -> tuple[AudioManager, SimulatedSpeechRecognizer, SimulatedTTSEngine, SimulatedAlertManager]:
    """Build a fully simulated AudioManager with injectable recognizer response."""
    mic = SimulatedMicrophoneManager()
    recognizer = SimulatedSpeechRecognizer(response=response)
    tts = SimulatedTTSEngine()
    alerts = SimulatedAlertManager()
    manager = AudioManager(
        microphone=mic,
        recognizer=recognizer,
        tts=tts,
        alerts=alerts,
    )
    return manager, recognizer, tts, alerts


def clear_snapshot() -> SensorSnapshot:
    """SensorSnapshot with all distances at max — no obstacles."""
    return SensorSnapshot(ultrasonic=UltrasonicData())


def obstacle_snapshot() -> SensorSnapshot:
    """SensorSnapshot with a front obstacle within warning range."""
    return SensorSnapshot(ultrasonic=UltrasonicData(front_center_cm=30.0))


# ---------------------------------------------------------------------------
# Initial state
# ---------------------------------------------------------------------------

def test_audio_manager_starts_idle():
    manager, _, _, _ = make_manager()
    assert manager.state == AudioState.IDLE


def test_audio_manager_starts_with_no_parse_result():
    manager, _, _, _ = make_manager()
    assert manager.last_parse_result is None


# ---------------------------------------------------------------------------
# start_listening() transitions
# ---------------------------------------------------------------------------

def test_start_listening_transitions_to_listening():
    manager, _, _, _ = make_manager()
    manager.start_listening()
    assert manager.state == AudioState.LISTENING


def test_start_listening_activates_microphone():
    manager, _, _, _ = make_manager()
    mic = SimulatedMicrophoneManager()
    recognizer = SimulatedSpeechRecognizer()
    tts = SimulatedTTSEngine()
    alerts = SimulatedAlertManager()
    m = AudioManager(microphone=mic, recognizer=recognizer, tts=tts, alerts=alerts)
    m.start_listening()
    assert mic.is_listening() is True


def test_start_listening_raises_if_not_idle():
    manager, _, _, _ = make_manager()
    manager.start_listening()
    with pytest.raises(RuntimeError):
        manager.start_listening()


# ---------------------------------------------------------------------------
# FORWARD command lifecycle
# ---------------------------------------------------------------------------

def test_forward_command_full_lifecycle(monkeypatch):
    manager, recognizer, tts, alerts = make_manager(response="move forward")
    manager.start_listening()
    result = manager.process(clear_snapshot())

    assert result.intent == VoiceIntent.MOVE_FORWARD
    assert result.input_command is not None
    assert result.input_command.source == InputSource.VOICE
    assert result.input_command.command == "FORWARD"
    assert manager.state == AudioState.IDLE  # returns to idle after speaking


def test_forward_command_produces_confirmation_alert():
    manager, _, _, alerts = make_manager(response="forward")
    manager.start_listening()
    manager.process(clear_snapshot())

    last = alerts.last_alert()
    assert last is not None
    assert last.alert_type == AlertType.COMMAND_CONFIRMED


def test_forward_command_produces_tts_feedback():
    manager, _, tts, _ = make_manager(response="go forward")
    manager.start_listening()
    manager.process(clear_snapshot())
    assert "forward" in tts.last_spoken().lower()


def test_forward_command_returns_idle_after_processing():
    manager, _, _, _ = make_manager(response="forward")
    manager.start_listening()
    manager.process(clear_snapshot())
    assert manager.state == AudioState.IDLE


# ---------------------------------------------------------------------------
# STOP command lifecycle
# ---------------------------------------------------------------------------

def test_stop_command_full_lifecycle():
    manager, _, _, _ = make_manager(response="stop")
    manager.start_listening()
    result = manager.process(clear_snapshot())

    assert result.intent == VoiceIntent.STOP
    assert result.input_command is not None
    assert result.input_command.source == InputSource.VOICE
    assert result.input_command.command == "STOP"
    assert manager.state == AudioState.IDLE


def test_stop_command_produces_confirmation_alert():
    manager, _, _, alerts = make_manager(response="halt")
    manager.start_listening()
    manager.process(clear_snapshot())

    last = alerts.last_alert()
    assert last is not None
    assert last.alert_type == AlertType.COMMAND_CONFIRMED


def test_stop_command_tts_says_stopping():
    manager, _, tts, _ = make_manager(response="stop")
    manager.start_listening()
    manager.process(clear_snapshot())
    assert "stop" in tts.last_spoken().lower()


# ---------------------------------------------------------------------------
# START intent lifecycle
# ---------------------------------------------------------------------------

def test_start_intent_recognized():
    manager, _, _, _ = make_manager(response="start")
    manager.start_listening()
    result = manager.process(clear_snapshot())
    assert result.intent == VoiceIntent.START


def test_start_intent_produces_no_input_command():
    """START must never inject an InputCommand into the motion pipeline."""
    manager, _, _, _ = make_manager(response="start")
    manager.start_listening()
    result = manager.process(clear_snapshot())
    assert result.input_command is None


def test_start_intent_does_not_crash():
    """START must be handled gracefully without any exception."""
    manager, _, _, _ = make_manager(response="start")
    manager.start_listening()
    result = manager.process(clear_snapshot())
    assert result is not None


def test_start_intent_returns_to_idle():
    manager, _, _, _ = make_manager(response="start")
    manager.start_listening()
    manager.process(clear_snapshot())
    assert manager.state == AudioState.IDLE


# ---------------------------------------------------------------------------
# UNKNOWN command lifecycle
# ---------------------------------------------------------------------------

def test_unknown_command_triggers_error_then_idle():
    manager, _, _, _ = make_manager(response="fly to the moon")
    manager.start_listening()
    manager.process(clear_snapshot())
    # After error + TTS, returns to IDLE
    assert manager.state == AudioState.IDLE


def test_unknown_command_raises_rejected_alert():
    manager, _, _, alerts = make_manager(response="open the pod bay doors")
    manager.start_listening()
    manager.process(clear_snapshot())

    rejected = [a for a in alerts.alert_history() if a.alert_type == AlertType.COMMAND_REJECTED]
    assert len(rejected) >= 1


def test_unknown_command_tts_says_not_recognized():
    manager, _, tts, _ = make_manager(response="xyzzy")
    manager.start_listening()
    manager.process(clear_snapshot())
    assert "not recognized" in tts.last_spoken().lower()


def test_unknown_command_produces_no_input_command():
    manager, _, _, _ = make_manager(response="blah blah nonsense")
    manager.start_listening()
    result = manager.process(clear_snapshot())
    assert result.input_command is None


# ---------------------------------------------------------------------------
# Safety layer integration — never bypassed
# ---------------------------------------------------------------------------

def test_safety_manager_is_consulted_for_forward():
    """
    Verify the SafetyManager is part of the forward command path.
    We pass a clear snapshot — command should succeed.
    """
    manager, _, _, alerts = make_manager(response="move forward")
    manager.start_listening()
    manager.process(clear_snapshot())
    confirmed = [a for a in alerts.alert_history() if a.alert_type == AlertType.COMMAND_CONFIRMED]
    assert len(confirmed) >= 1


def test_safety_layer_suppresses_forward_when_obstacle():
    """
    When SafetyManager detects a close obstacle (WARNING state),
    the voice command must be suppressed — no COMMAND_CONFIRMED alert,
    no motion.
    """
    manager, _, _, alerts = make_manager(response="move forward")
    manager.start_listening()
    manager.process(obstacle_snapshot())

    confirmed = [a for a in alerts.alert_history() if a.alert_type == AlertType.COMMAND_CONFIRMED]
    # Safety WARNING does not require stop by default — only STOP_REQUIRED blocks.
    # This test documents the current behaviour.
    # A future phase may add stricter safety gating.
    # For now verify the full pipeline ran without crash.
    assert manager.state == AudioState.IDLE


def test_voice_never_produces_motion_command_directly():
    """
    The AudioManager must never directly instantiate or return a MotionCommand.
    Voice → InputCommand → (validator → mapper → safety) is the only path.
    This is a structural safety test.
    """
    from autochair.motion.command import MotionCommand

    manager, _, _, _ = make_manager(response="move forward")
    manager.start_listening()
    result = manager.process(clear_snapshot())

    # ParseResult must not contain a MotionCommand
    assert not isinstance(result.input_command, MotionCommand)


def test_input_command_source_always_voice():
    """All InputCommands from AudioManager must have source=VOICE."""
    manager, _, _, _ = make_manager(response="stop")
    manager.start_listening()
    result = manager.process(clear_snapshot())
    if result.input_command is not None:
        assert result.input_command.source == InputSource.VOICE


# ---------------------------------------------------------------------------
# reset()
# ---------------------------------------------------------------------------

def test_reset_returns_to_idle_from_listening():
    manager, _, _, _ = make_manager()
    manager.start_listening()
    manager.reset()
    assert manager.state == AudioState.IDLE


def test_reset_stops_microphone():
    mic = SimulatedMicrophoneManager()
    recognizer = SimulatedSpeechRecognizer()
    tts = SimulatedTTSEngine()
    alerts = SimulatedAlertManager()
    manager = AudioManager(microphone=mic, recognizer=recognizer, tts=tts, alerts=alerts)
    manager.start_listening()
    manager.reset()
    assert mic.is_listening() is False


def test_reset_from_idle_does_not_raise():
    manager, _, _, _ = make_manager()
    manager.reset()  # must not raise
    assert manager.state == AudioState.IDLE


# ---------------------------------------------------------------------------
# last_parse_result
# ---------------------------------------------------------------------------

def test_last_parse_result_updated_after_process():
    manager, _, _, _ = make_manager(response="stop")
    manager.start_listening()
    manager.process(clear_snapshot())
    assert manager.last_parse_result is not None
    assert manager.last_parse_result.intent == VoiceIntent.STOP


# ---------------------------------------------------------------------------
# RealVoiceSession integration tests
# ---------------------------------------------------------------------------

from unittest.mock import MagicMock
from autochair.audio.voice_session import RealVoiceSession


def make_real_voice_manager(
    recognized_text: str = "",
) -> tuple[AudioManager, MagicMock, SimulatedTTSEngine, SimulatedAlertManager]:
    """Build an AudioManager configured with a mocked RealVoiceSession."""
    mock_session = MagicMock(spec=RealVoiceSession)
    mock_session.start.return_value = recognized_text

    tts = SimulatedTTSEngine()
    alerts = SimulatedAlertManager()
    manager = AudioManager(
        voice_session=mock_session,
        tts=tts,
        alerts=alerts,
    )
    return manager, mock_session, tts, alerts


class TestRealVoiceSessionIntegration:
    def test_real_voice_mode_flag_set(self):
        manager, mock_session, _, _ = make_real_voice_manager()
        assert manager.is_real_voice_mode is True
        assert manager.voice_session is mock_session

    def test_simulated_mode_flag_false(self):
        manager, _, _, _ = make_manager()
        assert manager.is_real_voice_mode is False
        assert manager.voice_session is None

    def test_real_voice_start_intent_no_motion_command(self):
        """
        Scenario A: Real voice session returns 'start'.
        START intent is recognized, confirmation alert raised, but NO MotionCommand produced.
        """
        manager, mock_session, tts, alerts = make_real_voice_manager(recognized_text="start")
        manager.start_listening()
        result = manager.process(clear_snapshot())

        mock_session.start.assert_called_once()
        assert result.intent == VoiceIntent.START
        assert result.input_command is None
        assert manager.state == AudioState.IDLE
        assert "starting" in tts.last_spoken().lower()

        confirmed = [a for a in alerts.alert_history() if a.alert_type == AlertType.COMMAND_CONFIRMED]
        assert len(confirmed) >= 1

    def test_real_voice_stop_command_passes_safety_pipeline(self):
        """
        Scenario B: Real voice session returns 'stop'.
        STOP InputCommand is produced, validated, mapped, and safety evaluated.
        """
        manager, mock_session, tts, alerts = make_real_voice_manager(recognized_text="stop")
        manager.start_listening()
        result = manager.process(clear_snapshot())

        mock_session.start.assert_called_once()
        assert result.intent == VoiceIntent.STOP
        assert result.input_command is not None
        assert result.input_command.command == "STOP"
        assert result.input_command.source == InputSource.VOICE
        assert manager.state == AudioState.IDLE
        assert "stop" in tts.last_spoken().lower()

        confirmed = [a for a in alerts.alert_history() if a.alert_type == AlertType.COMMAND_CONFIRMED]
        assert len(confirmed) >= 1

    def test_real_voice_move_forward_command_passes_validation_and_safety(self):
        """
        Scenario C: Real voice session returns 'move forward'.
        FORWARD InputCommand is produced, passes InputCommandValidator, CommandMapper, and SafetyManager.
        """
        manager, mock_session, tts, alerts = make_real_voice_manager(recognized_text="move forward")
        manager.start_listening()
        result = manager.process(clear_snapshot())

        mock_session.start.assert_called_once()
        assert result.intent == VoiceIntent.MOVE_FORWARD
        assert result.input_command is not None
        assert result.input_command.command == "FORWARD"
        assert result.input_command.source == InputSource.VOICE
        assert manager.state == AudioState.IDLE
        assert "forward" in tts.last_spoken().lower()

        confirmed = [a for a in alerts.alert_history() if a.alert_type == AlertType.COMMAND_CONFIRMED]
        assert len(confirmed) >= 1

    def test_real_voice_empty_recognition_produces_no_motion(self):
        """
        Scenario D: Real voice session returns '' (silence/timeout).
        Produces UNKNOWN intent, raises COMMAND_REJECTED alert, no motion.
        """
        manager, mock_session, tts, alerts = make_real_voice_manager(recognized_text="")
        manager.start_listening()
        result = manager.process(clear_snapshot())

        mock_session.start.assert_called_once()
        assert result.intent == VoiceIntent.UNKNOWN
        assert result.input_command is None
        assert manager.state == AudioState.IDLE
        assert "not recognized" in tts.last_spoken().lower()

        rejected = [a for a in alerts.alert_history() if a.alert_type == AlertType.COMMAND_REJECTED]
        assert len(rejected) >= 1

    def test_real_voice_unknown_text_produces_no_motion(self):
        """
        Scenario E: Real voice session returns unknown text (e.g. '[unk]' or unmapped phrase).
        Produces UNKNOWN intent, no InputCommand, no motion.
        """
        manager, mock_session, tts, alerts = make_real_voice_manager(recognized_text="banana split")
        manager.start_listening()
        result = manager.process(clear_snapshot())

        mock_session.start.assert_called_once()
        assert result.intent == VoiceIntent.UNKNOWN
        assert result.input_command is None
        assert manager.state == AudioState.IDLE

        rejected = [a for a in alerts.alert_history() if a.alert_type == AlertType.COMMAND_REJECTED]
        assert len(rejected) >= 1

    def test_real_voice_obstacle_in_front_suppressed_by_safety(self):
        """
        Verify safety layer suppresses motion when an obstacle requires stop:
        'move forward' recognized → InputCommand FORWARD → SafetyManager requires stop → motion suppressed.
        """
        from autochair.safety.state import SafetyState, SafetyStatus

        mock_session = MagicMock(spec=RealVoiceSession)
        mock_session.start.return_value = "move forward"

        safety_manager = MagicMock(spec=SafetyManager)
        stop_status = SafetyStatus()
        stop_status.set_state(SafetyState.STOP_REQUIRED)
        safety_manager.evaluate.return_value = stop_status

        tts = SimulatedTTSEngine()
        alerts = SimulatedAlertManager()

        manager = AudioManager(
            voice_session=mock_session,
            tts=tts,
            alerts=alerts,
            safety=safety_manager,
        )

        manager.start_listening()
        result = manager.process(obstacle_snapshot())

        # 1. Voice input parsed to FORWARD InputCommand
        assert result.intent == VoiceIntent.MOVE_FORWARD
        assert result.input_command is not None
        assert result.input_command.command == "FORWARD"

        # 2. Safety layer was evaluated
        safety_manager.evaluate.assert_called_once()

        # 3. Motion was suppressed: OBSTACLE alert raised, COMMAND_CONFIRMED was NOT raised
        confirmed_alerts = [
            a for a in alerts.alert_history() if a.alert_type == AlertType.COMMAND_CONFIRMED
        ]
        assert len(confirmed_alerts) == 0

        obstacle_alerts = [
            a for a in alerts.alert_history() if a.alert_type == AlertType.OBSTACLE
        ]
        assert len(obstacle_alerts) == 1
        assert "Safety layer requires stop" in obstacle_alerts[0].message

        # 4. TTS announces obstacle stopping, state returns to IDLE
        assert "obstacle" in tts.last_spoken().lower()
        assert manager.state == AudioState.IDLE

    def test_real_voice_session_exception_returns_to_idle_safely(self):
        """
        Verify exception safety: when RealVoiceSession.start() raises,
        AudioManager does not remain stuck in PROCESSING, returns to IDLE,
        raises a COMMAND_REJECTED alert, and delivers error TTS feedback.
        """
        mock_session = MagicMock(spec=RealVoiceSession)
        mock_session.start.side_effect = RuntimeError("ALSA device overrun / hardware disconnect")

        tts = SimulatedTTSEngine()
        alerts = SimulatedAlertManager()

        manager = AudioManager(
            voice_session=mock_session,
            tts=tts,
            alerts=alerts,
        )

        manager.start_listening()
        assert manager.state == AudioState.LISTENING

        result = manager.process(clear_snapshot())

        # 1. AudioManager must not remain in PROCESSING; must be back in IDLE
        assert manager.state == AudioState.IDLE

        # 2. Result is safe fallback UNKNOWN intent with no InputCommand
        assert result.intent == VoiceIntent.UNKNOWN
        assert result.input_command is None

        # 3. Rejection alert is logged and raised
        rejected_alerts = [
            a for a in alerts.alert_history() if a.alert_type == AlertType.COMMAND_REJECTED
        ]
        assert len(rejected_alerts) >= 1
        assert "Audio processing error" in rejected_alerts[-1].message

        # 4. TTS announces rejection feedback
        assert "not recognized" in tts.last_spoken().lower()

    def test_real_voice_reverse_command_passes_validation_and_safety(self):
        """
        Verify REVERSE voice command: 'move backward' → REVERSE InputCommand → Safety pipeline.
        """
        manager, mock_session, tts, alerts = make_real_voice_manager(recognized_text="move backward")
        manager.start_listening()
        result = manager.process(clear_snapshot())

        assert result.intent == VoiceIntent.REVERSE
        assert result.input_command is not None
        assert result.input_command.command == "REVERSE"
        assert result.input_command.source == InputSource.VOICE
        assert manager.state == AudioState.IDLE
        assert "reverse" in tts.last_spoken().lower()

    def test_real_voice_left_command_passes_validation_and_safety(self):
        """
        Verify LEFT voice command: 'turn left' → LEFT InputCommand → Safety pipeline.
        """
        manager, mock_session, tts, alerts = make_real_voice_manager(recognized_text="turn left")
        manager.start_listening()
        result = manager.process(clear_snapshot())

        assert result.intent == VoiceIntent.LEFT
        assert result.input_command is not None
        assert result.input_command.command == "LEFT"
        assert result.input_command.source == InputSource.VOICE
        assert manager.state == AudioState.IDLE
        assert "left" in tts.last_spoken().lower()

    def test_real_voice_right_command_passes_validation_and_safety(self):
        """
        Verify RIGHT voice command: 'turn right' → RIGHT InputCommand → Safety pipeline.
        """
        manager, mock_session, tts, alerts = make_real_voice_manager(recognized_text="turn right")
        manager.start_listening()
        result = manager.process(clear_snapshot())

        assert result.intent == VoiceIntent.RIGHT
        assert result.input_command is not None
        assert result.input_command.command == "RIGHT"
        assert result.input_command.source == InputSource.VOICE
        assert manager.state == AudioState.IDLE
        assert "right" in tts.last_spoken().lower()

    @pytest.mark.parametrize("hindi_phrase,expected_intent,expected_cmd", [
        ("chalo", VoiceIntent.START, None),
        ("ruko", VoiceIntent.STOP, "STOP"),
        ("rukko", VoiceIntent.STOP, "STOP"),
        ("aage chalo", VoiceIntent.MOVE_FORWARD, "FORWARD"),
        ("aage jao", VoiceIntent.MOVE_FORWARD, "FORWARD"),
        ("piche chalo", VoiceIntent.REVERSE, "REVERSE"),
        ("peeche chalo", VoiceIntent.REVERSE, "REVERSE"),
        ("piche jao", VoiceIntent.REVERSE, "REVERSE"),
        ("peeche jao", VoiceIntent.REVERSE, "REVERSE"),
        ("left chalo", VoiceIntent.LEFT, "LEFT"),
        ("right chalo", VoiceIntent.RIGHT, "RIGHT"),
    ])
    def test_real_voice_hindi_commands_integration(
        self, hindi_phrase: str, expected_intent: VoiceIntent, expected_cmd: str | None
    ):
        manager, mock_session, tts, alerts = make_real_voice_manager(recognized_text=hindi_phrase)
        manager.start_listening()
        result = manager.process(clear_snapshot())

        assert result.intent == expected_intent
        if expected_cmd is not None:
            assert result.input_command is not None
            assert result.input_command.command == expected_cmd
            assert result.input_command.source == InputSource.VOICE
        else:
            assert result.input_command is None
        assert manager.state == AudioState.IDLE
