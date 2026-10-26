from autochair.motion.command import MotionCommand


MAX_SPEED_KMH = 6.0


class MotionCommandValidator:
    def validate(self, command: MotionCommand) -> None:
        if command.speed_kmh < 0:
            raise ValueError("Speed cannot be negative.")

        if command.speed_kmh > MAX_SPEED_KMH:
            raise ValueError(
                f"Speed cannot exceed {MAX_SPEED_KMH} km/h."
            )
