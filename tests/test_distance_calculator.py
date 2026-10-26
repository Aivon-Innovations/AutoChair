import math

import pytest

from autochair.navigation.distance_calculator import DistanceCalculator
from autochair.navigation.encoder_resolution import EncoderResolution
from autochair.navigation.wheel_geometry import WheelGeometry


def test_zero_ticks_gives_zero_distance():
    calculator = DistanceCalculator()

    assert calculator.ticks_to_distance(0) == 0.0


def test_one_revolution_gives_wheel_circumference():
    geometry = WheelGeometry(wheel_diameter_m=0.30)
    resolution = EncoderResolution(
        ppr=600,
        decoding_multiplier=1,
    )

    calculator = DistanceCalculator(
        geometry=geometry,
        resolution=resolution,
    )

    expected_distance = math.pi * 0.30

    assert math.isclose(
        calculator.ticks_to_distance(600),
        expected_distance,
    )


def test_negative_ticks_give_negative_distance():
    calculator = DistanceCalculator()

    distance = calculator.ticks_to_distance(-600)

    assert math.isclose(
        distance,
        -math.pi * 0.30,
    )


def test_encoder_distances():
    calculator = DistanceCalculator()

    left_distance, right_distance = calculator.encoder_distances(
        600,
        1200,
    )

    assert math.isclose(left_distance, math.pi * 0.30)
    assert math.isclose(right_distance, 2 * math.pi * 0.30)


def test_average_distance():
    calculator = DistanceCalculator()

    average = calculator.average_distance(
        600,
        1200,
    )

    expected = (math.pi * 0.30 + 2 * math.pi * 0.30) / 2

    assert math.isclose(average, expected)


def test_x2_resolution_is_used():
    resolution = EncoderResolution(
        ppr=600,
        decoding_multiplier=2,
    )

    calculator = DistanceCalculator(
        resolution=resolution,
    )

    distance = calculator.ticks_to_distance(1200)

    assert math.isclose(
        distance,
        math.pi * 0.30,
    )


def test_x4_resolution_is_used():
    resolution = EncoderResolution(
        ppr=600,
        decoding_multiplier=4,
    )

    calculator = DistanceCalculator(
        resolution=resolution,
    )

    distance = calculator.ticks_to_distance(2400)

    assert math.isclose(
        distance,
        math.pi * 0.30,
    )


def test_custom_wheel_geometry():
    geometry = WheelGeometry(
        wheel_diameter_m=0.40,
    )

    calculator = DistanceCalculator(
        geometry=geometry,
    )

    distance = calculator.ticks_to_distance(600)

    assert math.isclose(
        distance,
        math.pi * 0.40,
    )


def test_zero_ppr_is_rejected():
    with pytest.raises(ValueError):
        EncoderResolution(ppr=0)


def test_invalid_decoding_multiplier_is_rejected():
    with pytest.raises(ValueError):
        EncoderResolution(
            ppr=600,
            decoding_multiplier=3,
        )
