import math

from autochair.navigation.wheel_geometry import WheelGeometry


class DistanceCalculator:
    def __init__(
        self,
        geometry: WheelGeometry | None = None,
        ticks_per_revolution: int = 600,
    ):
        self.geometry = geometry or WheelGeometry()
        self.ticks_per_revolution = ticks_per_revolution

    def ticks_to_distance(self, ticks: int) -> float:
        wheel_circumference_m = (
            math.pi * self.geometry.wheel_diameter_m
        )

        revolutions = ticks / self.ticks_per_revolution

        return revolutions * wheel_circumference_m
