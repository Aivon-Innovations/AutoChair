from autochair.config.settings import (
    APP_NAME,
    APP_VERSION,
    HARDWARE_STATUS,
)

from autochair.core.status import SystemState
from autochair.utils.logger import get_logger


logger = get_logger("autochair.core.main")


def main():
    system = SystemState()

    logger.info(f"{APP_NAME} v{APP_VERSION} starting")
    logger.info(f"Hardware status: {HARDWARE_STATUS}")
    logger.info(f"System state: {system.status.value}")
    logger.info(f"Running: {system.running}")
    logger.info(f"Fault: {system.fault}")


if __name__ == "__main__":
    main()
