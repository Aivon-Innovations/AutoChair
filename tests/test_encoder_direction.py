import pytest

from autochair.navigation.encoder_direction import EncoderDirection


def test_default_encoder_direction():
    direction = EncoderDirection()

    assert direction.left_sign == 1
    assert direction.right_sign == 1


def test_default_direction_keeps_ticks_unchanged():
    direction = EncoderDirection()

    left_ticks, right_ticks = direction.apply(
        100,
        200,
    )

    assert left_ticks == 100
    assert right_ticks == 200


def test_left_encoder_can_be_inverted():
    direction = EncoderDirection(
        left_sign=-1,
        right_sign=1,
    )

    left_ticks, right_ticks = direction.apply(
        100,
        200,
    )

    assert left_ticks == -100
    assert right_ticks == 200


def test_right_encoder_can_be_inverted():
    direction = EncoderDirection(
        left_sign=1,
        right_sign=-1,
    )

    left_ticks, right_ticks = direction.apply(
        100,
        200,
    )

    assert left_ticks == 100
    assert right_ticks == -200


def test_both_encoders_can_be_inverted():
    direction = EncoderDirection(
        left_sign=-1,
        right_sign=-1,
    )

    left_ticks, right_ticks = direction.apply(
        100,
        200,
    )

    assert left_ticks == -100
    assert right_ticks == -200


def test_invalid_left_sign_is_rejected():
    with pytest.raises(ValueError):
        EncoderDirection(left_sign=0)


def test_invalid_right_sign_is_rejected():
    with pytest.raises(ValueError):
        EncoderDirection(right_sign=0)
