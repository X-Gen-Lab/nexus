"""Static provider wiring and family system lifecycle emission."""
from . import bindings, packet
from tools.configure.providers.common import mode_traits
from tools.configure import can, usb_device, ethernet, timer_advanced, preflight, gpio_phase


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
    return f"nx_gd32_{selected}_ops"


def includes(result):
    selections = result.controllers
    source = []
    source += ['#include "gd32f470_provider.h"', '#include "gd32f4xx.h"',
               '#include "system.h"', '#include "gd32f470_platform_cold.h"']
    if any(item.kind in {"uart", "i2c"} for item in selections):
        source.append('#include "gd32f470_cold_pins.h"')
    if any(item.kind == "i2c" for item in selections):
        source.append('#include "gd32f470_i2c_cold.h"')
    if any(item.mode == "irq-words" for item in selections):
        source.append('#include "gd32f470_uart_word.h"')
    if any(item.interface_kind == "spi_dma_transaction" for item in selections):
        source += ['#include "spi_dma_sequence.h"',
                   'extern const nx_spi_dma_sequence_backend_t',
                   '    nx_gd32_spi_dma_sequence_backend;']
    if gpio_phase.applies(result):
        source += ['#include "gd32f4xx_gpio.h"', '#include "gd32f4xx_rcu.h"']
    if any(item.kind in {"pwm", "adc"} for item in selections):
        source.append('#include "gd32f470_control_cold.h"')
    if any(item.kind == "uart" for item in selections):
        source.append('#include "gd32f470_uart_cold.h"')
    if any(item.kind == "spi" for item in selections):
        source.append('#include "gd32f470_spi_cold.h"')
    if any(item.kind == "spi" and item.mode == "dma-full-duplex" for item in selections):
        source.append('#include "gd32f470_spi_dma.h"')
    if any(item.kind == "adc" and mode_traits(item.kind, item.mode).dma
           for item in selections):
        source.append('#include "gd32f470_adc_stream.h"')
    if any(item.kind == "uart" and item.mode == "irq-blocks" for item in selections):
        source.append('#include "gd32f470_uart_stream.h"')
    if any(item.kind == "uart" and mode_traits(item.kind, item.mode).rx_dma
           for item in selections):
        source.append('#include "gd32f470_uart_receive.h"')
    if any(item.kind == "i2c" and mode_traits(item.kind, item.mode).irq
           for item in selections):
        source.append('#include "gd32f470_i2c_async.h"')
    if any(item.kind in timer_advanced.KINDS for item in selections):
        source.append('#include "gd32f470_timer_cold.h"')
        source.append('#include "gd32f4xx_timer.h"')
        source.append('#include "gd32f4xx_gpio.h"')
    if any(item.kind == "eth" for item in selections):
        source.append('#include "gd32f470_eth_cold.h"')
    if any(item.kind == "eth_frames" for item in selections):
        source.append('#include "gd32f470_eth_frames_cold.h"')
        source.append('#include "gd32f4xx_syscfg.h"')
    if any(item.kind == "can" for item in selections):
        source.append('#include "gd32f470_can_cold.h"')
    if any(item.kind == "usb_device" for item in selections):
        source.append('#include "gd32f470_usb_cold.h"')
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
        source.append('static nx_gd32'
                      f"_{implementation(item)}_endpoint_state_t "
                      f"s_nx_endpoint_{name}{initializer};")
    return source


def has_dma(result):
    return any(mode_traits(item.kind, item.mode).dma
               for item in result.controllers)


def _dma_clock_mask(result):
    controllers = {dma.split(":", 1)[0]
                   for item in result.controllers for dma in item.dma}
    return " | ".join(f"RCU_AHB1EN_{name}EN" for name in sorted(controllers))


def state_declarations(result):
    source = []
    if has_dma(result):
        source.append("static uint32_t s_nx_platform_dma_clocks_added;")
    source.extend(can.state_declarations(result, "gd32"))
    source.extend(packet.state_declarations(result))
    return source


def shared_irqs(result):
    selected = sorted((item for item in result.controllers
                       if item.kind == "exti"),
                      key=lambda item: int(item.pins[0]["pin"][2:]))
    if not selected:
        return []
    pointers = ", ".join("&s_nx_port_" + item.id.replace("-", "_")
                         for item in selected)
    source = ["static nx_gd32_exti_state_t* const s_nx_exti_ports[] = {",
              "    " + pointers, "};",
              "static const nx_gd32_exti_domain_t s_nx_exti_domain = "
              f"{{ s_nx_exti_ports, {len(selected)}u }};"]
    vectors = {}
    for item in selected:
        line = int(item.pins[0]["pin"][2:])
        vector = (f"EXTI{line}" if line < 5 else
                  "EXTI5_9" if line < 10 else "EXTI10_15")
        vectors[vector] = vectors.get(vector, 0) | (1 << line)
    for vector, mask in vectors.items():
        source += [f"void {vector}_IRQHandler(void);",
                   r"/** \brief Dispatch only exact selected EXTI domain members. */",
                   f"void {vector}_IRQHandler(void) {{",
                   f"    nx_gd32_exti_dispatch(&s_nx_exti_domain, {mask}u);",
                   "}"]
    return source


def stop_system(result, *, system_phase=None):
    dma_clock = has_dma(result)
    source = (['    nx_result_t can_domain = nx_gd32_can_domain_close(&s_nx_can_domain);',
               '    if (can_domain != NX_SUCCESS) { return can_domain; }']
              if result.can_domain is not None else [])
    source.extend(packet.stop_system(result))
    source += gpio_phase.release(result)
    if dma_clock:
        source += ['    if (s_nx_platform_started) {',
                   '        nx_result_t dma = nx_gd32_platform_dma_release(&s_nx_platform_dma_clocks_added);',
                   '        if (dma != NX_SUCCESS) { return dma; }', '    }']
    if system_phase is not None:
        source.append(f'    {system_phase};')
    source += ['    if (s_nx_platform_started && nx_gd32_soc_stop() != 0) { return NX_ERROR_IO; }']
    return source


def start_system(result):
    dma_clock = has_dma(result)
    source = preflight.emit(result)
    if dma_clock:
        source.append('    s_nx_platform_dma_clocks_added = 0u;')
    source += gpio_phase.acquire(result)
    source += ['    if (nx_gd32_soc_start() != 0) {',
               '        result.primary = NX_ERROR_IO;',
               '        result.cleanup = nx_gd32_soc_stop() == 0 ? NX_SUCCESS : NX_ERROR_IO;',
               '        result.remaining_effects = result.cleanup == NX_SUCCESS ? 0u : 1u;']
    source += gpio_phase.abort(result)
    source += ['        s_nx_platform_started = result.remaining_effects != 0u;',
               '        return result;', '    }']
    return source


def start_resources(result, *, cleanup="nx_platform_stop()"):
    source = packet.start_resources(result)
    mask = _dma_clock_mask(result)
    if mask:
        source += [f'    result.primary = nx_gd32_platform_dma_acquire(({mask}), &s_nx_platform_dma_clocks_added);',
                   '    if (result.primary != NX_SUCCESS) {',
                   f'        result.cleanup = {cleanup};',
                   '        result.remaining_effects = result.cleanup == NX_SUCCESS ? 0u : 1u;',
                   '        return result;', '    }']
    return source
