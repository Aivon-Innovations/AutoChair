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
