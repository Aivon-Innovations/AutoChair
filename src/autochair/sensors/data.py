from dataclasses import dataclass


@dataclass
class UltrasonicData:
    front_left_cm: float = 999.0
    front_center_cm: float = 999.0
    front_right_cm: float = 999.0
    left_cm: float = 999.0
    right_cm: float = 999.0
    rear_cm: float = 999.0


@dataclass
class SensorSnapshot:
    ultrasonic: UltrasonicData
