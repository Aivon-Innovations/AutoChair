from dataclasses import dataclass


@dataclass(frozen=True)
class EncoderResolution:
    ppr: int = 600
    decoding_multiplier: int = 1

    def __post_init__(self) -> None:
        if self.ppr <= 0:
            raise ValueError("PPR must be greater than zero.")

        if self.decoding_multiplier not in (1, 2, 4):
            raise ValueError(
                "Decoding multiplier must be 1, 2, or 4."
            )

    @property
    def counts_per_revolution(self) -> int:
        return self.ppr * self.decoding_multiplier
