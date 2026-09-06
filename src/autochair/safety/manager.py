from autochair.safety.obstacle_detector import ObstacleDetector
from autochair.safety.state import SafetyState, SafetyStatus
from autochair.sensors.data import SensorSnapshot


class SafetyManager:
    def __init__(self, obstacle_detector: ObstacleDetector | None = None):
        self.obstacle_detector = obstacle_detector or ObstacleDetector()
        self.status = SafetyStatus()

    def evaluate(self, snapshot: SensorSnapshot) -> SafetyStatus:
        obstacles = self.obstacle_detector.detect(snapshot)

        if any(obstacles.values()):
            self.status.set_state(SafetyState.WARNING)
        else:
            self.status.set_state(SafetyState.CLEAR)

        return self.status
