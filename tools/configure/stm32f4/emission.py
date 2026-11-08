"""Static provider wiring and family system lifecycle emission."""
from . import bindings, packet
from tools.configure.calls import Address, DirectCall, InitializeSequence
from tools.configure.providers.common import mode_traits
from tools.configure import can, timer_advanced, gpio_phase


def implementation(item):
    from .contract import mode_implementation
    return mode_implementation(item.kind, item.mode)


def operations(item):
    """Name the actual readonly table; shared algorithms keep one identity."""
    selected = implementation(item)
    if item.interface_kind == "spi_dma_transaction":
        return "nx_spi_dma_sequence_port_ops"
    if item.kind in {"eth", "eth_frames"}:
        return f"nx_{item.kind}_mac_ops"
    if item.kind == "can":
        return "nx_classic_can_ops"
    if item.kind == "usb_device":
        return "nx_usb_fs_device_ops"
    if item.kind == "external_bus":
        return "nx_stm32_fsmc_external_bus_ops"
    if selected == "i2c_async":
        return "nx_i2c_v1_async_ops"
    return f"nx_stm32_{selected}_ops"


def includes(result):
    selections = result.controllers
    source = []
    source.append('#include "stm32f407_provider.h"')
    source.append('#include "stm32f407_platform_cold.h"')
    if any(item.kind == "dac" for item in selections):
        source.append('#include "stm32f407_dac.h"')
    source.append('#include "stm32f4_platform_lifecycle_impl.h"')
    if any(item.kind in {"exti", "pwm", "adc"} for item in selections):
        source.append('#include "stm32f407_control_cold.h"')
    if any(item.kind in {"gpio", "flash", "watchdog"} for item in selections):
        source.append('#include "stm32f407_basic_cold.h"')
    if any(item.kind == "spi" for item in selections):
        source.append('#include "stm32f407_spi_cold.h"')
    if any(item.kind == "i2c" for item in selections):
        source.append('#include "stm32f407_i2c_cold.h"')
    if any(item.kind == "uart" for item in selections):
        source.append('#include "stm32f407_uart_cold.h"')
    if any(implementation(item) == "uart_hal" for item in selections):
        source.append('#include "stm32f407_uart_hal.h"')
    if any(item.mode == "irq-words" for item in selections):
        source.append('#include "stm32f407_uart_word.h"')
    if any(item.interface_kind == "spi_dma_transaction" for item in selections):
        source += ['#include "spi_dma_sequence.h"',
                   'extern const nx_spi_dma_sequence_backend_t',
                   '    nx_stm32_spi_dma_sequence_backend;']
    if any(implementation(item) in {"uart_dma", "spi_dma"}
           for item in selections):
        source.append('#include "stm32f407_dma.h"')
    if any(mode_traits(item.kind, item.mode).block_stream
           for item in selections):
        source.append('#include "stm32f407_stream.h"')
    if any(item.kind == "uart" and mode_traits(item.kind, item.mode).rx_dma
           for item in selections):
        source.append('#include "stm32f407_uart_receive.h"')
    if any(item.kind == "i2c" and mode_traits(item.kind, item.mode).irq
           for item in selections):
        source.append('#include "stm32f407_i2c_async.h"')
    if any(item.kind in timer_advanced.KINDS for item in selections):
        source.append('#include "stm32f407_timer_cold.h"')
    if any(item.kind == "eth" for item in selections):
        source.append('#include "stm32f407_eth_cold.h"')
    if any(item.kind == "eth_frames" for item in selections):
        source.append('#include "stm32f407_eth_frames_cold.h"')
    if any(item.kind == "can" for item in selections):
        source.append('#include "stm32f407_can_cold.h"')
    if any(item.kind == "usb_device" for item in selections):
        source.append('#include "stm32f407_usb_cold.h"')
    if any(item.kind == "block" for item in selections):
        source.append('#include "stm32f407_sdio.h"')
    if any(item.kind == "rtc" for item in selections):
        source.append('#include "stm32f407_rtc.h"')
    if any(item.kind == "external_bus" for item in selections):
        source.append('#include "stm32f407_fsmc.h"')
    source += ['void _init(void);', 'void _fini(void);',
               r'/** \brief C runtime hooks; constructors use .init_array. */',
               'void _init(void) {}', 'void _fini(void) {}']
    return source


def instance_declarations(item, result):
    source = []
    name = item.id.replace("-", "_")
    kind = item.kind
    if kind in {"spi", "i2c"}:
        initializer = ""
        if kind == "i2c":
            child = next((device for device in result.devices if device.controller == item.id), None)
            if child is not None:
                initializer = f" = {{ .port = &s_nx_port_{name}, .address = {child.options.address}u }}"
        source.append('static nx_stm32'
                      f"_{implementation(item)}_endpoint_state_t "
                      f"s_nx_endpoint_{name}{initializer};")
    return source


def has_dma(result):
    return any(mode_traits(item.kind, item.mode).dma
               for item in result.controllers)


def _dma_clock_mask(result):
    controllers = {dma.split(":", 1)[0]
                   for item in result.controllers for dma in item.dma}
    return " | ".join(f"RCC_AHB1ENR_{name}EN" for name in sorted(controllers))


