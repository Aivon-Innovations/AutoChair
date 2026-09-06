from autochair.input.command import InputCommand


VALID_COMMANDS = {
    "FORWARD",
    "REVERSE",
    "LEFT",
    "RIGHT",
    "STOP",
    "GO_TO_KITCHEN",
}


class InputCommandValidator:
    def validate(self, command: InputCommand) -> None:
        if not command.command:
            raise ValueError("Command cannot be empty.")

        if command.command not in VALID_COMMANDS:
            raise ValueError(
                f"Unknown command: {command.command}"
            )
