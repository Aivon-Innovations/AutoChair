from autochair.sensors.data import SensorSnapshot, UltrasonicData


class SensorSimulator:
    def __init__(self):
        self.snapshot = SensorSnapshot(
            ultrasonic=UltrasonicData()
        )

    def get_snapshot(self) -> SensorSnapshot:
        return self.snapshot

    def set_ultrasonic_data(self, data: UltrasonicData) -> None:
        self.snapshot.ultrasonic = data
