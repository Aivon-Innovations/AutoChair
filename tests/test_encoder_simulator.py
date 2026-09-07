from autochair.sensors.encoder_simulator import EncoderSimulator


def test_default_encoder_simulator():
    simulator = EncoderSimulator()

    data = simulator.get_data()

    assert data.left_ticks == 0
    assert data.right_ticks == 0


def test_set_encoder_ticks():
    simulator = EncoderSimulator()

    simulator.set_ticks(1200, 1250)

    data = simulator.get_data()

    assert data.left_ticks == 1200
    assert data.right_ticks == 1250
