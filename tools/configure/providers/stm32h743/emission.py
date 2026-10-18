"""Fixed H743 clock and selected GPIOB/GPIOD cold field/gate leases."""
try:
    from ... import preflight
except ImportError:
    import preflight


def implementation(item):
    from . import mode_implementation
    return mode_implementation(item.kind, item.mode)


def operations(item):
    return "nx_i2c_v2_ops" if item.kind == "i2c" else f"nx_stm32h743_{implementation(item)}_ops"


def includes(result):
    source = ['#include "stm32h743_provider.h"']
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
        for pin in item["pins"]:
            bank = pin["pin"][1]
            ports.setdefault(bank, set()).add(int(pin["pin"][2:]))
    for item in result.get("devices", ()):
        binding = result["board_bindings"].get(item.get("cs_binding"))
        if binding is not None:
            for pin in result["routes"][binding["route"]]["pins"]:
                ports.setdefault(pin["pin"][1], set()).add(int(pin["pin"][2:]))
    return tuple((bank, tuple(sorted(pins))) for bank, pins in sorted(ports.items()))


def _fields(pins):
    return (sum(1 << pin for pin in pins),
            sum(3 << (2 * pin) for pin in pins),
            sum(15 << (4 * (pin % 8)) for pin in pins))


def _uart(result):
    return any(item.kind == "uart" for item in result.controllers)


def _dma(result):
    return any((item.kind == "uart" and item.mode in ("dma-tx", "rx-dma-idle")) or
               (item.kind == "spi" and item.mode == "dma")
               for item in result.controllers)


def dma_admission_conditions(result, *, spi_only=False):
    """Project already validated lanes; never infer another physical owner."""
    conditions = []
    for lane in result.dma_topology.lanes:
        if spi_only and lane.controller != "SPI4":
            continue
        stream = f"DMA1_Stream{lane.lane}"
        conditions += [f"NX_H743_READ(&{stream}->CR) != 0u",
                       f"NX_H743_READ(&DMAMUX1_Channel{lane.lane}->CCR) != 0u",
                       f"!NX_H743_IRQ_AVAILABLE(DMA1_Stream{lane.lane}_IRQn)"]
        if lane.controller == "SPI4":
            for field in ("NDTR", "PAR", "M0AR", "M1AR"):
                conditions.append(f"NX_H743_READ(&{stream}->{field}) != 0u")
            mask = 0x003d0000 if lane.lane == 2 else 0x0f400000
            conditions += [f"(NX_H743_READ(&{stream}->FCR) & 0x87u) != 1u",
                           f"(NX_H743_READ(&DMA1->LISR) & {mask}u) != 0u"]
    return conditions


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
    source = [f'static uint32_t s_nx_extra_{reg.lower()}_added;'
              for reg, _ in _extra_gates(result)]
    if result.clock_plan is not None:
        source.append('static nx_stm32h743_pll384_state_t s_nx_high_clock;')
    elif any(item.kind == "flash" for item in result.controllers):
        source.append('static nx_stm32h743_flash_timing_state_t s_nx_flash_timing;')
    if _dma(result):
        source.append('static uint32_t s_nx_dma_clocks_added;')
    if _ports(result):
        source.append('static uint32_t s_nx_gpio_clocks_added;')
    if _uart(result):
        source.append('static uint32_t s_nx_uart_clocks_added;')
    for bank, pins in _ports(result):
        bits, fields, af = _fields(pins)
        reg = f'GPIO{bank}'
        lease = f's_nx_gpio{bank.lower()}_lease'
        af_index = pins[0] // 8
        source += ['static struct {',
                   '    uint32_t mode, type, speed, pull, af0, af1, odr;',
                   '    bool captured;', f'}} {lease};',
                   f'static nx_result_t restore_gpio{bank.lower()}(void) {{',
                   f'    if (!{lease}.captured) {{ return NX_SUCCESS; }}',
                   # Drive is withdrawn before restoring mux/latch. Original
                   # mode is restored last, selected fields only.
                   f'    NX_H743_WRITE(&{reg}->MODER, (NX_H743_READ(&{reg}->MODER) & ~{fields}u) | {fields}u);',
                   '    nx_arch_dsb();',
                   f'    if ((NX_H743_READ(&{reg}->MODER) & {fields}u) != {fields}u) {{ return NX_ERROR_IO; }}',
                   f'    NX_H743_WRITE(&{reg}->BSRR, {lease}.odr | (({bits}u & ~{lease}.odr) << 16u));',
                   '    nx_arch_dsb();',
                   f'    if ((NX_H743_READ(&{reg}->ODR) & {bits}u) != {lease}.odr) {{ return NX_ERROR_IO; }}']
        for register, field, mask in ((f'AFR[0]', 'af0', sum(15 << (4 * pin) for pin in pins if pin < 8)),
                                      (f'AFR[1]', 'af1', sum(15 << (4 * (pin % 8)) for pin in pins if pin >= 8)),
                                      ('OTYPER', 'type', bits),
                                      ('OSPEEDR', 'speed', fields),
                                      ('PUPDR', 'pull', fields),
                                      ('MODER', 'mode', fields)):
            source += [f'    NX_H743_WRITE(&{reg}->{register}, (NX_H743_READ(&{reg}->{register}) & ~{mask}u) | {lease}.{field});',
                       '    nx_arch_dsb();',
                       f'    if ((NX_H743_READ(&{reg}->{register}) & {mask}u) != {lease}.{field}) {{ return NX_ERROR_IO; }}']
        source += [f'    {lease}.captured = false;',
                   '    return NX_SUCCESS;', '}']
    return source


