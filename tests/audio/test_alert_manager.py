"""
Tests for AlertType, AlertEvent, and SimulatedAlertManager.

Verifies alert type definitions, event structure, and alert manager
recording behaviour. No buzzer, GPIO, or hardware is accessed.
"""

import pytest

from autochair.audio.alerts.manager import (
    AlertEvent,
    AlertManager,
    AlertType,
    SimulatedAlertManager,
)


# ---------------------------------------------------------------------------
# AlertType
# ---------------------------------------------------------------------------

def test_alert_type_warning_exists():
    assert AlertType.WARNING == "WARNING"


def test_alert_type_error_exists():
    assert AlertType.ERROR == "ERROR"


def test_alert_type_obstacle_exists():
    assert AlertType.OBSTACLE == "OBSTACLE"


def test_alert_type_command_confirmed_exists():
    assert AlertType.COMMAND_CONFIRMED == "COMMAND_CONFIRMED"


def test_alert_type_command_rejected_exists():
    assert AlertType.COMMAND_REJECTED == "COMMAND_REJECTED"


def test_alert_type_emergency_exists():
    assert AlertType.EMERGENCY == "EMERGENCY"


def test_alert_type_is_str_enum():
    assert isinstance(AlertType.WARNING, str)


# ---------------------------------------------------------------------------
# AlertEvent
# ---------------------------------------------------------------------------

def test_alert_event_construction():
    event = AlertEvent(alert_type=AlertType.WARNING, message="Obstacle ahead")
    assert event.alert_type == AlertType.WARNING
    assert event.message == "Obstacle ahead"


# ---------------------------------------------------------------------------
# SimulatedAlertManager
# ---------------------------------------------------------------------------

def test_simulated_alert_manager_implements_interface():
    manager = SimulatedAlertManager()
    assert isinstance(manager, AlertManager)


def test_simulated_alert_manager_starts_empty():
    manager = SimulatedAlertManager()
    assert manager.alert_history() == []


def test_simulated_alert_manager_records_alert():
    manager = SimulatedAlertManager()
    event = AlertEvent(alert_type=AlertType.WARNING, message="Test warning")
    manager.raise_alert(event)
    assert len(manager.alert_history()) == 1


def test_simulated_alert_manager_records_correct_type():
    manager = SimulatedAlertManager()
    manager.raise_alert(AlertEvent(AlertType.OBSTACLE, "Obstacle close"))
    assert manager.alert_history()[0].alert_type == AlertType.OBSTACLE


def test_simulated_alert_manager_records_multiple_alerts():
    manager = SimulatedAlertManager()
    manager.raise_alert(AlertEvent(AlertType.WARNING, "First"))
    manager.raise_alert(AlertEvent(AlertType.ERROR, "Second"))
    manager.raise_alert(AlertEvent(AlertType.EMERGENCY, "Third"))
    assert len(manager.alert_history()) == 3


def test_simulated_alert_manager_last_alert():
    manager = SimulatedAlertManager()
    manager.raise_alert(AlertEvent(AlertType.WARNING, "First"))
    manager.raise_alert(AlertEvent(AlertType.EMERGENCY, "Last"))
    last = manager.last_alert()
    assert last is not None
    assert last.alert_type == AlertType.EMERGENCY


def test_simulated_alert_manager_last_alert_none_when_empty():
    manager = SimulatedAlertManager()
    assert manager.last_alert() is None


def test_simulated_alert_manager_history_is_copy():
    """Mutating the returned list must not affect internal state."""
    manager = SimulatedAlertManager()
    manager.raise_alert(AlertEvent(AlertType.WARNING, "test"))
    history = manager.alert_history()
    history.clear()
    assert len(manager.alert_history()) == 1


def test_simulated_alert_manager_no_hardware_access():
    """
    Simulation boundary test: if this passes without hardware,
    the SimulatedAlertManager correctly avoids all GPIO/buzzer access.
    """
    manager = SimulatedAlertManager()
    manager.raise_alert(AlertEvent(AlertType.EMERGENCY, "Emergency stop"))
    assert manager.last_alert().alert_type == AlertType.EMERGENCY
