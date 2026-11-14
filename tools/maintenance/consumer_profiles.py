"""Reviewed source SDK consumer configurations and execution domains."""

from dataclasses import dataclass

CONSUMER_POLICY = "nexus-sdk-consumers-v29"
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
    ServiceConsumer("f407 smbus packet pec", "f407.toml", "arm-smbus", True,
                    "smbus"),
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
    ServiceConsumer("f405zg family", "f405zg.toml", "arm-family-f405zg", True,
                    "f4_family"),
    ServiceConsumer("f415zg family", "f415zg.toml", "arm-family-f415zg", True,
                    "f4_family"),
    ServiceConsumer("f417zg family", "f417zg.toml", "arm-family-f417zg", True,
                    "f4_family"),
    ServiceConsumer("f411re family", "f411re.toml", "arm-family-f411re", True,
                    "f4_family"),
    ServiceConsumer("f446ze family", "f446ze.toml", "arm-family-f446ze", True,
                    "f4_family"),
    ServiceConsumer("f479ni family", "f479ni.toml", "arm-family-f479ni", True,
                    "f4_family"),
    *(ServiceConsumer(f"{part} power", f"{part}-provision-empty.toml",
                       f"arm-power-{part}", True, "f4_power")
      for part in ("f411re", "f446ze", "f479ni")),
    ServiceConsumer("f415zg crypto", "f415zg.toml", "arm-crypto-f415zg", True,
                    "f4_crypto"),
    ServiceConsumer("f417zg crypto", "f417zg.toml", "arm-crypto-f417zg", True,
                    "f4_crypto"),
    ServiceConsumer("f415zg crypto dma", "f415zg-dma.toml",
                    "arm-crypto-dma-f415zg", True, "f4_crypto"),
    ServiceConsumer("f417zg crypto dma", "f417zg-dma.toml",
                    "arm-crypto-dma-f417zg", True, "f4_crypto"),
    ServiceConsumer("f479ni sdram", "f479ni-sdram.toml", "arm-sdram-f479ni", True,
                    "f4_sdram"),
    ServiceConsumer("f479ni qspi", "f479ni-qspi.toml", "arm-qspi-f479ni", True,
                    "f4_qspi"),
    ServiceConsumer("f479ni qspi dma", "f479ni-qspi-dma.toml",
                    "arm-qspi-dma-f479ni", True, "f4_qspi"),
    ServiceConsumer("f479ni qspi quad", "f479ni-qspi-quad.toml",
                    "arm-qspi-quad-f479ni", True, "f4_qspi"),
    ServiceConsumer("f479ni qspi quad dma", "f479ni-qspi-quad-dma.toml",
                    "arm-qspi-quad-dma-f479ni", True, "f4_qspi"),
    ServiceConsumer("f479ni qspi mapped cpu", "f479ni-qspi-map.toml",
                    "arm-qspi-mapped-f479ni", True, "f4_qspi_mapped"),
    ServiceConsumer("f407 sd37 dma", "sd37-dma.toml",
                    "arm-sdio-hs-sd37", True, "f4_sdio_hs"),
    ServiceConsumer("f407 mmc37 irq", "mmc37-irq.toml",
                    "arm-sdio-hs-mmc37", True, "f4_sdio_hs"),
    ServiceConsumer("f407 capture frames", "ve-frames-raw.toml",
                    "arm-capture-frames-f407", True, "f4_capture_audio"),
    ServiceConsumer("f479ni graphics", "combined.toml", "arm-graphics-f479ni", True,
                    "f4_graphics"),
    ServiceConsumer("f479ni image", "image-only.toml", "arm-image-f479ni", True,
                    "f4_graphics"),
    ServiceConsumer("f479ni graphics sdram", "combined.toml",
                    "arm-graphics-sdram-f479ni", True, "f4_graphics_sdram"),
    *(ServiceConsumer(f"{part} sai {role}", f"{part}-{role}.toml",
                       f"arm-sai-{part}-{role}", True, "f4_sai")
      for part in ("f446ze", "f479ni") for role in ("master", "slave")),
    *(ServiceConsumer(f"{part} {slice_name} {role} blocks",
                       f"{part}-{slice_name}-{role}-blocks.toml",
                       f"arm-{slice_name}-{part}-{role}-blocks", True, "f4_sai")
      for part, slice_name in (("f446ze", "sai2"), ("f479ni", "sparse"))
      for role in ("master", "slave")),
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
