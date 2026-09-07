from autochair.navigation.odometry import Odometry
from autochair.sensors.encoder import EncoderData


class EncoderOdometry:
    def __init__(self):
        self.odometry = Odometry()

    def update(self, encoder_data: EncoderData) -> None:
        self.odometry.update_from_encoders(encoder_data)

    def get_pose(self):
        return self.odometry.get_pose()

    def reset(self) -> None:
        self.odometry.reset()
