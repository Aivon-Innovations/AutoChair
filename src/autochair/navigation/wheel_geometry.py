from dataclasses import dataclass


@dataclass
class WheelGeometry:
    wheel_diameter_m: float = 0.30
    wheel_base_m: float = 0.50

    def __post_init__(self) -> None:
        if self.wheel_diameter_m <= 0:
            raise ValueError(
                "Wheel diameter must be greater than zero."
            )

        if self.wheel_base_m <= 0:
            raise ValueError(
                "Wheelbase must be greater than zero."
            )
