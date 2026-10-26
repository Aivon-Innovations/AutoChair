from dataclasses import dataclass
from enum import Enum


class InputSource(str, Enum):
    JOYSTICK = "JOYSTICK"
    TOUCHSCREEN = "TOUCHSCREEN"
    VOICE = "VOICE"


@dataclass
class InputCommand:
    source: InputSource
    command: str
