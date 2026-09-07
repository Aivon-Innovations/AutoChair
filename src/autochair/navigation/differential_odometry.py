import math

from autochair.navigation.pose import Pose
from autochair.navigation.wheel_geometry import WheelGeometry


class DifferentialOdometry:
    def __init__(
        self,
        geometry: WheelGeometry | None = None,
    ):
        self.geometry = geometry or WheelGeometry()
        self.pose = Pose()

    def get_pose(self) -> Pose:
        return self.pose

    def reset(self) -> None:
        self.pose = Pose()

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

        distance_change = (
            left_distance_m + right_distance_m
        ) / 2

        heading_change_rad = (
            right_distance_m - left_distance_m
        ) / wheel_base

        heading_rad = math.radians(self.pose.heading_deg)

        self.pose.x_m += (
            distance_change * math.cos(heading_rad)
        )

        self.pose.y_m += (
            distance_change * math.sin(heading_rad)
        )

        heading_rad += heading_change_rad

        self.pose.heading_deg = math.degrees(heading_rad)
