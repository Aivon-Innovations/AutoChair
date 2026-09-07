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
def test_update_from_encoder_ticks_straight_line():
    odometry = DifferentialOdometry()

    odometry.update_from_encoder_ticks(
        left_ticks=600,
        right_ticks=600,
    )

    pose = odometry.get_pose()

    assert math.isclose(pose.x_m, math.pi * 0.30)
    assert math.isclose(pose.y_m, 0.0)
    assert math.isclose(pose.heading_deg, 0.0)


def test_update_from_encoder_ticks_turning():
    odometry = DifferentialOdometry()

    odometry.update_from_encoder_ticks(
        left_ticks=300,
        right_ticks=600,
    )

    pose = odometry.get_pose()

    left_distance = math.pi * 0.30 * 0.5
    right_distance = math.pi * 0.30

    expected_distance = (
        left_distance + right_distance
    ) / 2

    expected_heading = math.degrees(
        (right_distance - left_distance) / 0.50
    )

    assert math.isclose(
        pose.x_m,
        expected_distance,
    )

    assert math.isclose(
        pose.y_m,
        0.0,
    )

    assert math.isclose(
        pose.heading_deg,
        expected_heading,
    )
