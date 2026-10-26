import pytest

from autochair.navigation.encoder_resolution import EncoderResolution


def test_default_encoder_resolution():
    resolution = EncoderResolution()

    assert resolution.ppr == 600
    assert resolution.decoding_multiplier == 1
    assert resolution.counts_per_revolution == 600


def test_x2_decoding():
    resolution = EncoderResolution(
        ppr=600,
        decoding_multiplier=2,
    )

    assert resolution.counts_per_revolution == 1200


def test_x4_decoding():
    resolution = EncoderResolution(
        ppr=600,
        decoding_multiplier=4,
    )

    assert resolution.counts_per_revolution == 2400


def test_zero_ppr_is_rejected():
    with pytest.raises(ValueError):
        EncoderResolution(ppr=0)


def test_invalid_decoding_multiplier_is_rejected():
    with pytest.raises(ValueError):
        EncoderResolution(
            ppr=600,
            decoding_multiplier=3,
        )
