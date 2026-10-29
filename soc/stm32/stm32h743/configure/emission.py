"""Fixed H743 clock and selected GPIOB/GPIOD cold field/gate leases."""
from tools.configure import preflight


def implementation(item):
    from .contract import mode_implementation
    return mode_implementation(item.kind, item.mode)


def operations(item):
    return "nx_i2c_v2_ops" if item.kind == "i2c" else f"nx_stm32h743_{implementation(item)}_ops"


def includes(result):
    source = ['#include "stm32h743_cold.h"',
              '#include "stm32h743_platform_cold.h"']
    if any(item.kind == "flash" for item in result.controllers):
        source.append('#include "stm32h743_flash_ram.h"')
    if result.clock_plan is not None:
        source.append('#include "stm32h743_clock.h"')
    if any(item.kind == "uart" and item.mode == "irq-fifo-bytes"
           for item in result.controllers):
        source.append('#include "stm32h743_uart_fifo.h"')
    if any(item.kind == "uart" and item.mode == "irq-fifo-rtscts-bytes"
           for item in result.controllers):
        source.append('#include "stm32h743_uart_rtscts.h"')
    if any(item.kind == "uart" and item.mode in ("dma-tx", "rx-dma-idle")
           for item in result.controllers):
        source.append('#include "stm32h743_dma.h"')
    if any(item.kind == "spi" and item.mode == "dma"
           for item in result.controllers):
        source.append('#include "stm32h743_spi_dma.h"')
    if any(item.kind == "uart" and item.mode == "rx-dma-idle"
           for item in result.controllers):
        source.append('#include "stm32h743_uart_receive.h"')
    if any(item.kind == "uart" and item.mode == "rx-dma-continuous"
           for item in result.controllers):
        source.append('#include "stm32h743_uart_continuous.h"')
    if any(item.kind == "uart" and item.mode == "dma-duplex-continuous"
           for item in result.controllers):
        source.append('#include "stm32h743_uart_duplex.h"')
    return source + ['void _init(void);', 'void _fini(void);',
                    r'/** \brief C runtime hooks; constructors use .init_array. */',
                    'void _init(void) {}', 'void _fini(void) {}']


def instance_declarations(item, result):
    del result
    if item.kind == "flash":
        return ['__attribute__((section(".bss.nx_h743_flash_workspace")))',
                "_Alignas(1024) nx_stm32h743_flash_ram_workspace_t "
                "g_nx_stm32h743_flash_ram_workspace;"]
    return []


def shared_irqs(result):
    del result
    return []


def _ports(result):
    ports = {}
    for item in result.controllers:
        for pin in item.pins:
            bank = pin["pin"][1]
            ports.setdefault(bank, set()).add(int(pin["pin"][2:]))
    for item in result.devices:
        binding = result.board_bindings.get(getattr(item.options, "cs_binding", None))
        if binding is not None:
            for pin in result.routes[binding["route"]]["pins"]:
                ports.setdefault(pin["pin"][1], set()).add(int(pin["pin"][2:]))
    return tuple((bank, tuple(sorted(pins))) for bank, pins in sorted(ports.items()))


def _fields(pins):
    return (sum(1 << pin for pin in pins),
            sum(3 << (2 * pin) for pin in pins),
            sum(15 << (4 * (pin % 8)) for pin in pins))


def _uart(result):
    return any(item.kind == "uart" for item in result.controllers)


def _dma(result):
    return any((item.kind == "uart" and item.mode in
                ("dma-tx", "rx-dma-idle", "rx-dma-continuous",
                 "dma-duplex-continuous")) or
               (item.kind == "spi" and item.mode == "dma")
               for item in result.controllers)


def dma_admission(result):
    """Project explicit selected lane facts, without register expressions."""
    duplex = any(item.kind == "uart" and item.mode == "dma-duplex-continuous"
                 for item in result.controllers)
    lanes = sum(1 << lane.lane for lane in result.dma_topology.lanes)
    strict = sum(1 << lane.lane for lane in result.dma_topology.lanes
                 if lane.controller == "SPI4" or
                 (duplex and lane.controller == "USART3"))
    return lanes, strict


def _extra_gates(result):
    selected = {item.controller for item in result.controllers}
    groups = (("APB2ENR", ("SPI4",)), ("APB1LENR", ("TIM3", "I2C1")),
              ("AHB1ENR", ("ADC12",)),
              ("APB4ENR", ("SYSCFG",)))
    if "ADC1" in selected:
        selected.add("ADC12")
    if selected.intersection({"EXTI0", "EXTI1"}):
        selected.add("SYSCFG")
    return tuple((reg, " | ".join(f"RCC_{reg}_{name}EN" for name in names
                                  if name in selected)) for reg, names in groups
                 if selected.intersection(names))


