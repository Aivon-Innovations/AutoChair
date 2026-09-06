from autochair.core.command_mapper import CommandMapper
from autochair.input.command import InputCommand, InputSource
from autochair.motion.command import Direction


def test_forward_mapping():
    mapper = CommandMapper()

    command = InputCommand(
        source=InputSource.JOYSTICK,
        command="FORWARD",
    )

    motion = mapper.map(command)

    assert motion is not None
    assert motion.direction == Direction.FORWARD
    assert motion.speed_kmh == 0.5


def test_stop_mapping():
    mapper = CommandMapper()

    command = InputCommand(
        source=InputSource.TOUCHSCREEN,
        command="STOP",
    )

    motion = mapper.map(command)

    assert motion is not None
    assert motion.direction == Direction.STOP
    assert motion.speed_kmh == 0.0


def test_navigation_command_not_mapped_yet():
    mapper = CommandMapper()

    command = InputCommand(
        source=InputSource.VOICE,
        command="GO_TO_KITCHEN",
    )

    motion = mapper.map(command)

    assert motion is None
