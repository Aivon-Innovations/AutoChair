"""
AutoChair Audio/Voice Subsystem

Provides:
- AudioState: subsystem state lifecycle enum
- AudioManager: orchestrates all audio sub-modules
- VoiceCommandParser: converts recognized speech to AutoChair InputCommands
- SimulatedMicrophoneManager: hardware stub for development/testing
- SimulatedSpeechRecognizer: recognition stub for development/testing
- SimulatedTTSEngine: TTS stub for development/testing
- SimulatedAlertManager: alert stub for development/testing

Hardware status: SIMULATED
Real I2S microphone, PAM8403 amplifier, and speaker hardware are NOT
integrated in this phase. Physical hardware testing requires the Raspberry Pi 4
environment and will be addressed in a future phase.
"""

from autochair.audio.state import AudioState

__all__ = ["AudioState"]
