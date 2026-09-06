from autochair.navigation.pose import Pose


class Odometry:
    def __init__(self):
        self.pose = Pose()

    def get_pose(self) -> Pose:
        return self.pose

    def reset(self) -> None:
        self.pose = Pose()
