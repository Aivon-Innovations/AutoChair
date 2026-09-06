from autochair.motion.command import Direction, MotionCommand
from autochair.motion.validator import MotionCommandValidator


def test_valid_motion_command():
    validator = MotionCommandValidator()

    command = MotionCommand(
        direction=Direction.FORWARD,
        speed_kmh=0.5,
    )

    validator.validate(command)


def test_negative_speed_rejected():
    validator = MotionCommandValidator()

    command = MotionCommand(
        direction=Direction.FORWARD,
        speed_kmh=-1.0,
    )

    try:
        validator.validate(command)
    except ValueError:
        return

    raise AssertionError("Negative speed was not rejected")


def test_excessive_speed_rejected():
    validator = MotionCommandValidator()

    command = MotionCommand(
        direction=Direction.FORWARD,
        speed_kmh=7.0,
    )

    try:
        validator.validate(command)
    except ValueError:
        return

    raise AssertionError("Excessive speed was not rejected")
