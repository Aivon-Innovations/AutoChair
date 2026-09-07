import math

from autochair.navigation.distance_calculator import DistanceCalculator
from autochair.navigation.wheel_geometry import WheelGeometry


def test_one_wheel_revolution_distance():
    calculator = DistanceCalculator()

    distance = calculator.ticks_to_distance(600)

    expected = math.pi * 0.30

    assert math.isclose(distance, expected)


def test_zero_ticks():
    calculator = DistanceCalculator()

    distance = calculator.ticks_to_distance(0)

    assert distance == 0.0


def test_custom_wheel_diameter():
    calculator = DistanceCalculator(
        geometry=WheelGeometry(wheel_diameter_m=0.40)
    )

    distance = calculator.ticks_to_distance(600)

    expected = math.pi * 0.40

    assert math.isclose(distance, expected)


def test_encoder_distances():
    calculator = DistanceCalculator()

    left_distance, right_distance = calculator.encoder_distances(
        600,
        1200,
    )

    assert math.isclose(left_distance, math.pi * 0.30)
    assert math.isclose(right_distance, math.pi * 0.30 * 2)


def test_average_distance():
    calculator = DistanceCalculator()

    average = calculator.average_distance(
        600,
        1200,
    )

    expected = (
        (math.pi * 0.30)
        + (math.pi * 0.30 * 2)
    ) / 2

    assert math.isclose(average, expected)
