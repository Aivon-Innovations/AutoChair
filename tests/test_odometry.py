from autochair.navigation.odometry import Odometry


def test_default_odometry_pose():
    odometry = Odometry()

    pose = odometry.get_pose()

    assert pose.x_m == 0.0
    assert pose.y_m == 0.0
    assert pose.heading_deg == 0.0


def test_odometry_reset():
    odometry = Odometry()

    odometry.pose.x_m = 5.0
    odometry.pose.y_m = 3.0
    odometry.pose.heading_deg = 90.0

    odometry.reset()

    pose = odometry.get_pose()

    assert pose.x_m == 0.0
    assert pose.y_m == 0.0
    assert pose.heading_deg == 0.0