def start_system(result):
    from . import cache_mask
    cache = f'{cache_mask(result["clock_profile"])}u'
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
        f'    nx_stm32h743_start_result_t system = nx_stm32h743_system_start(&g_nx_stm32h743_system, {result["clock"]["core_hz"]}u, {cache}, 1000000u);',
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
    ports = _ports(result)
    if not ports:
        return []
    gpio_gates = ' | '.join(f'RCC_AHB4ENR_GPIO{bank}EN' for bank, _ in ports)
    source = []
    for register, gates in _extra_gates(result):
        slot = f's_nx_extra_{register.lower()}_added'
        source += [f'    {slot} = ~NX_H743_READ(&RCC->{register}) & ({gates});',
                   f'    NX_H743_WRITE(&RCC->{register}, NX_H743_READ(&RCC->{register}) | ({gates}));',
                   '    nx_arch_dsb();',
                   f'    if ((NX_H743_READ(&RCC->{register}) & ({gates})) != ({gates})) {{',
                   '        result.primary = NX_ERROR_IO;',
                   f'        result.cleanup = {cleanup};',
                   '        result.remaining_effects = result.cleanup == NX_SUCCESS ? 0u : 1u;',
                   '        return result;', '    }']
    source += [f'    s_nx_gpio_clocks_added = ~NX_H743_READ(&RCC->AHB4ENR) & ({gpio_gates});']
    if _uart(result):
        source += ['    s_nx_uart_clocks_added = ~NX_H743_READ(&RCC->APB1LENR) & RCC_APB1LENR_USART3EN;']
    if _dma(result):
        source += ['    s_nx_dma_clocks_added = ~NX_H743_READ(&RCC->AHB1ENR) & RCC_AHB1ENR_DMA1EN;',
                   '    NX_H743_WRITE(&RCC->AHB1ENR, NX_H743_READ(&RCC->AHB1ENR) | RCC_AHB1ENR_DMA1EN);',
                   '    nx_arch_dsb();',
                   '    const bool dma_clock_ready = (NX_H743_READ(&RCC->AHB1ENR) & RCC_AHB1ENR_DMA1EN) != 0u;',
                   '    if (!dma_clock_ready || (NX_H743_READ(&RCC->AHB1RSTR) & (RCC_AHB1RSTR_DMA1RST | RCC_AHB1RSTR_DMA2RST)) != 0u || ' + ' || '.join(dma_admission_conditions(result)) + ') {',
                   '        result.primary = dma_clock_ready ? NX_ERROR_BUSY : NX_ERROR_IO;',
                   f'        result.cleanup = {cleanup};',
                   '        result.remaining_effects = result.cleanup == NX_SUCCESS ? 0u : 1u;',
                   '        return result;', '    }']
    # Complete admission for every bank before taking any reversible pin lease.
    for bank, _ in ports:
        source += [f'    result.primary = nx_stm32h743_gpio_clock_enable({ord(bank) - ord("A")}u);',
                   '    if (result.primary != NX_SUCCESS) {',
                   f'        result.cleanup = {cleanup};',
                   '        result.remaining_effects = result.cleanup == NX_SUCCESS ? 0u : 1u;',
                   '        return result;', '    }']
    for bank, pins in ports:
        reg = f'GPIO{bank}'
        mode = f'gpio{bank.lower()}_mode'
        bits, _, _ = _fields(pins)
        source.append(f'    const uint32_t {mode} = NX_H743_READ(&{reg}->MODER);')
        conditions = [f'(NX_H743_READ(&{reg}->LCKR) & {bits}u) != 0u']
        for pin in pins:
            field = f'(({mode} >> {pin * 2}u) & 3u)'
            conditions.append(f'({field} != 0u && {field} != 3u)')
        source += ['    if (' + ' || '.join(conditions) + ') {',
                   '        result.primary = NX_ERROR_BUSY;',
                   f'        result.cleanup = {cleanup};',
                   '        result.remaining_effects = result.cleanup == NX_SUCCESS ? 0u : 1u;',
                   '        return result;', '    }']
    for bank, pins in ports:
        reg = f'GPIO{bank}'
        lease = f's_nx_gpio{bank.lower()}_lease'
        bits, fields, af = _fields(pins)
        for register, field, mask in (('MODER', 'mode', fields),
                                      ('OTYPER', 'type', bits),
                                      ('OSPEEDR', 'speed', fields),
                                      ('PUPDR', 'pull', fields),
                                      (f'AFR[0]', 'af0', sum(15 << (4 * pin) for pin in pins if pin < 8)),
                                      (f'AFR[1]', 'af1', sum(15 << (4 * (pin % 8)) for pin in pins if pin >= 8)),
                                      ('ODR', 'odr', bits)):
            source.append(f'    {lease}.{field} = NX_H743_READ(&{reg}->{register}) & {mask}u;')
        source.append(f'    {lease}.captured = true;')
    return source


