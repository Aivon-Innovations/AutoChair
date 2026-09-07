from autochair.sensors.encoder import EncoderData


class EncoderSimulator:
    def __init__(self):
        self.encoder_data = EncoderData()

    def get_data(self) -> EncoderData:
        return self.encoder_data

    def set_ticks(self, left_ticks: int, right_ticks: int) -> None:
        self.encoder_data.left_ticks = left_ticks
        self.encoder_data.right_ticks = right_ticks
