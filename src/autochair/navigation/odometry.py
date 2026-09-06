from autochair.navigation.pose import Pose


class Odometry:
    def __init__(self):
        self.pose = Pose()

    def get_pose(self) -> Pose:
        return self.pose

    def reset(self) -> None:
        self.pose = Pose()

    def update(
        self,
        distance_m: float,
        heading_deg: float | None = None,
    ) -> None:
        self.pose.x_m += distance_m

        if heading_deg is not None:
            self.pose.heading_deg = heading_deg
