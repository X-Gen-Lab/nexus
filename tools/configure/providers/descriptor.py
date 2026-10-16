"""Explicit immutable family integration contracts, never source build graphs.

These descriptors select reviewed configuration modules and SDK identities.
CMake remains the sole owner of production translation units and dependencies.
No descriptor performs file discovery, downloads or runtime registration.
"""
from dataclasses import dataclass
import importlib.util
from pathlib import Path
from types import MappingProxyType

from .common import fail

try:
    from ...maintenance.source_catalogue import SOURCE_DEPENDENCIES
except ImportError:
    # The CLI also runs as a standalone script in a relocated Source SDK.
    # Load one exact maintained catalogue, never search an ambient sys.path.
    path = Path(__file__).resolve().parents[2] / "maintenance/source_catalogue.py"
    spec = importlib.util.spec_from_file_location("nexus_source_catalogue", path)
    if spec is None or spec.loader is None:
        fail("Missing maintained source dependency catalogue")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    SOURCE_DEPENDENCIES = module.SOURCE_DEPENDENCIES


@dataclass(frozen=True)
class FamilyDescriptor:
    """Inputs required by one maintained family, without CPU-family inference."""

    family: str
    git_modules: tuple = ()
    import_lock: str | None = None
    references: str | None = None
    owned_payload: tuple = ()

    @property
    def git_dependencies(self):
        sources = {item.identifier: item for item in SOURCE_DEPENDENCIES}
        if any(identifier not in sources for _, identifier in self.git_modules):
            fail("Unregistered family source dependency")
        return tuple((key, sources[identifier].path)
                     for key, identifier in self.git_modules)

    @property
    def configuration_modules(self):
        directory = f"tools/configure/providers/{self.family}"
        return tuple(f"{directory}/{name}" for name in (
            "__init__.py", "bindings.py", "emission.py"))

    @property
    def fact_files(self):
        directory = f"soc/{self.family}"
        names = ("soc.json", "routes.json")
        if self.references is not None:
            names += (self.references,)
        return tuple(f"{directory}/{name}" for name in names)


