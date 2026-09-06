from autochair.sensors.data import SensorSnapshot


class ObstacleDetector:
    def __init__(self, warning_distance_cm: float = 100.0):
        self.warning_distance_cm = warning_distance_cm

    def detect(self, snapshot: SensorSnapshot) -> dict[str, bool]:
        ultrasonic = snapshot.ultrasonic

        return {
            "front_left": ultrasonic.front_left_cm <= self.warning_distance_cm,
            "front_center": ultrasonic.front_center_cm <= self.warning_distance_cm,
            "front_right": ultrasonic.front_right_cm <= self.warning_distance_cm,
            "left": ultrasonic.left_cm <= self.warning_distance_cm,
            "right": ultrasonic.right_cm <= self.warning_distance_cm,
            "rear": ultrasonic.rear_cm <= self.warning_distance_cm,
        }
