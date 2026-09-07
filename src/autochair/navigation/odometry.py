from autochair.navigation.pose import Pose
from autochair.sensors.encoder import EncoderData


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

    def update_from_encoders(
        self,
        encoder_data: EncoderData,
    ) -> None:
        # Temporary simulation mapping.
        # Real distance calculation will be added after
        # the physical encoder and wheel specifications are verified.
        average_ticks = (
            encoder_data.left_ticks + encoder_data.right_ticks
        ) / 2

        simulated_distance_m = average_ticks / 1000.0

        self.pose.x_m += simulated_distance_m
