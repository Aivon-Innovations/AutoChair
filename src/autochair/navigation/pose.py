from dataclasses import dataclass


@dataclass
class Pose:
    x_m: float = 0.0
    y_m: float = 0.0
    heading_deg: float = 0.0
