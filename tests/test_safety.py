from autochair.sensors.data import UltrasonicData, SensorSnapshot
from autochair.safety.manager import SafetyManager
from autochair.safety.state import SafetyState


def test_clear_environment():
    snapshot = SensorSnapshot(
        ultrasonic=UltrasonicData()
    )

    manager = SafetyManager()
    status = manager.evaluate(snapshot)

    assert status.state == SafetyState.CLEAR


def test_front_obstacle():
    snapshot = SensorSnapshot(
        ultrasonic=UltrasonicData(
            front_center_cm=30.0
        )
    )

    manager = SafetyManager()
    status = manager.evaluate(snapshot)

    assert status.state == SafetyState.WARNING
