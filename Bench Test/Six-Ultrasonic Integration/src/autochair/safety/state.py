from enum import Enum


class SafetyState(str, Enum):
    CLEAR = "CLEAR"
    WARNING = "WARNING"
    STOP_REQUIRED = "STOP_REQUIRED"


class SafetyStatus:
    def __init__(self):
        self.state = SafetyState.CLEAR

    def set_state(self, state: SafetyState) -> None:
        self.state = state

    def is_clear(self) -> bool:
        return self.state == SafetyState.CLEAR

    def requires_stop(self) -> bool:
        return self.state == SafetyState.STOP_REQUIRED
