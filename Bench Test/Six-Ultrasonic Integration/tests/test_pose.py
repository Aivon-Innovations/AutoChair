from autochair.navigation.pose import Pose


def test_default_pose():
    pose = Pose()

    assert pose.x_m == 0.0
    assert pose.y_m == 0.0
    assert pose.heading_deg == 0.0


def test_custom_pose():
    pose = Pose(
        x_m=2.5,
        y_m=1.5,
        heading_deg=90.0,
    )

    assert pose.x_m == 2.5
    assert pose.y_m == 1.5
    assert pose.heading_deg == 90.0
