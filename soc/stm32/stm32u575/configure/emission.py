"""One fixed time domain and selected-mask GPIO leases, without registries."""
from tools.configure.providers.common import owned_claims
from tools.configure import preflight


def implementation(item):
    from .contract import mode_implementation
    return mode_implementation(item.kind, item.mode)


def operations(item):
    if item.kind == "i2c":
        return "nx_i2c_v2_ops"
    return f"nx_stm32u575_{implementation(item)}_ops"


def includes(result):
    selected = ['#include "stm32u575_uart_gpdma.h"'] if any(
        item.kind == "uart" and item.mode == "dma-tx"
        for item in result.controllers) else []
    if any(item.kind == "rtc" for item in result.controllers):
        if any(pin["pin"] in {"PC14", "PC15"}
               for item in result.controllers for pin in item.pins):
            raise ValueError("Retained LSE RTC conflicts with selected PC14/PC15 owner")
        selected.append('#include "stm32u575_rtc.h"')
    if any(item.kind == "lptim" for item in result.controllers):
        selected.append('#include "stm32u575_lptim.h"')
    if any(item.kind == "uart" and item.mode == "dma-chain-tx"
           for item in result.controllers):
        selected.append('#include "stm32u575_uart_gpdma_chain.h"')
    if any(item.kind == "uart" and item.mode == "irq-fifo-bytes"
           for item in result.controllers):
        selected.append('#include "stm32u575_uart_fifo.h"')
    if any(item.kind == "uart" and item.mode == "irq-fifo-rtscts-bytes"
           for item in result.controllers):
        selected.append('#include "stm32u575_uart_rtscts.h"')
    return ['#include "stm32u575_provider.h"', '#include "stm32u575_cold.h"',
            '#include "stm32u575_platform_cold.h"', *selected, 'void _init(void);',
            'void _fini(void);',
            r'/** \brief C runtime hooks; constructors use .init_array. */',
            'void _init(void) {}', 'void _fini(void) {}']


def instance_declarations(item, result):
    del item, result
    return []


def shared_irqs(result):
    del result
    return []


def _leases(result):
    groups = {}
    for item in result.controllers:
        for pin in item.pins:
            name, number = pin['pin'][1], int(pin['pin'][2:])
            groups.setdefault(name, set()).add(number)
    owners = {item.id for item in result.controllers}
    owners.update(item.id for item in result.devices)
    for claim in owned_claims(result, owners):
        if claim.key.startswith("pin:"):
            pin = claim.key[4:]
            groups.setdefault(pin[1], set()).add(int(pin[2:]))
    return [(name, sum(1 << n for n in numbers))
            for name, numbers in sorted(groups.items())]


def _peripheral_registers(result):
    registers = set()
    for item in result.controllers:
        if item.kind in {"uart", "spi", "pwm", "i2c", "adc"}:
            register = ("APB2ENR" if item.controller in {"USART1", "SPI1"}
                        else "AHB3ENR" if item.controller == "ADC4"
                        else "APB1ENR1")
            registers.add(register)
    return tuple(sorted(registers))


def state_declarations(result):
    leases = _leases(result)
    if not leases:
        return []
    source = []
    if leases:
        source += ['static uint32_t s_nx_platform_gpio_clocks_added;',
            f'static nx_stm32u575_gpio_receipt_t s_nx_platform_receipts[{len(leases)}];',
            'static const nx_stm32u575_gpio_lease_t s_nx_platform_leases[] = {']
        for index, (name, bits, *_rest) in enumerate(leases):
            source += [f'    {{ GPIO{name}, &s_nx_platform_receipts[{index}u], {bits}u, {ord(name) - ord("A")}u }},']
        source += ['};']
    gates = set(_peripheral_registers(result))
    for register in sorted(gates):
        source += [f'static uint32_t s_nx_peripheral_{register.lower()}_added;']
    flags = []
    for item in result.controllers:
        physical = {"uart": item.controller, "spi": item.controller,
            "pwm": item.controller, "i2c": item.controller, "adc": item.controller,
            "flash": "FLASH", "exti": item.controller, "lptim": "LPTIM2"}.get(item.kind)
        if physical is not None:
            flags.append("NX_U575_SELECT_" + physical)
        if item.kind == "uart" and item.mode in {"dma-tx", "dma-chain-tx"}:
            flags.append("NX_U575_SELECT_GPDMA")
    source += ['static const nx_stm32u575_platform_config_t s_nx_platform_cold = {',
        f'    .leases = {"s_nx_platform_leases" if leases else "NULL"}, .lease_count = {len(leases)}u,',
        '    .selected = ' + (' | '.join(sorted(set(flags))) if flags else '0u') + ',',
        f'    .gpio_added = {"&s_nx_platform_gpio_clocks_added" if leases else "NULL"},']
    for field, register in (("apb1", "APB1ENR1"), ("apb2", "APB2ENR"), ("ahb3", "AHB3ENR")):
        pointer = f'&s_nx_peripheral_{register.lower()}_added' if register in gates else 'NULL'
        source.append(f'    .{field}_added = {pointer},')
    return source + ['};']


def start_system(result):
    return [*preflight.emit(result),
        '    nx_stm32u575_start_result_t system = nx_stm32u575_system_start(&g_nx_stm32u575_system, 1000000u);',
        '    if (system.primary != NX_SUCCESS) {',
        '        result.primary = system.primary;',
        '        result.cleanup = system.cleanup;',
        '        result.remaining_effects = system.remaining_effects;',
        '        s_nx_platform_started = result.remaining_effects != 0u;',
        '        return result;', '    }']


def start_resources(result, *, cleanup="nx_platform_stop()"):
    if not _leases(result):
        return []
    return ['    result.primary = nx_stm32u575_platform_acquire(&s_nx_platform_cold);',
        '    if (result.primary != NX_SUCCESS) {',
        f'        result.cleanup = {cleanup};',
        '        result.remaining_effects = result.cleanup == NX_SUCCESS ? 0u : 1u;',
        '        return result;', '    }']


def stop_system(result, *, system_phase=None):
    source = ['    if (s_nx_platform_started) {']
    if _leases(result):
        source += ['        nx_result_t resources = nx_stm32u575_platform_release(&s_nx_platform_cold);',
            '        if (resources != NX_SUCCESS) { return resources; }']
    if system_phase is not None:
        source.append(f'        {system_phase};')
    return source + ['        nx_result_t system = nx_stm32u575_system_stop(&g_nx_stm32u575_system, 1000000u);',
        '        if (system != NX_SUCCESS) { return system; }', '    }']
