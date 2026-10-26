from autochair.input.command import InputCommand, InputSource
from autochair.input.validator import InputCommandValidator


def test_valid_input_command():
    validator = InputCommandValidator()

    command = InputCommand(
        source=InputSource.JOYSTICK,
        command="FORWARD",
    )

    validator.validate(command)


def test_invalid_input_command():
    validator = InputCommandValidator()

    command = InputCommand(
        source=InputSource.VOICE,
        command="FLY_TO_MOON",
    )

    try:
        validator.validate(command)
    except ValueError:
        return

    raise AssertionError("Invalid command was not rejected")
