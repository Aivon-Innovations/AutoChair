import math

import pytest

from autochair.navigation.differential_odometry import DifferentialOdometry


def test_straight_line_movement():
    odometry = DifferentialOdometry()

    odometry.update(
        left_distance_m=1.0,
        right_distance_m=1.0,
    )

    pose = odometry.get_pose()

    assert math.isclose(pose.x_m, 1.0)
    assert math.isclose(pose.y_m, 0.0)
    assert math.isclose(pose.heading_deg, 0.0)


def test_turning_movement():
    odometry = DifferentialOdometry()

    odometry.update(
        left_distance_m=0.5,
        right_distance_m=1.0,
    )

    pose = odometry.get_pose()

    assert math.isclose(pose.x_m, 0.75)
    assert math.isclose(pose.y_m, 0.0)
    assert math.isclose(
        pose.heading_deg,
        math.degrees(1.0),
    )


def test_reset():
    odometry = DifferentialOdometry()

    odometry.update(
        left_distance_m=1.0,
        right_distance_m=1.0,
    )

    odometry.reset()

    pose = odometry.get_pose()

    assert pose.x_m == 0.0
    assert pose.y_m == 0.0
    assert pose.heading_deg == 0.0


def test_nan_left_distance_rejected():
    odometry = DifferentialOdometry()

    with pytest.raises(ValueError):
        odometry.update(
            left_distance_m=float("nan"),
            right_distance_m=1.0,
        )


def test_infinite_right_distance_rejected():
    odometry = DifferentialOdometry()

    with pytest.raises(ValueError):
        odometry.update(
            left_distance_m=1.0,
            right_distance_m=float("inf"),
        )
