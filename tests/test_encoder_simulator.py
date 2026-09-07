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


def test_encoder_simulator_to_odometry():
    from autochair.navigation.odometry import Odometry

    simulator = EncoderSimulator()

    simulator.set_ticks(1000, 1000)

    odometry = Odometry()
    odometry.update_from_encoders(simulator.get_data())

    pose = odometry.get_pose()

    assert pose.x_m == 1.0
    assert pose.y_m == 0.0
    assert pose.heading_deg == 0.0