def state_declarations(result):
    """Only exact selected storage and immutable static connections."""
    source = []
    if result.clock_plan is not None:
        source.append('static nx_stm32h743_pll384_state_t s_nx_high_clock;')
    elif any(item.kind == "flash" for item in result.controllers):
        source.append('static nx_stm32h743_flash_timing_state_t s_nx_flash_timing;')
    ports = _ports(result)
    if not ports:
        return source
    source += ['static uint32_t s_nx_gpio_clocks_added;']
    if _uart(result):
        source += ['static uint32_t s_nx_uart_clocks_added;']
    if _dma(result):
        source += ['static uint32_t s_nx_dma_clocks_added;']
    for register, _ in _extra_gates(result):
        source += [f'static uint32_t s_nx_extra_{register.lower()}_added;']
    for bank, _ in ports:
        source += [f'static nx_stm32h743_gpio_lease_t s_nx_gpio{bank.lower()}_lease;']
    source += ['static const nx_stm32h743_gpio_lease_config_t s_nx_gpio_leases[] = {']
    for bank, pins in ports:
        bits, _, _ = _fields(pins)
        source += [f'    {{ GPIO{bank}, &s_nx_gpio{bank.lower()}_lease, {bits}u, {ord(bank) - ord("A")}u }},']
    source += ['};', 'static const nx_stm32h743_platform_config_t s_nx_platform_config = {',
        f'    .ports = s_nx_gpio_leases, .port_count = {len(ports)}u,',
        '    .gpio_added = &s_nx_gpio_clocks_added,',
        f'    .uart_added = {"&s_nx_uart_clocks_added" if _uart(result) else "NULL"},',
        f'    .dma_added = {"&s_nx_dma_clocks_added" if _dma(result) else "NULL"},']
    gates = dict(_extra_gates(result))
    for field, register in (("apb2", "APB2ENR"), ("apb1", "APB1LENR"),
                            ("ahb1", "AHB1ENR"), ("apb4", "APB4ENR")):
        mask = gates.get(register)
        source += [f'    .{field} = {{ {mask}, &s_nx_extra_{register.lower()}_added }},'
                   if mask else f'    .{field} = {{ 0u, NULL }},']
    lanes, strict = dma_admission(result)
    return source + [f'    .dma_lanes = {lanes}u, .dma_strict = {strict}u', '};']


def start_system(result):
    from .contract import cache_mask
    cache = f'{cache_mask(result.clock_profile)}u'
    if result.clock_plan is not None:
        return preflight.emit(result) + [
            '    result.primary = nx_stm32h743_validate_identity();',
            '    if (result.primary != NX_SUCCESS) { return result; }',
            '    nx_stm32h743_start_result_t clock = nx_stm32h743_pll384_start(&s_nx_high_clock, 1000000u);',
            '    if (clock.primary != NX_SUCCESS) {',
            '        result.primary = clock.primary;',
            '        result.cleanup = clock.cleanup;',
            '        result.remaining_effects = clock.remaining_effects == 0u ? 0u : 1u;',
            '        s_nx_platform_started = result.remaining_effects != 0u;',
            '        return result;', '    }',
            '    s_nx_platform_started = true;',
            f'    nx_stm32h743_start_result_t system = nx_stm32h743_pll384_system_start(&s_nx_high_clock, {cache}, 1000000u);',
            '    if (system.primary != NX_SUCCESS) {',
            '        result.primary = system.primary;',
            '        result.cleanup = nx_platform_stop_impl(diagnostic, true);',
            '        result.remaining_effects = result.cleanup == NX_SUCCESS ? 0u : 1u;',
            '        return result;', '    }']
    source = preflight.emit(result) + [
        '    result.primary = nx_stm32h743_validate_identity();',
        '    if (result.primary != NX_SUCCESS) { return result; }',
        f'    nx_stm32h743_start_result_t system = nx_stm32h743_system_start(&g_nx_stm32h743_system, {result.clock["core_hz"]}u, {cache}, 1000000u);',
        '    if (system.primary != NX_SUCCESS) {',
        '        result.primary = system.primary;',
        '        result.cleanup = system.cleanup;',
        '        result.remaining_effects = system.remaining_effects;',
        '        s_nx_platform_started = result.remaining_effects != 0u;',
        '        return result;', '    }']
    if result.clock_plan is None and any(item.kind == "flash" for item in result.controllers):
        source += ['    s_nx_platform_started = true;',
                   '    result.primary = nx_stm32h743_flash_timing_initialize(&s_nx_flash_timing);',
                   '    if (result.primary != NX_SUCCESS) {',
                   '        result.cleanup = nx_platform_stop_impl(diagnostic, true);',
                   '        result.remaining_effects = result.cleanup == NX_SUCCESS ? 0u : 1u;',
                   '        return result;', '    }']
    return source


def start_resources(result, *, cleanup="nx_platform_stop()"):
    """Fixed C transaction and explicit Platform failure propagation."""
    if not _ports(result):
        return []
    return ['    result.primary = nx_stm32h743_platform_acquire(&s_nx_platform_config);',
            '    if (result.primary != NX_SUCCESS) {',
            f'        result.cleanup = {cleanup};',
            '        result.remaining_effects = result.cleanup == NX_SUCCESS ? 0u : 1u;',
            '        return result;', '    }']


def stop_resources(result):
    if not _ports(result):
        return []
    return ['    nx_result_t resources = nx_stm32h743_platform_release(&s_nx_platform_config);',
            '    if (resources != NX_SUCCESS) { return resources; }']


def stop_system(result, *, system_phase=None, resources=True):
    source = stop_resources(result) if resources else []
    if system_phase is not None:
        source.append(f'    {system_phase};')
    if result.clock_plan is None and any(item.kind == "flash" for item in result.controllers):
        source += ['    nx_result_t timing = nx_stm32h743_flash_timing_stop(&s_nx_flash_timing);',
                   '    if (timing != NX_SUCCESS) { return timing; }']
    source += ['    if (s_nx_platform_started) {',
               '        nx_result_t system = nx_stm32h743_system_stop(&g_nx_stm32h743_system, 1000000u);',
               '        if (system != NX_SUCCESS) { return system; }', '    }']
    if result.clock_plan is not None:
        source += ['    nx_result_t clock = nx_stm32h743_pll384_stop(&s_nx_high_clock, 1000000u);',
                   '    if (clock != NX_SUCCESS) { return clock; }']
    return source
