"""Exact gd32f470 integration metadata; CMake owns sources."""
from tools.configure.providers.descriptor import FamilyDescriptor

DESCRIPTOR = FamilyDescriptor(
    family="gd32f470",
    vendor="gd32",
    source_directory="soc/gd32/gd32f470",
    configuration_directory="soc/gd32/gd32f470/configure",
    git_modules=(
        ("cmsis_core", "cmsis_core"),
    ),
    import_lock="vendors/gigadevice/gd32f4xx/source.lock.json",
    references="references.json",
    owned_payload=(
        "soc/gd32/gd32f470/private/gd32f470_can_cold.h",
        "soc/gd32/gd32f470/private/gd32f470_timer_cold.h",
        "soc/gd32/gd32f470/private/gd32f470_packet_cold.h",
        "soc/gd32/gd32f470/private/gd32f470_usb_cold.h",
        "soc/gd32/gd32f470/private/gd32f470_eth_cold.h",
        "soc/gd32/gd32f470/private/gd32f470_eth_frames_cold.h",
        "soc/gd32/gd32f470/drivers/platform_cold.c",
        "soc/gd32/gd32f470/private/gd32f470_platform_cold.h",
        "soc/gd32/gd32f470/drivers/uart_cold.c",
        "soc/gd32/gd32f470/private/gd32f470_uart_cold.h",
        "soc/gd32/gd32f470/private/gd32f470_control_cold.h",
        "soc/gd32/gd32f470/private/gd32f470_spi_cold.h",
        "soc/gd32/gd32f470/drivers/i2c_cold.c",
        "soc/gd32/gd32f470/private/gd32f470_i2c_cold.h",
        "soc/gd32/gd32f470/private/gd32f470_cold_pins.h",
        "soc/gd32/gd32f470/drivers/ethernet_frames.c",
        "soc/gd32/gd32f470/private/gd32f470_eth_frames.h",
    ),
    configuration_helpers=(
        "soc/gd32/gd32f470/configure/integration.py",
        "soc/gd32/gd32f470/configure/hardware.py",
        "soc/gd32/gd32f470/configure/uart.py",
        "soc/gd32/gd32f470/configure/control.py",
        "soc/gd32/gd32f470/configure/preflight.py",
        "soc/gd32/gd32f470/configure/gpio.py",
        "soc/gd32/gd32f470/configure/can.py",
        "soc/gd32/gd32f470/configure/timer.py",
        "soc/gd32/gd32f470/configure/packet.py",
    ),
)
