"""Reviewed source SDK consumer configurations and execution domains."""

from dataclasses import dataclass

CONSUMER_POLICY = "nexus-sdk-consumers-v17"
BASE_CONSUMERS = (("native", False, "native"), ("stm32", True, "stm32"))
MODERN_CONSUMERS = ()
SELECTED_CONSUMERS = (
    ('f407 frames',
     'cmake/package/verification/io_evolution/f407zg-eth-frames.toml',
     'ethernet-frames'),
    ('native time pair',
     'cmake/package/verification/io_evolution/native-time-pair.toml',
     'native-time-pair'),
    ('native time empty',
     'cmake/package/verification/io_evolution/native-time-empty.toml',
     'native-time-empty'),
    ('f407 i2s2 slave tx',
     'tools/configure/assemblies/sky-i2s2-slave-tx.toml',
     'f407-i2s2-slave-tx'),
)


@dataclass(frozen=True)
class ServiceConsumer:
    """An exact additional consumer, with explicit execution/image domains."""

    name: str
    assembly: str
    domain: str
    arm: bool
    scenario: str = "common_services"


SERVICE_CONSUMERS = (
    ServiceConsumer("native services", "native.toml", "native-services", False),
    ServiceConsumer("f407 services", "f407.toml", "arm-services", True),
    ServiceConsumer("native secure services", "native/assembly.toml",
                    "native-secure", False, "secure_services"),
    ServiceConsumer("f407 secure services", "f407/assembly.toml",
                    "arm-secure", True, "secure_services"),
    ServiceConsumer("native tmp117", "native.toml", "native-device", False,
                    "device_tmp117"),
    ServiceConsumer("f407 tmp117", "f407.toml", "arm-device", True,
                    "device_tmp117"),
    ServiceConsumer("f407 image window", "assembly.toml", "arm-image", True,
                    "image_window"),
    ServiceConsumer("f407 loaned rx", "f407.toml", "arm-loaned-rx", True,
                    "loaned_rx"),
    ServiceConsumer("native mcuboot control", "native.toml", "native-boot-control",
                    False, "mcuboot_control"),
    ServiceConsumer("f407 mcuboot control", "f407.toml", "arm-boot-control",
                    True, "mcuboot_control"),
    ServiceConsumer("f407 mcuboot boot", "assembly.toml", "arm-boot", True,
                    "mcuboot_boot"),
)
NATIVE_TIME_SMOKES = frozenset({"native-time-pair", "native-time-empty"})


def selected_domain(specification):
    """Classify reviewed host-time or F4 consumers by their exact identity."""
    if tuple(specification) not in SELECTED_CONSUMERS:
        raise ValueError("Selected consumer differs from the reviewed plan")
    return ("native-time" if specification[2] in NATIVE_TIME_SMOKES
            else "arm-provider")


def consumer_plan():
    """The producer and transport consume this one ordered policy."""
    return (tuple(("base", item) for item in BASE_CONSUMERS)
            + tuple(("modern", item) for item in MODERN_CONSUMERS)
            + tuple(("selected", item) for item in SELECTED_CONSUMERS)
            + tuple(("services", item) for item in SERVICE_CONSUMERS))
