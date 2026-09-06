from dataclasses import dataclass
from enum import Enum


class Direction(str, Enum):
    STOP = "STOP"
    FORWARD = "FORWARD"
    REVERSE = "REVERSE"
    LEFT = "LEFT"
    RIGHT = "RIGHT"


@dataclass
class MotionCommand:
    direction: Direction = Direction.STOP
    speed_kmh: float = 0.0
