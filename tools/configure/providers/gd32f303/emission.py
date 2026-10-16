"""Exact legacy GPIO lease and fixed IRC8 lifecycle; no runtime registry."""


def implementation(item):
    from . import mode_implementation
    return mode_implementation(item.kind, item.mode)


def operations(item):
    return f"nx_gd32f303_{implementation(item)}_ops"


def includes(result):
    del result
    return ['#include "gd32f303_provider.h"', 'void _init(void);',
            'void _fini(void);',
            r'/** \brief Newlib runtime hooks; no dynamic constructor policy. */',
            'void _init(void) {}', 'void _fini(void) {}']


def instance_declarations(item, result):
    del item, result
    return []


def shared_irqs(result):
    del result
    return []


def _pins(result):
    numbers = {int(pin['pin'][2:]) for item in result.controllers
               for pin in item['pins']}
    for child in result.get("devices", ()):
        if "cs_binding" in child:
            binding = result["board_bindings"][child["cs_binding"]]
            numbers.update(int(pin['pin'][2:])
                for pin in result["routes"][binding["route"]]["pins"])
    return sum(1 << number for number in sorted(numbers))


def _spi(result):
    return any(item.kind == "spi" for item in result.controllers)


def _peripheral_gates(result):
    gates = []
    if _uart(result):
        gates.append("RCU_APB2EN_USART0EN")
    if _spi(result):
        gates.append("RCU_APB2EN_SPI0EN")
    if gates:
        gates.append("RCU_APB2EN_AFEN")
    return " | ".join(gates)


def _uart(result):
    return any(item.kind == "uart" for item in result.controllers)


def state_declarations(result):
    source = (["static nx_gd32f303_pin_lease_t s_nx_platform_pins;"]
              if _pins(result) else [])
    if _peripheral_gates(result):
        source += ["static uint32_t s_nx_platform_uart_clocks_added;"]
    return source


def start_system(result):
    source = []
    if _pins(result):
        source += [f'    result.primary = nx_gd32f303_pins_preflight({_pins(result)}u);',
                   '    if (result.primary != NX_SUCCESS) { return result; }']
    if _uart(result):
        source += ['    if ((NX_GD303_READ(&RCU_APB2EN) & RCU_APB2EN_USART0EN) != 0u || !NX_GD303_IRQ_AVAILABLE(USART0_IRQn)) {',
                   '        result.primary = NX_ERROR_BUSY; return result;',
                   '    }']
    if _spi(result):
        source += ['    if ((NX_GD303_READ(&RCU_APB2EN) & RCU_APB2EN_SPI0EN) != 0u || !NX_GD303_IRQ_AVAILABLE(SPI0_IRQn)) {',
                   '        result.primary = NX_ERROR_BUSY; return result;', '    }']
    return source + [
        '    nx_gd32f303_start_result_t system = nx_gd32f303_system_start(&g_nx_gd32f303_system, 1000000u);',
        '    if (system.primary != NX_SUCCESS) {',
        '        result.primary = system.primary;',
        '        result.cleanup = system.cleanup;',
        '        result.remaining_effects = system.remaining_effects;',
        '        s_nx_platform_started = result.remaining_effects != 0u;',
        '        return result;', '    }']


def start_resources(result, *, cleanup="nx_platform_stop()"):
    source = []
    if _pins(result):
        source += [f'    result.primary = nx_gd32f303_pins_acquire(&s_nx_platform_pins, {_pins(result)}u);',
            '    if (result.primary != NX_SUCCESS) {',
            f'        result.cleanup = {cleanup};',
            '        result.remaining_effects = result.cleanup == NX_SUCCESS ? 0u : 1u;',
            '        return result;', '    }']
    if _peripheral_gates(result):
        source += [f'    const uint32_t uart_gates = {_peripheral_gates(result)};',
            '    s_nx_platform_uart_clocks_added = ~NX_GD303_READ(&RCU_APB2EN) & uart_gates;',
            '    NX_GD303_WRITE(&RCU_APB2EN, NX_GD303_READ(&RCU_APB2EN) | uart_gates);',
            '    nx_arch_dsb();',
            '    if ((NX_GD303_READ(&RCU_APB2EN) & uart_gates) != uart_gates) {',
            '        result.primary = NX_ERROR_IO;',
            f'        result.cleanup = {cleanup};',
            '        result.remaining_effects = result.cleanup == NX_SUCCESS ? 0u : 1u;',
            '        return result;', '    }']
    return source


def stop_system(result, *, system_phase=None):
    source = []
    if _pins(result):
        source += ['    nx_result_t pins = nx_gd32f303_pins_release(&s_nx_platform_pins);',
                   '    if (pins != NX_SUCCESS) { return pins; }']
    if _peripheral_gates(result):
        source += ['    if (s_nx_platform_uart_clocks_added != 0u) {',
            '        NX_GD303_WRITE(&RCU_APB2EN, NX_GD303_READ(&RCU_APB2EN) & ~s_nx_platform_uart_clocks_added);',
            '        nx_arch_dsb();',
            '        if ((NX_GD303_READ(&RCU_APB2EN) & s_nx_platform_uart_clocks_added) != 0u) { return NX_ERROR_IO; }',
            '        s_nx_platform_uart_clocks_added = 0u;', '    }']
    if system_phase is not None:
        source.append(f'    {system_phase};')
    return source + ['    if (s_nx_platform_started) {',
        '        nx_result_t system = nx_gd32f303_system_stop(&g_nx_gd32f303_system);',
        '        if (system != NX_SUCCESS) { return system; }', '    }']