def stop_resources(result):
    source = []
    if _ports(result):
        source += ['    if (s_nx_platform_started) {']
        for bank, _ in reversed(_ports(result)):
            source += [f'        nx_result_t restored_{bank.lower()} = restore_gpio{bank.lower()}();',
                       f'        if (restored_{bank.lower()} != NX_SUCCESS) {{ return restored_{bank.lower()}; }}']
        if _dma(result):
            source += ['        NX_H743_WRITE(&RCC->AHB1ENR, NX_H743_READ(&RCC->AHB1ENR) & ~s_nx_dma_clocks_added);',
                       '        nx_arch_dsb();',
                       '        if ((NX_H743_READ(&RCC->AHB1ENR) & s_nx_dma_clocks_added) != 0u) { return NX_ERROR_IO; }',
                       '        s_nx_dma_clocks_added = 0u;']
        if _uart(result):
            source += ['        NX_H743_WRITE(&RCC->APB1LENR, NX_H743_READ(&RCC->APB1LENR) & ~s_nx_uart_clocks_added);',
                       '        nx_arch_dsb();',
                       '        if ((NX_H743_READ(&RCC->APB1LENR) & s_nx_uart_clocks_added) != 0u) { return NX_ERROR_IO; }',
                       '        s_nx_uart_clocks_added = 0u;']
        source += ['        NX_H743_WRITE(&RCC->AHB4ENR, NX_H743_READ(&RCC->AHB4ENR) & ~s_nx_gpio_clocks_added);',
                   '        nx_arch_dsb();',
                   '        if ((NX_H743_READ(&RCC->AHB4ENR) & s_nx_gpio_clocks_added) != 0u) { return NX_ERROR_IO; }',
                   '        s_nx_gpio_clocks_added = 0u;', '    }']
    for register, _ in reversed(_extra_gates(result)):
        slot = f's_nx_extra_{register.lower()}_added'
        source += [f'    if ({slot} != 0u) {{',
                   f'        NX_H743_WRITE(&RCC->{register}, NX_H743_READ(&RCC->{register}) & ~{slot});',
                   '        nx_arch_dsb();',
                   f'        if ((NX_H743_READ(&RCC->{register}) & {slot}) != 0u) {{ return NX_ERROR_IO; }}',
                   f'        {slot} = 0u;', '    }']
    return source


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