def state_declarations(result):
    selections = result.controllers
    source = []
    if has_dma(result):
        source.append("static uint32_t s_nx_platform_dma_clocks_added;")
    if any(item.kind == "exti" for item in selections):
        source.append('static uint32_t s_nx_platform_syscfg_clock_added;')
    source.extend(can.state_declarations(result, "stm32"))
    source.extend(packet.state_declarations(result))
    from .sram import declarations
    source.extend(declarations(result))
    return source


def shared_irqs(result):
    return bindings.shared_irq_definitions(result.controllers)


def platform_declarations(result):
    """Project readonly owner connections; C owns startup and withdrawal."""
    source = []
    boot_owners = {child.controller for child in result.devices
                   if child.driver == "memory_window-endpoint" and
                   child.options.mode == "startup-memory"}
    permanent = "NULL"
    if boot_owners:
        if len(boot_owners) != 1:
            raise ValueError("F4 startup memory requires one permanent owner")
        owner = next(iter(boot_owners)).replace("-", "_")
        sequence = InitializeSequence("nx_platform_permanent_stop", (
            DirectCall("nx_stm32_fsmc_stop", (Address("s_nx_port_" + owner),)),))
        source += sequence.render()
        permanent = sequence.function
    can_close = "NULL"
    if result.can_domain is not None:
        sequence = InitializeSequence("nx_platform_can_close", (
            DirectCall("nx_stm32_can_domain_close", (Address("s_nx_can_domain"),)),))
        source += sequence.render()
        can_close = sequence.function
    ethernet = any(item.kind in {"eth", "eth_frames"}
                   for item in result.controllers)
    frames = any(item.kind == "eth_frames" for item in result.controllers)
    pin_owner = "NULL"
    if gpio_phase.applies(result):
        source += [
            'static const nx_stm32f4_platform_pin_owner_t s_nx_platform_pin_owner = {',
            '    .preflight = nx_stm32_platform_selected_preflight,',
            '    .acquire = nx_stm32_platform_pins_acquire,',
            '    .release_clocks = nx_stm32_platform_gpio_clocks_release,',
            '    .abort = nx_stm32_platform_pins_abort,',
            '    .release = nx_stm32_platform_pins_release', '};']
        pin_owner = "&s_nx_platform_pin_owner"
    selected = "&s_nx_platform_config" if pin_owner != "NULL" else "NULL"
    pins = "&s_nx_platform_pins" if pin_owner != "NULL" else "NULL"
    dma, syscfg = _resource_slots(result)
    clock_owner = "NULL"
    if dma != "NULL" or syscfg != "NULL":
        source += [
            'static const nx_stm32f4_platform_clock_owner_t s_nx_platform_clock_owner = {',
            '    .acquire = nx_stm32_platform_resources_acquire,',
            '    .release = nx_stm32_platform_resources_release', '};']
        clock_owner = "&s_nx_platform_clock_owner"
    source += [
        'static const nx_stm32f4_platform_config_t s_nx_platform_lifecycle_config = {',
        '    .system = &g_nx_stm32_system,',
        f'    .selected = {selected}, .pins = {pins},',
        f'    .pin_owner = {pin_owner},',
        f'    .clock_owner = {clock_owner},',
        f'    .flash_bytes = {result.flash["size"]}u, .poll_limit = 1000000u,',
        f'    .dma_mask = {_dma_clock_mask(result) or "0u"},',
        f'    .dma_added = {dma}, .syscfg_added = {syscfg},',
        f'    .permanent_stop = {permanent}, .can_close = {can_close},',
        '    .ethernet = ' + ('&s_nx_eth_resources,' if ethernet else 'NULL,'),
        '    .ethernet_capture = ' + ('nx_stm32_eth_resources_capture,' if ethernet else 'NULL,'),
        '    .ethernet_close = ' + ('nx_stm32_eth_resources_close,' if ethernet else 'NULL,'),
        f'    .ethernet_frames = {str(frames).lower()}', '};']
    return source


def platform_system_start(result):
    """Connect one concrete C startup transaction."""
    del result
    return DirectCall("nx_stm32f4_platform_system_start_local", (
        Address("s_nx_platform_lifecycle_config"),))


def platform_resources_acquire(result):
    """Connect shared clock and packet capture without emitted recovery."""
    del result
    return DirectCall("nx_stm32f4_platform_resources_acquire_local", (
        Address("s_nx_platform_lifecycle_config"),))


def platform_resources_release(result):
    """Connect fixed C permanent/domain/pad/shared-clock withdrawal."""
    del result
    return DirectCall("nx_stm32f4_platform_resources_release_local", (
        Address("s_nx_platform_lifecycle_config"),))


def platform_system_release(result, started):
    """Connect the same system owner with the engine's actual receipt flag."""
    del result
    return DirectCall("nx_stm32f4_platform_system_release_local", (
        Address("s_nx_platform_lifecycle_config"), started))


def _resource_slots(result):
    """Name only selected receipt storage; absent resources allocate nothing."""
    dma = '&s_nx_platform_dma_clocks_added' if has_dma(result) else 'NULL'
    syscfg = ('&s_nx_platform_syscfg_clock_added' if
              any(item.kind == 'exti' for item in result.controllers) else 'NULL')
    return dma, syscfg
