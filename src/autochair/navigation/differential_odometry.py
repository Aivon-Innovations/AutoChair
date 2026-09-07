import math

from autochair.navigation.distance_calculator import DistanceCalculator
from autochair.navigation.encoder_resolution import EncoderResolution
from autochair.navigation.pose import Pose
from autochair.navigation.wheel_geometry import WheelGeometry


class DifferentialOdometry:
    def __init__(
        self,
        geometry: WheelGeometry | None = None,
        resolution: EncoderResolution | None = None,
    ):
        self.geometry = geometry or WheelGeometry()
        self.distance_calculator = DistanceCalculator(
            geometry=self.geometry,
            resolution=resolution,
        )
        self.pose = Pose()

    def get_pose(self) -> Pose:
        return self.pose

    def reset(self) -> None:
        self.pose = Pose()

    def _normalize_heading(self, heading_deg: float) -> float:
        return (heading_deg + 180.0) % 360.0 - 180.0

    def update(
        self,
        left_distance_m: float,
        right_distance_m: float,
    ) -> None:
        if not math.isfinite(left_distance_m):
            raise ValueError("Left wheel distance must be finite.")

        if not math.isfinite(right_distance_m):
            raise ValueError("Right wheel distance must be finite.")

        wheel_base = self.geometry.wheel_base_m

        if wheel_base <= 0:
            raise ValueError("Wheelbase must be greater than zero.")

        heading_rad = math.radians(self.pose.heading_deg)

        distance_change = (
            left_distance_m + right_distance_m
        ) / 2

        heading_change_rad = (
            right_distance_m - left_distance_m
        ) / wheel_base

        if math.isclose(
            heading_change_rad,
            0.0,
            abs_tol=1e-12,
        ):
            self.pose.x_m += (
                distance_change * math.cos(heading_rad)
            )

            self.pose.y_m += (
                distance_change * math.sin(heading_rad)
            )
        else:
            turning_radius = (
                distance_change / heading_change_rad
            )

            new_heading_rad = (
                heading_rad + heading_change_rad
            )

            self.pose.x_m += (
                turning_radius
                * (
                    math.sin(new_heading_rad)
                    - math.sin(heading_rad)
                )
            )

            self.pose.y_m += (
                -turning_radius
                * (
                    math.cos(new_heading_rad)
                    - math.cos(heading_rad)
                )
            )

        heading_rad += heading_change_rad

        self.pose.heading_deg = self._normalize_heading(
            math.degrees(heading_rad)
        )

    def update_from_encoder_ticks(
        self,
        left_ticks: int,
        right_ticks: int,
    ) -> None:
        left_distance_m, right_distance_m = (
            self.distance_calculator.encoder_distances(
                left_ticks,
                right_ticks,
            )
        )

        self.update(
            left_distance_m=left_distance_m,
            right_distance_m=right_distance_m,
        )
