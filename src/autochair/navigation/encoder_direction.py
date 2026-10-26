from dataclasses import dataclass


@dataclass(frozen=True)
class EncoderDirection:
    left_sign: int = 1
    right_sign: int = 1

    def __post_init__(self) -> None:
        if self.left_sign not in (-1, 1):
            raise ValueError("Left encoder sign must be 1 or -1.")

        if self.right_sign not in (-1, 1):
            raise ValueError("Right encoder sign must be 1 or -1.")

    def apply(
        self,
        left_ticks: int,
        right_ticks: int,
    ) -> tuple[int, int]:
        return (
            left_ticks * self.left_sign,
            right_ticks * self.right_sign,
        )
