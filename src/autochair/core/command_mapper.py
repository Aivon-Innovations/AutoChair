from autochair.input.command import InputCommand
from autochair.motion.command import Direction, MotionCommand


class CommandMapper:
    def map(self, input_command: InputCommand) -> MotionCommand | None:
        command = input_command.command

        if command == "FORWARD":
            return MotionCommand(
                direction=Direction.FORWARD,
                speed_kmh=0.5,
            )

        if command == "REVERSE":
            return MotionCommand(
                direction=Direction.REVERSE,
                speed_kmh=0.5,
            )

        if command == "LEFT":
            return MotionCommand(
                direction=Direction.LEFT,
                speed_kmh=0.5,
            )

        if command == "RIGHT":
            return MotionCommand(
                direction=Direction.RIGHT,
                speed_kmh=0.5,
            )

        if command == "STOP":
            return MotionCommand(
                direction=Direction.STOP,
                speed_kmh=0.0,
            )

        return None
