"""
AlertManager — abstract interface and simulated implementation.

Handles safety/system alerts by routing events to voice (TTS) and/or
buzzer output depending on alert severity and type.

Hardware status: SIMULATED
The real buzzer implementation requires:
  - Raspberry Pi 4 GPIO access (RPi.GPIO or gpiozero)
  - Active buzzer connected to a designated GPIO pin
  - Physical hardware testing on the Pi

DO NOT access GPIO, buzzer hardware, ALSA, or any physical device in this phase.
"""

from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from enum import Enum

from autochair.utils.logger import get_logger


logger = get_logger("autochair.audio.alerts.manager")


class AlertType(str, Enum):
    """
    Categories of alerts that the AutoChair audio subsystem can raise.

    WARNING          — non-critical caution (e.g. obstacle in range)
    ERROR            — system error requiring user attention
    OBSTACLE         — imminent obstacle detected by safety layer
    COMMAND_CONFIRMED — voice command was accepted and will execute
    COMMAND_REJECTED  — voice command was not recognized or was invalid
    EMERGENCY        — emergency stop or critical safety event
    """

    WARNING = "WARNING"
    ERROR = "ERROR"
    OBSTACLE = "OBSTACLE"
    COMMAND_CONFIRMED = "COMMAND_CONFIRMED"
    COMMAND_REJECTED = "COMMAND_REJECTED"
    EMERGENCY = "EMERGENCY"


@dataclass
class AlertEvent:
    """
    Represents a single alert raised within the audio subsystem.

    alert_type: The category of this alert.
    message:    Human-readable description of the event.
    """

    alert_type: AlertType
    message: str


class AlertManager(ABC):
    """
    Abstract interface for alert management.

    Receives AlertEvent objects from any AutoChair module and decides
    whether to route them to TTS, buzzer, or both.
    """

    @abstractmethod
    def raise_alert(self, event: AlertEvent) -> None:
        """
        Process an alert event.

        Args:
            event: The alert to raise.
        """

    @abstractmethod
    def alert_history(self) -> list[AlertEvent]:
        """Return all alerts raised since initialisation."""


class SimulatedAlertManager(AlertManager):
    """
    Software-only stub for the alert manager.

    Records all alerts in memory and logs them. No buzzer or speaker
    hardware is accessed.

    Usage in tests:
        manager = SimulatedAlertManager()
        manager.raise_alert(AlertEvent(AlertType.WARNING, "Obstacle ahead"))
        assert len(manager.alert_history()) == 1

    Hardware status: SIMULATED — not hardware-integrated or validated.
    No GPIO, buzzer, PAM8403, or ALSA access.
    """

    def __init__(self) -> None:
        self._history: list[AlertEvent] = []
        logger.info("SimulatedAlertManager initialised (no hardware access)")

    def raise_alert(self, event: AlertEvent) -> None:
        """Record and log the alert. No hardware action is taken."""
        self._history.append(event)
        logger.info(
            f"[SIMULATED] Alert: [{event.alert_type.value}] {event.message}"
        )

    def alert_history(self) -> list[AlertEvent]:
        """Return all alerts raised since initialisation."""
        return list(self._history)

    def last_alert(self) -> AlertEvent | None:
        """
        Return the most recent alert, or None if no alerts have been raised.

        Extension method available on SimulatedAlertManager for test convenience.
        """
        return self._history[-1] if self._history else None
