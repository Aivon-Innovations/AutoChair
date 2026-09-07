from autochair.navigation.encoder_odometry import EncoderOdometry
from autochair.sensors.encoder import EncoderData


def test_encoder_odometry_update():
    system = EncoderOdometry()

    system.update(
        EncoderData(
            left_ticks=1000,
            right_ticks=1000,
        )
    )

    pose = system.get_pose()

    assert pose.x_m == 1.0
    assert pose.y_m == 0.0
    assert pose.heading_deg == 0.0


def test_encoder_odometry_reset():
    system = EncoderOdometry()

    system.update(
        EncoderData(
            left_ticks=1000,
            right_ticks=1000,
        )
    )

    system.reset()

    pose = system.get_pose()

    assert pose.x_m == 0.0
    assert pose.y_m == 0.0
    assert pose.heading_deg == 0.0
