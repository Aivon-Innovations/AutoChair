from dataclasses import dataclass


@dataclass
class WheelGeometry:
    wheel_diameter_m: float = 0.30
    wheel_base_m: float = 0.50
