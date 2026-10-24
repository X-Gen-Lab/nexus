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
    configuration_helpers: tuple = ()

    @property
    def source_directory(self):
        """Physical layout only; maintained() independently admits a family."""
        if self.family == "native":
            return "soc/native"
        for manufacturer in ("stm32", "gd32"):
            if self.family.startswith(manufacturer):
                return f"soc/{manufacturer}/{self.family}"
        fail(f"No source directory for maintained family: {self.family}")

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
        modules = tuple(f"{directory}/{name}" for name in (
            "__init__.py", "contract.py", "bindings.py", "emission.py"))
        return modules + self.configuration_helpers

    @property
    def fact_files(self):
        directory = self.source_directory
        names = ("soc.json", "routes.json")
        if self.references is not None:
            names += (self.references,)
        return tuple(f"{directory}/{name}" for name in names)


_DESCRIPTORS = MappingProxyType({
    "native": FamilyDescriptor("native", owned_payload=(
        "soc/native/CMakeLists.txt",
        'io/include/nexus/io/rtc.h',
        'io/include/nexus/io/lptim.h',
        'io/include/nexus/io/native/rtc_lptim_model.h',
        'soc/native/drivers/rtc_lptim_provider.h',
        'soc/native/drivers/rtc_lptim.c',
        'io/private/rtc_lptim_internal.h',
        'io/src/api/rtc.c',
        'io/src/api/lptim.c',
        'boards/native_reference/board.json',
        'cmake/package/verification/io_evolution/native-time-pair.toml',
        'cmake/package/verification/io_evolution/native-time-empty.toml',
        'cmake/package/verification/io_evolution/consumers/native-time-pair.c',
        'cmake/package/verification/io_evolution/consumers/native-time-empty.c',
    )),
    "stm32f103": FamilyDescriptor("stm32f103", (
        ("cmsis_core", "cmsis5"),
        ("cmsis_device_f1", "stm32f1_cmsis"),
        ("hal_driver_f1", "stm32f1_hal")), references="references.json",
        configuration_helpers=("tools/configure/providers/legacy_transport.py",),
        owned_payload=(
            "soc/stm32/stm32f103/CMakeLists.txt", "soc/stm32/stm32f103/system.c",
            "soc/stm32/stm32f103/drivers/gpio.c", "soc/stm32/stm32f103/drivers/uart.c",
            "soc/stm32/stm32f103/drivers/watchdog.c",
            "soc/stm32/stm32f103/drivers/exti.c",
            "soc/stm32/stm32f103/drivers/spi.c",
            "soc/stm32/stm32f103/drivers/uart_dma.c",
            "soc/stm32/stm32f103/drivers/i2c_async.c",
            "soc/stm32/stm32f103/private/stm32f103_transport.h",
            "soc/common/uart_v1_channel_dma.c",
            "soc/common/uart_v1_channel_dma.h",
            "soc/common/i2c_v1_async.c", "soc/common/i2c_v1_async.h",
            "soc/stm32/stm32f103/drivers/devices.c",
            "soc/stm32/stm32f103/private/stm32f103_devices.h",
            "soc/common/legacy_devices.h", "soc/common/legacy_adc.c",
            "soc/common/legacy_pwm.c", "soc/common/legacy_flash.c",
            "soc/stm32/stm32f103/private/stm32f103_system.h",
            "soc/stm32/stm32f103/private/stm32f103_provider.h",
            "boards/stm32f103_reference/board.json")),
    "stm32f407": FamilyDescriptor("stm32f407", (
        ("cmsis_core", "cmsis5"),
        ("cmsis_device_f4", "stm32f4_cmsis"),
        ("hal_driver_f4", "stm32f4_hal")),
        references="references.json", owned_payload=(
            "components/soc/stm32/f407_i2s2_slave_tx/CMakeLists.txt",
            "components/soc/stm32/f407_i2s2_slave_tx/include/nexus/components/f407_i2s2_slave_tx.h",
            "soc/stm32/stm32f407/drivers/i2s2_slave_tx.c",
            "soc/stm32/stm32f407/private/stm32f407_i2s2_slave_tx.h",
            "tools/configure/assemblies/sky-i2s2-slave-tx.toml",
            "tools/configure/assemblies/qiming-i2s2-slave-tx.toml",
            "cmake/package/verification/io_evolution/consumers/f407-i2s2-slave-tx.c",
            "soc/stm32/stm32f407/drivers/ethernet_frames.c",
            "soc/stm32/stm32f407/private/stm32f407_eth_frames.h")),
    "gd32f470": FamilyDescriptor("gd32f470",
        import_lock="vendors/gigadevice/gd32f4xx/source.lock.json",
        references="references.json", owned_payload=(
            "soc/gd32/gd32f470/drivers/ethernet_frames.c",
            "soc/gd32/gd32f470/private/gd32f470_eth_frames.h")),
    "gd32f303": FamilyDescriptor("gd32f303", (("cmsis_core", "cmsis5"),),
        import_lock="vendors/gigadevice/gd32f30x/source.lock.json",
        configuration_helpers=("tools/configure/providers/legacy_transport.py",),
        references="references.json", owned_payload=(
            "soc/gd32/gd32f303/CMakeLists.txt", "soc/gd32/gd32f303/system.c",
            "soc/gd32/gd32f303/drivers/gpio.c", "soc/gd32/gd32f303/drivers/uart.c",
            "soc/gd32/gd32f303/drivers/spi.c", "soc/gd32/gd32f303/drivers/watchdog.c",
            "soc/gd32/gd32f303/drivers/uart_dma.c",
            "soc/gd32/gd32f303/drivers/i2c_async.c",
            "soc/gd32/gd32f303/private/gd32f303_transport.h",
            "soc/common/uart_v1_channel_dma.c",
            "soc/common/uart_v1_channel_dma.h",
            "soc/common/i2c_v1_async.c", "soc/common/i2c_v1_async.h",
            "soc/gd32/gd32f303/drivers/devices.c", "soc/gd32/gd32f303/drivers/exti.c",
            "soc/gd32/gd32f303/private/gd32f303_devices.h",
            "soc/gd32/gd32f303/private/gd32f303_exti.h",
            "soc/common/legacy_devices.h", "soc/common/legacy_adc.c",
            "soc/common/legacy_pwm.c", "soc/common/legacy_flash.c",
            "soc/gd32/gd32f303/private/gd32f303_system.h",
            "soc/gd32/gd32f303/private/gd32f303_provider.h",
            "soc/gd32/gd32f303/private/gd32f30x_libopt.h",
            "boards/gd32f303_reference/board.json",
            "vendors/gigadevice/gd32f30x/source.lock.json",
            "vendors/gigadevice/gd32f30x/README.md",
            "vendors/gigadevice/gd32f30x/LICENSES/Apache-2.0.txt",
            "vendors/gigadevice/gd32f30x/LICENSES/BSD-3-Clause.txt",
            "vendors/gigadevice/gd32f30x/LICENSES/GigaDevice-SLA.txt",
            "vendors/gigadevice/gd32f30x/Firmware/CMSIS/GD/GD32F30x/Include/gd32f30x.h",
            "vendors/gigadevice/gd32f30x/Firmware/CMSIS/GD/GD32F30x/Include/system_gd32f30x.h",
            "vendors/gigadevice/gd32f30x/Firmware/CMSIS/GD/GD32F30x/Source/GCC/startup_gd32f30x_hd.S",
            "vendors/gigadevice/gd32f30x/Firmware/GD32F30x_standard_peripheral/Include/gd32f30x_adc.h",
            "vendors/gigadevice/gd32f30x/Firmware/GD32F30x_standard_peripheral/Include/gd32f30x_dma.h",
            "vendors/gigadevice/gd32f30x/Firmware/GD32F30x_standard_peripheral/Include/gd32f30x_exti.h",
            "vendors/gigadevice/gd32f30x/Firmware/GD32F30x_standard_peripheral/Include/gd32f30x_i2c.h",
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
        ("cmsis_device_g4", "stm32g4_cmsis"),
        ("hal_driver_g4", "stm32g4_hal")), references="references.json",
        configuration_helpers=tuple("tools/configure/providers/stm32g474/" + name
                                    for name in ("coupled.py", "adc_group.py",
                                                 "pwm_shared.py")),
        owned_payload=(
            "soc/stm32/stm32g474/CMakeLists.txt", "soc/stm32/stm32g474/system.c",
            "soc/stm32/stm32g474/drivers/gpio.c", "soc/stm32/stm32g474/drivers/uart.c",
            "soc/stm32/stm32g474/drivers/pin_state.c",
            "soc/stm32/stm32g474/private/stm32g474_pin_state.h",
            "io/include/nexus/io/pin_state.h", "io/src/api/pin_state.c",
            "io/private/pin_state_impl.h",
            "soc/stm32/stm32g474/drivers/exti.c", "soc/stm32/stm32g474/drivers/spi.c",
            "soc/stm32/stm32g474/drivers/watchdog.c", "soc/stm32/stm32g474/drivers/pwm.c",
            "soc/stm32/stm32g474/drivers/pwm_group.c",
            "soc/stm32/stm32g474/drivers/pwm_shared.c",
            "soc/stm32/stm32g474/drivers/encoder.c",
            "soc/stm32/stm32g474/drivers/timer_capture.c",
            "soc/stm32/stm32g474/drivers/adc.c", "soc/stm32/stm32g474/drivers/adc_stream.c",
            "soc/stm32/stm32g474/drivers/adc_group.c",
            "soc/stm32/stm32g474/drivers/fdcan.c",
            "soc/stm32/stm32g474/drivers/pwm_adc.c",
            "soc/stm32/stm32g474/drivers/i2c.c", "soc/stm32/stm32g474/drivers/flash.c",
            "soc/stm32/stm32g474/private/stm32g474_acquisition.h",
            "soc/stm32/stm32g474/private/stm32g474_adc_group.h",
            "soc/stm32/stm32g474/private/stm32g474_fdcan.h",
            "soc/stm32/stm32g474/private/stm32g474_pwm_adc.h",
            "soc/stm32/stm32g474/private/stm32g474_pwm_group.h",
            "soc/stm32/stm32g474/private/stm32g474_pwm_shared.h",
            "soc/stm32/stm32g474/private/stm32g474_timer.h",
            "soc/stm32/stm32g474/private/stm32g474_system.h",
            "soc/stm32/stm32g474/private/stm32g474_provider.h",
            "soc/common/i2c_v2.c", "soc/common/i2c_v2.h",
            "boards/stm32g474re_hse24_reference/board.json")),
    "stm32h743": FamilyDescriptor("stm32h743", (
        ("cmsis_core", "cmsis5"),
        ("cmsis_device_h7", "stm32h7_cmsis")), owned_payload=(
            "tools/configure/assemblies/h743-spi-dma-runtime-clock.toml",
            "cmake/package/verification/io_evolution/consumers/h743-runtime-clock.c",
            "tools/configure/assemblies/h743-spi-dma.toml",
            "tools/configure/assemblies/h743-spi-dma-cache-on.toml",
            "tools/configure/assemblies/h743-spi-dma-uart-tx.toml",
            "tools/configure/assemblies/h743-spi-dma-uart-rx.toml",
            "cmake/package/verification/io_evolution/consumers/h743-spi-dma.c",
            "cmake/platform/H743FlashRam.cmake",
            "cmake/platform/H743StackUsage.cmake",
            "tools/measurement/h743_flash_ram.py",
            "tools/measurement/flash_execution.py",
            "tools/measurement/flash_ram_static.py",
            "tools/measurement/soc_costs.py",
            "tools/measurement/static_storage.py",
            "soc/stm32/stm32h743/CMakeLists.txt", "soc/stm32/stm32h743/system.c",
            "soc/stm32/stm32h743/clock.c",
            "soc/stm32/stm32h743/drivers/gpio.c", "soc/stm32/stm32h743/drivers/uart.c",
            "soc/stm32/stm32h743/drivers/uart_dma.c",
            "soc/stm32/stm32h743/drivers/uart_continuous.c",
            "soc/stm32/stm32h743/private/stm32h743_uart_continuous.h",
            "soc/stm32/stm32h743/drivers/uart_duplex.c",
            "soc/stm32/stm32h743/private/stm32h743_uart_duplex.h",
            "soc/stm32/stm32h743/private/stm32h743_uart_continuous_impl.h",
            "soc/stm32/stm32h743/private/stm32h743_uart_dma_parts.h",
            "tools/configure/assemblies/h743-uart-continuous.toml",
            "tools/configure/assemblies/h743-uart-continuous-cache-on.toml",
            "tools/configure/assemblies/h743-uart-duplex-continuous.toml",
            "tools/configure/assemblies/h743-uart-duplex-continuous-cache-on.toml",
            "cmake/package/verification/io_evolution/consumers/h743-uart-continuous.c",
            "cmake/package/verification/io_evolution/consumers/h743-uart-duplex-continuous.c",
            "soc/stm32/stm32h743/drivers/uart_receive.c",
            "soc/stm32/stm32h743/drivers/uart_fifo.c",
            "soc/stm32/stm32h743/drivers/uart_rtscts.c",
            "soc/stm32/stm32h743/drivers/spi.c",
            "soc/stm32/stm32h743/drivers/spi_dma.c",
            "soc/stm32/stm32h743/private/stm32h743_spi_dma.h",
            "soc/stm32/stm32h743/drivers/spi_wire.c",
            "soc/stm32/stm32h743/private/stm32h743_spi_wire.h",
            "soc/stm32/stm32h743/drivers/pwm.c",
            "soc/stm32/stm32h743/drivers/exti.c", "soc/stm32/stm32h743/drivers/watchdog.c",
            "soc/stm32/stm32h743/drivers/i2c.c", "soc/stm32/stm32h743/drivers/adc.c",
            "soc/stm32/stm32h743/drivers/flash.c",
            "soc/stm32/stm32h743/drivers/flash_execution.c",
            "soc/stm32/stm32h743/drivers/flash_ram.c",
            "soc/stm32/stm32h743/drivers/flash_ram_owner.c",
            "soc/stm32/stm32h743/drivers/flash_ram_transaction.c",
            "soc/stm32/stm32h743/drivers/flash_ram_enter.S",
            "soc/stm32/stm32h743/drivers/flash_mpu.c",
            "soc/stm32/stm32h743/private/stm32h743_flash_ram.h",
            "soc/stm32/stm32h743/private/stm32h743_flash_ram_layout.h",
            "soc/stm32/stm32h743/private/stm32h743_flash_mpu.h",
            "soc/stm32/stm32h743/memory_init.c",
            "soc/stm32/stm32h743/private/memory_init.h",
            "soc/common/i2c_v2.c", "soc/common/i2c_v2.h",
            "soc/stm32/stm32h743/private/stm32h743_dma.h",
            "soc/stm32/stm32h743/private/stm32h743_uart_receive.h",
            "soc/stm32/stm32h743/private/stm32h743_uart_fifo.h",
            "soc/stm32/stm32h743/private/stm32h743_uart_rtscts.h",
            "soc/stm32/stm32h743/private/stm32h743_flash_execution.h",
            "soc/stm32/stm32h743/private/stm32h743_system.h",
            "soc/stm32/stm32h743/private/stm32h743_clock.h",
            "soc/stm32/stm32h743/private/stm32h743_provider.h",
            "soc/stm32/stm32h743/power_boot.S",
            "soc/stm32/stm32h743/private/boot_registers.h",
            "boards/stm32h743_nucleo_zi2/board.json",
            "boards/stm32h743zi_hsi_reference/board.json",
            "boards/stm32h743zi_rtscts_reference/board.json")),
    "stm32u575": FamilyDescriptor("stm32u575", (
        ("cmsis_core", "cmsis5"),
        ("cmsis_device_u5", "stm32u5_cmsis"),
        ("hal_driver_u5", "stm32u5_hal")), owned_payload=(
            "soc/stm32/stm32u575/CMakeLists.txt", "soc/stm32/stm32u575/system.c",
            "soc/stm32/stm32u575/memory_init.c",
            "soc/stm32/stm32u575/private/stm32u575_memory_init.h",
            "soc/stm32/stm32u575/mpu_user_init.c",
            "soc/stm32/stm32u575/private/stm32u575_mpu_user_init.h",
            "soc/stm32/stm32u575/private/stm32u575_gpio_mpu.h",
            "tools/configure/assemblies/u575-mpu-gpio-write.toml",
            "cmake/package/verification/io_evolution/consumers/u575-mpu-gpio.c",
            "soc/stm32/stm32u575/drivers/gpio.c", "soc/stm32/stm32u575/drivers/uart.c",
            "soc/stm32/stm32u575/drivers/uart_dma.c",
            "soc/stm32/stm32u575/drivers/uart_dma_chain.c",
            "soc/stm32/stm32u575/private/stm32u575_uart_gpdma_impl.h",
            "soc/stm32/stm32u575/private/stm32u575_uart_gpdma_chain.h",
            "soc/stm32/stm32u575/drivers/rtc.c",
            "soc/stm32/stm32u575/private/stm32u575_rtc.h",
            "soc/stm32/stm32u575/private/stm32u575_rtc_calendar.h",
            "components/soc/stm32/stm32u575_retained_time/CMakeLists.txt",
            "components/soc/stm32/stm32u575_retained_time/include/nexus/components/stm32u575_retained_time.h",
            "components/soc/stm32/stm32u575_retained_time/private/retained_time.h",
            "components/soc/stm32/stm32u575_retained_time/src/stm32u575_retained_time.c",
            "cmake/package/verification/io_evolution/consumers/u575-retained-time.c",
            "components/soc/stm32/stm32u575_stop2/CMakeLists.txt",
            "components/soc/stm32/stm32u575_stop2/include/nexus/components/stm32u575_stop2.h",
            "soc/stm32/stm32u575/stop2.c",
            "soc/stm32/stm32u575/private/stm32u575_stop2_hw.h",
            "cmake/package/verification/io_evolution/consumers/u575-stop2.c",
            "components/soc/stm32/stm32u575_lpdma_copy/CMakeLists.txt",
            "components/soc/stm32/stm32u575_lpdma_copy/include/nexus/components/stm32u575_lpdma_copy.h",
            "components/soc/stm32/stm32u575_lpdma_copy/private/stm32u575_lpdma_copy_internal.h",
            "soc/stm32/stm32u575/drivers/lpdma_copy.c",
            "soc/stm32/stm32u575/private/stm32u575_lpdma_copy_hw.h",
            "components/soc/stm32/stm32u575_stop2/include/nexus/components/stm32u575_stop2_copy.h",
            "tools/configure/assemblies/u575-lpdma-copy-stop2.toml",
            "cmake/package/verification/io_evolution/consumers/u575-lpdma-copy.c",
            "components/soc/stm32/stm32u575_lpdma_lptim1/CMakeLists.txt",
            "components/soc/stm32/stm32u575_lpdma_lptim1/include/nexus/components/stm32u575_lpdma_lptim1.h",
            "components/soc/stm32/stm32u575_lpdma_lptim1/private/stm32u575_lpdma_lptim1_internal.h",
            "components/soc/stm32/stm32u575_stop2/include/nexus/components/stm32u575_stop2_lptim1.h",
            "soc/stm32/stm32u575/private/stm32u575_lptim1_profile.h",
            "soc/stm32/stm32u575/drivers/lpdma_lptim1.c",
            "tools/configure/assemblies/u575-lptim1-lpbam-stop2.toml",
            "cmake/package/verification/io_evolution/consumers/u575-lptim1-lpbam.c",
            "tools/configure/assemblies/u575-rtc-run.toml",
            "boards/stm32u575zi_rtc_reference/board.json",
            "cmake/package/verification/io_evolution/consumers/u575-rtc.c",
            "soc/stm32/stm32u575/drivers/lptim.c",
            "soc/stm32/stm32u575/private/stm32u575_lptim.h",
            "tools/configure/assemblies/u575-lptim-run.toml",
            "tools/configure/assemblies/u575-uart-chain.toml",
            "boards/stm32u575zi_lptim_reference/board.json",
            "cmake/package/verification/io_evolution/consumers/u575-lptim.c",
            "soc/stm32/stm32u575/drivers/uart_fifo.c",
            "soc/stm32/stm32u575/drivers/uart_rtscts.c",
            "soc/stm32/stm32u575/private/stm32u575_uart_gpdma.h",
            "soc/stm32/stm32u575/private/stm32u575_uart_fifo.h",
            "soc/stm32/stm32u575/private/stm32u575_uart_rtscts.h",
            "soc/stm32/stm32u575/drivers/watchdog.c",
            "soc/stm32/stm32u575/drivers/spi.c",
            "soc/stm32/stm32u575/drivers/i2c.c",
            "soc/stm32/stm32u575/drivers/adc.c",
            "soc/stm32/stm32u575/drivers/pwm.c",
            "soc/stm32/stm32u575/drivers/exti.c",
            "soc/stm32/stm32u575/drivers/flash.c",
            "soc/stm32/stm32u575/secure_batch.c",
            "soc/stm32/stm32u575/private/stm32u575_secure_batch.h",
            "soc/stm32/stm32u575/security.c",
            "soc/stm32/stm32u575/private/stm32u575_security.h",
            "soc/stm32/stm32u575/private/stm32u575_security_hw.h",
            "soc/common/i2c_v2.c", "soc/common/i2c_v2.h",
            "soc/stm32/stm32u575/private/stm32u575_system.h",
            "soc/stm32/stm32u575/private/stm32u575_provider.h",
            "boards/stm32u575_nucleo_zi_q/board.json",
            "boards/stm32u575zi_rtscts_reference/board.json")),
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


def source_directory(family):
    """Return the canonical layout only after exact descriptor admission."""
    return maintained(family).source_directory
