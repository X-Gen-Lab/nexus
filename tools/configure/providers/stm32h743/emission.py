"""Fixed H743 clock and selected GPIOB/GPIOD cold field/gate leases."""
try:
    from ... import preflight
except ImportError:
    import preflight


def implementation(item):
    from . import mode_implementation
    return mode_implementation(item.kind, item.mode)


def operations(item):
    return f"nx_stm32h743_{implementation(item)}_ops"


def includes(result):
    source = ['#include "stm32h743_provider.h"']
    if _dma(result):
        source.append('#include "stm32h743_dma.h"')
    return source + ['void _init(void);', 'void _fini(void);',
                    r'/** \brief C runtime hooks; constructors use .init_array. */',
                    'void _init(void) {}', 'void _fini(void) {}']


def instance_declarations(item, result):
    del item, result
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
    return tuple((bank, tuple(sorted(pins))) for bank, pins in sorted(ports.items()))


def _fields(pins):
    return (sum(1 << pin for pin in pins),
            sum(3 << (2 * pin) for pin in pins),
            sum(15 << (4 * (pin % 8)) for pin in pins))


def _uart(result):
    return any(item.kind == "uart" for item in result.controllers)


def _dma(result):
    return any(item.kind == "uart" and item.mode == "dma-tx"
               for item in result.controllers)


def state_declarations(result):
    source = []
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
                   '    uint32_t mode, type, speed, pull, af, odr;',
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
        for register, field, mask in ((f'AFR[{af_index}]', 'af', af),
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
    return preflight.emit(result) + [
        '    result.primary = nx_stm32h743_validate_identity();',
        '    if (result.primary != NX_SUCCESS) { return result; }',
        f'    nx_stm32h743_start_result_t system = nx_stm32h743_system_start(&g_nx_stm32h743_system, {result["clock"]["core_hz"]}u, {cache}, 1000000u);',
        '    if (system.primary != NX_SUCCESS) {',
        '        result.primary = system.primary;',
        '        result.cleanup = system.cleanup;',
        '        result.remaining_effects = system.remaining_effects;',
        '        s_nx_platform_started = result.remaining_effects != 0u;',
        '        return result;', '    }']


def start_resources(result, *, cleanup="nx_platform_stop()"):
    ports = _ports(result)
    if not ports:
        return []
    gates = ' | '.join(f'RCC_AHB4ENR_GPIO{bank}EN' for bank, _ in ports)
    source = [f'    s_nx_gpio_clocks_added = ~NX_H743_READ(&RCC->AHB4ENR) & ({gates});']
    if _uart(result):
        source += ['    s_nx_uart_clocks_added = ~NX_H743_READ(&RCC->APB1LENR) & RCC_APB1LENR_USART3EN;']
    if _dma(result):
        source += ['    s_nx_dma_clocks_added = ~NX_H743_READ(&RCC->AHB1ENR) & RCC_AHB1ENR_DMA1EN;',
                   '    NX_H743_WRITE(&RCC->AHB1ENR, NX_H743_READ(&RCC->AHB1ENR) | RCC_AHB1ENR_DMA1EN);',
                   '    nx_arch_dsb();',
                   '    const bool dma_clock_ready = (NX_H743_READ(&RCC->AHB1ENR) & RCC_AHB1ENR_DMA1EN) != 0u;',
                   '    if (!dma_clock_ready || NX_H743_READ(&DMA1_Stream0->CR) != 0u ||',
                   '        NX_H743_READ(&DMAMUX1_Channel0->CCR) != 0u || !NX_H743_IRQ_AVAILABLE(DMA1_Stream0_IRQn)) {',
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
                                      (f'AFR[{pins[0] // 8}]', 'af', af),
                                      ('ODR', 'odr', bits)):
            source.append(f'    {lease}.{field} = NX_H743_READ(&{reg}->{register}) & {mask}u;')
        source.append(f'    {lease}.captured = true;')
    return source


def stop_system(result, *, system_phase=None):
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
    if system_phase is not None:
        source.append(f'    {system_phase};')
    return source + ['    if (s_nx_platform_started) {',
                     '        nx_result_t system = nx_stm32h743_system_stop(&g_nx_stm32h743_system, 1000000u);',
                     '        if (system != NX_SUCCESS) { return system; }',
                     '    }']
