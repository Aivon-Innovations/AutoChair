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

    distance_change = (0.5 + 1.0) / 2
    heading_change_rad = (1.0 - 0.5) / 0.5
    turning_radius = distance_change / heading_change_rad

    expected_x = (
        turning_radius * math.sin(heading_change_rad)
    )

    expected_y = (
        turning_radius
        * (1 - math.cos(heading_change_rad))
    )

    expected_heading = math.degrees(
        heading_change_rad
    )

    assert math.isclose(pose.x_m, expected_x)
    assert math.isclose(pose.y_m, expected_y)
    assert math.isclose(
        pose.heading_deg,
        expected_heading,
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

    assert math.isclose(
        pose.x_m,
        math.pi * 0.30,
    )

    assert math.isclose(pose.y_m, 0.0)
    assert math.isclose(pose.heading_deg, 0.0)


def test_update_from_encoder_ticks_turning():
    odometry = DifferentialOdometry()

    odometry.update_from_encoder_ticks(
        left_ticks=300,
        right_ticks=600,
    )

    pose = odometry.get_pose()

    left_distance = (
        math.pi * 0.30 * 0.5
    )

    right_distance = (
        math.pi * 0.30
    )

    distance_change = (
        left_distance + right_distance
    ) / 2

    heading_change_rad = (
        right_distance - left_distance
    ) / 0.50

    turning_radius = (
        distance_change / heading_change_rad
    )

    expected_x = (
        turning_radius
        * math.sin(heading_change_rad)
    )

    expected_y = (
        turning_radius
        * (1 - math.cos(heading_change_rad))
    )

    expected_heading = math.degrees(
        heading_change_rad
    )

    assert math.isclose(
        pose.x_m,
        expected_x,
    )

    assert math.isclose(
        pose.y_m,
        expected_y,
    )

    assert math.isclose(
        pose.heading_deg,
        expected_heading,
    )


def test_multiple_odometry_updates():
    odometry = DifferentialOdometry()

    odometry.update(
        left_distance_m=1.0,
        right_distance_m=1.0,
    )

    odometry.update(
        left_distance_m=0.5,
        right_distance_m=1.0,
    )

    pose = odometry.get_pose()

    first_x = 1.0
    first_y = 0.0
    first_heading_rad = 0.0

    distance_change = (
        0.5 + 1.0
    ) / 2

    heading_change_rad = (
        1.0 - 0.5
    ) / 0.5

    turning_radius = (
        distance_change
        / heading_change_rad
    )

    second_x = (
        turning_radius
        * (
            math.sin(
                first_heading_rad
                + heading_change_rad
            )
            - math.sin(first_heading_rad)
        )
    )

    second_y = (
        -turning_radius
        * (
            math.cos(
                first_heading_rad
                + heading_change_rad
            )
            - math.cos(first_heading_rad)
        )
    )

    expected_x = first_x + second_x
    expected_y = first_y + second_y

    expected_heading = math.degrees(
        first_heading_rad
        + heading_change_rad
    )

    assert math.isclose(
        pose.x_m,
        expected_x,
    )

    assert math.isclose(
        pose.y_m,
        expected_y,
    )

    assert math.isclose(
        pose.heading_deg,
        expected_heading,
    )


def test_heading_wraps_above_180_degrees():
    odometry = DifferentialOdometry()

    odometry.pose.heading_deg = 170.0

    odometry.update(
        left_distance_m=0.0,
        right_distance_m=1.0,
    )

    assert math.isclose(
        odometry.get_pose().heading_deg,
        -75.40844097383551,
    )


def test_heading_wraps_below_minus_180_degrees():
    odometry = DifferentialOdometry()

    odometry.pose.heading_deg = -170.0

    odometry.update(
        left_distance_m=1.0,
        right_distance_m=0.0,
    )

    assert math.isclose(
        odometry.get_pose().heading_deg,
        75.40844097383551,
    )


def test_encoder_direction_can_invert_left_ticks():
    from autochair.navigation.encoder_direction import EncoderDirection

    direction = EncoderDirection(
        left_sign=-1,
        right_sign=1,
    )

    odometry = DifferentialOdometry(
        direction=direction,
    )

    odometry.update_from_encoder_ticks(
        left_ticks=600,
        right_ticks=600,
    )

    pose = odometry.get_pose()

    assert math.isclose(
        pose.heading_deg,
        -144.0,
    )


def test_encoder_direction_can_invert_right_ticks():
    from autochair.navigation.encoder_direction import EncoderDirection

    direction = EncoderDirection(
        left_sign=1,
        right_sign=-1,
    )

    odometry = DifferentialOdometry(
        direction=direction,
    )

    odometry.update_from_encoder_ticks(
        left_ticks=600,
        right_ticks=600,
    )

    pose = odometry.get_pose()

    assert math.isclose(
        pose.heading_deg,
        144.0,
    )
