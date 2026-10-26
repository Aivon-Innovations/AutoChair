import pytest

from autochair.navigation.wheel_geometry import WheelGeometry


def test_default_wheel_geometry():
    geometry = WheelGeometry()

    assert geometry.wheel_diameter_m == 0.30
    assert geometry.wheel_base_m == 0.50


def test_custom_wheel_geometry():
    geometry = WheelGeometry(
        wheel_diameter_m=0.32,
        wheel_base_m=0.55,
    )

    assert geometry.wheel_diameter_m == 0.32
    assert geometry.wheel_base_m == 0.55


def test_zero_wheel_diameter_rejected():
    with pytest.raises(ValueError):
        WheelGeometry(
            wheel_diameter_m=0.0,
            wheel_base_m=0.50,
        )


def test_negative_wheel_diameter_rejected():
    with pytest.raises(ValueError):
        WheelGeometry(
            wheel_diameter_m=-0.30,
            wheel_base_m=0.50,
        )


def test_zero_wheelbase_rejected():
    with pytest.raises(ValueError):
        WheelGeometry(
            wheel_diameter_m=0.30,
            wheel_base_m=0.0,
        )


def test_negative_wheelbase_rejected():
    with pytest.raises(ValueError):
        WheelGeometry(
            wheel_diameter_m=0.30,
            wheel_base_m=-0.50,
        )