_DESCRIPTORS = MappingProxyType({
    "native": FamilyDescriptor("native"),
    "stm32f103": FamilyDescriptor("stm32f103", (
        ("cmsis_core", "cmsis5"),
        ("cmsis_device_f1", "stm32f1_cmsis"),
        ("hal_driver_f1", "stm32f1_hal")), references="references.json",
        owned_payload=(
            "soc/stm32f103/CMakeLists.txt", "soc/stm32f103/system.c",
            "soc/stm32f103/drivers/gpio.c", "soc/stm32f103/drivers/uart.c",
            "soc/stm32f103/drivers/watchdog.c",
            "soc/stm32f103/drivers/exti.c",
            "soc/stm32f103/drivers/spi.c",
            "soc/stm32f103/private/stm32f103_system.h",
            "soc/stm32f103/private/stm32f103_provider.h",
            "boards/stm32f103_reference/board.json")),
    "stm32f407": FamilyDescriptor("stm32f407", (
        ("cmsis_core", "cmsis5"),
        ("cmsis_device_f4", "stm32f4_cmsis"),
        ("hal_driver_f4", "stm32f4_hal")),
        references="references.json"),
    "gd32f470": FamilyDescriptor("gd32f470",
        import_lock="vendors/gigadevice/gd32f4xx/source.lock.json",
        references="references.json"),
    "gd32f303": FamilyDescriptor("gd32f303", (("cmsis_core", "cmsis5"),),
        import_lock="vendors/gigadevice/gd32f30x/source.lock.json",
        references="references.json", owned_payload=(
            "soc/gd32f303/CMakeLists.txt", "soc/gd32f303/system.c",
            "soc/gd32f303/drivers/gpio.c", "soc/gd32f303/drivers/uart.c",
            "soc/gd32f303/drivers/spi.c", "soc/gd32f303/drivers/watchdog.c",
            "soc/gd32f303/private/gd32f303_system.h",
            "soc/gd32f303/private/gd32f303_provider.h",
            "soc/gd32f303/private/gd32f30x_libopt.h",
            "boards/gd32f303_reference/board.json",
            "vendors/gigadevice/gd32f30x/source.lock.json",
            "vendors/gigadevice/gd32f30x/README.md",
            "vendors/gigadevice/gd32f30x/LICENSES/Apache-2.0.txt",
            "vendors/gigadevice/gd32f30x/LICENSES/BSD-3-Clause.txt",
            "vendors/gigadevice/gd32f30x/LICENSES/GigaDevice-SLA.txt",
            "vendors/gigadevice/gd32f30x/Firmware/CMSIS/GD/GD32F30x/Include/gd32f30x.h",
            "vendors/gigadevice/gd32f30x/Firmware/CMSIS/GD/GD32F30x/Include/system_gd32f30x.h",
            "vendors/gigadevice/gd32f30x/Firmware/CMSIS/GD/GD32F30x/Source/GCC/startup_gd32f30x_hd.S",
            "vendors/gigadevice/gd32f30x/Firmware/GD32F30x_standard_peripheral/Include/gd32f30x_gpio.h",
            "vendors/gigadevice/gd32f30x/Firmware/GD32F30x_standard_peripheral/Include/gd32f30x_usart.h",
            "vendors/gigadevice/gd32f30x/Firmware/GD32F30x_standard_peripheral/Include/gd32f30x_timer.h",
            "vendors/gigadevice/gd32f30x/Firmware/GD32F30x_standard_peripheral/Include/gd32f30x_rcu.h",
            "vendors/gigadevice/gd32f30x/Firmware/GD32F30x_standard_peripheral/Include/gd32f30x_fmc.h",
            "vendors/gigadevice/gd32f30x/Firmware/GD32F30x_standard_peripheral/Include/gd32f30x_spi.h",
            "vendors/gigadevice/gd32f30x/Firmware/GD32F30x_standard_peripheral/Include/gd32f30x_fwdgt.h",
            "vendors/gigadevice/gd32f30x/Firmware/GD32F30x_standard_peripheral/Include/gd32f30x_dbg.h")),
    "stm32g474": FamilyDescriptor("stm32g474", (
        ("cmsis_core", "cmsis5"),
        ("cmsis_device_g4", "stm32g4_cmsis")), references="references.json"),
    "stm32h743": FamilyDescriptor("stm32h743", (
        ("cmsis_core", "cmsis5"),
        ("cmsis_device_h7", "stm32h7_cmsis")), owned_payload=(
            "soc/stm32h743/CMakeLists.txt", "soc/stm32h743/system.c",
            "soc/stm32h743/drivers/gpio.c", "soc/stm32h743/drivers/uart.c",
            "soc/stm32h743/drivers/uart_dma.c",
            "soc/stm32h743/drivers/uart_receive.c",
            "soc/stm32h743/private/stm32h743_dma.h",
            "soc/stm32h743/private/stm32h743_uart_receive.h",
            "soc/stm32h743/private/stm32h743_system.h",
            "soc/stm32h743/private/stm32h743_provider.h",
            "soc/stm32h743/power_boot.S",
            "soc/stm32h743/private/boot_registers.h",
            "boards/stm32h743_nucleo_zi2/board.json")),
    "stm32u575": FamilyDescriptor("stm32u575", (
        ("cmsis_core", "cmsis5"),
        ("cmsis_device_u5", "stm32u5_cmsis")), owned_payload=(
            "soc/stm32u575/CMakeLists.txt", "soc/stm32u575/system.c",
            "soc/stm32u575/drivers/gpio.c", "soc/stm32u575/drivers/uart.c",
            "soc/stm32u575/drivers/watchdog.c",
            "soc/stm32u575/private/stm32u575_system.h",
            "soc/stm32u575/private/stm32u575_provider.h",
            "boards/stm32u575_nucleo_zi_q/board.json")),
})


def maintained(family):
    """Reject unknown families instead of inheriting another MCU's contract."""
    result = _DESCRIPTORS.get(family)
    if result is None:
        fail(f"Unmaintained SoC integration descriptor: {family}")
    return result


def families():
    """Return only explicitly maintained descriptors, in stable order."""
    return tuple(_DESCRIPTORS.values())
