import math

from autochair.navigation.wheel_geometry import WheelGeometry


class DistanceCalculator:
    def __init__(
        self,
        geometry: WheelGeometry | None = None,
        ticks_per_revolution: int = 600,
    ):
        if ticks_per_revolution <= 0:
            raise ValueError("Ticks per revolution must be greater than zero.")

        self.geometry = geometry or WheelGeometry()
        self.ticks_per_revolution = ticks_per_revolution

    def ticks_to_distance(self, ticks: int) -> float:
        wheel_circumference_m = (
            math.pi * self.geometry.wheel_diameter_m
        )

        revolutions = ticks / self.ticks_per_revolution

        return revolutions * wheel_circumference_m

    def encoder_distances(
        self,
        left_ticks: int,
        right_ticks: int,
    ) -> tuple[float, float]:
        left_distance_m = self.ticks_to_distance(left_ticks)
        right_distance_m = self.ticks_to_distance(right_ticks)

        return left_distance_m, right_distance_m

    def average_distance(
        self,
        left_ticks: int,
        right_ticks: int,
    ) -> float:
        left_distance_m, right_distance_m = self.encoder_distances(
            left_ticks,
            right_ticks,
        )

        return (left_distance_m + right_distance_m) / 2
