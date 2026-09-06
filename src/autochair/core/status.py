from enum import Enum


class SystemStatus(str, Enum):
    SIMULATED = "SIMULATED"
    HARDWARE_INTEGRATED = "HARDWARE_INTEGRATED"
    TESTED_VALIDATED = "TESTED_VALIDATED"


class SystemState:
    def __init__(self):
        self.status = SystemStatus.SIMULATED
        self.running = False
        self.fault = False

    def start(self):
        self.running = True

    def stop(self):
        self.running = False

    def set_fault(self, fault: bool):
        self.fault = fault
