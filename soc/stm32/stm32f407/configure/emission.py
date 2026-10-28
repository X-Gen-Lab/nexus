"""Static provider wiring and family system lifecycle emission."""
from . import bindings
from tools.configure.providers.common import mode_traits
from tools.configure import can, usb_device, ethernet, timer_advanced, secondary_controllers, preflight, gpio_phase


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
    if selected == "i2c_async":
        return "nx_i2c_v1_async_ops"
    return f"nx_stm32_{selected}_ops"


def includes(result):
    selections = result.controllers
    source = []
    source.append('#include "stm32f407_provider.h"')
    if any(item.kind in {"gpio", "flash", "watchdog"} for item in selections):
        source.append('#include "stm32f407_basic_cold.h"')
    if any(item.kind == "spi" for item in selections):
        source.append('#include "stm32f407_spi_cold.h"')
    if any(item.kind == "i2c" for item in selections):
        source.append('#include "stm32f407_i2c_cold.h"')
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
        source.append('#include "stm32f407_timer_advanced.h"')
    if any(item.kind == "eth" for item in selections):
        source.append('#include "stm32f407_eth.h"')
    if any(item.kind == "eth_frames" for item in selections):
        source.append('#include "stm32f407_eth_frames.h"')
    if any(item.kind == "can" for item in selections):
        source.append('#include "stm32f407_can.h"')
    if any(item.kind == "usb_device" for item in selections):
        source.append('#include "stm32f407_usb_device.h"')
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
    source.extend(ethernet.state_declarations(result, "stm32"))
    source.extend(timer_advanced.state_declarations(result, "stm32"))
    source.extend(secondary_controllers.state_declarations(result, "stm32"))
    return source


def shared_irqs(result):
    return bindings.shared_irq_definitions(result.controllers)


def stop_system(result, *, system_phase=None):
    selections = result.controllers
    dma_clock = has_dma(result)
    source = can.stop_domain(result, "stm32")
    source.extend(ethernet.stop_system(result, "stm32"))
    source.extend(timer_advanced.stop_system(result, "stm32"))
    source.extend(secondary_controllers.stop_system(result, "stm32"))
    source += gpio_phase.release(result)
    if any(item.kind == "exti" for item in selections):
        source += ['    if (s_nx_platform_started) { RCC->APB2ENR &= ~s_nx_platform_syscfg_clock_added; }']
    if dma_clock:
        source += ['    if (s_nx_platform_started) {',
                   '        NX_PLATFORM_REGISTER_WRITE(RCC->AHB1ENR,',
                   '            RCC->AHB1ENR & ~s_nx_platform_dma_clocks_added);',
                   '        nx_arch_dsb();',
                   '        if ((RCC->AHB1ENR & s_nx_platform_dma_clocks_added) != 0u) {',
                   '            return NX_ERROR_IO;', '        }',
                   '        s_nx_platform_dma_clocks_added = 0u;',
                   '    }']
    source.append('    nx_arch_dsb();')
    if system_phase is not None:
        source.append(f'    {system_phase};')
    source += ['    if (s_nx_platform_started && nx_stm32_system_stop(&g_nx_stm32_system, 1000000u) != 0) {',
               '        return NX_ERROR_IO;', '    }']
    return source


def start_system(result):
    selections = result.controllers
    dma_clock = has_dma(result)
    source = preflight.emit(result)
    if dma_clock:
        source.append('    s_nx_platform_dma_clocks_added = 0u;')
    if any(item.kind == "exti" for item in selections):
        source.append('    s_nx_platform_syscfg_clock_added = 0u;')
    source += [f'    result.primary = nx_stm32_validate_identity({result.flash["size"]}u);',
               '    if (result.primary != NX_SUCCESS) { return result; }']
    source += gpio_phase.acquire(result)
    source += ['    nx_stm32_start_result_t system = nx_stm32_system_start(&g_nx_stm32_system, 1000000u);',
               '    if (system.primary != 0) {',
               '        result.primary = NX_ERROR_IO;',
               '        result.cleanup = system.cleanup == 0 ? NX_SUCCESS : NX_ERROR_IO;',
               '        result.remaining_effects = system.remaining_effects;']
    source += gpio_phase.abort(result)
    source += ['        s_nx_platform_started = result.remaining_effects != 0u;',
               '        return result;', '    }']
    return source


def start_resources(result, *, cleanup="nx_platform_stop()"):
    del cleanup
    selections = result.controllers
    source = ethernet.start_resources(result, "stm32")
    source.extend(timer_advanced.start_resources(result, "stm32"))
    source.extend(secondary_controllers.start_resources(result, "stm32"))
    mask = _dma_clock_mask(result)
    if mask:
        source += [f'    s_nx_platform_dma_clocks_added = ~RCC->AHB1ENR & ({mask});',
                   f'    RCC->AHB1ENR |= ({mask});',
                   '    (void)RCC->AHB1ENR;', '    nx_arch_dsb();']
    if any(item.kind == "exti" for item in selections):
        source.append('    s_nx_platform_syscfg_clock_added = ~RCC->APB2ENR & RCC_APB2ENR_SYSCFGEN;')
    return source
