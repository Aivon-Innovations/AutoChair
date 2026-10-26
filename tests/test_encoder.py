from autochair.sensors.encoder import EncoderData


def test_default_encoder_data():
    encoder = EncoderData()

    assert encoder.left_ticks == 0
    assert encoder.right_ticks == 0


def test_custom_encoder_data():
    encoder = EncoderData(
        left_ticks=1200,
        right_ticks=1250,
    )

    assert encoder.left_ticks == 1200
    assert encoder.right_ticks == 1250
