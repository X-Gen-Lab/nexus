"""One fixed time domain and selected-mask GPIO leases, without registries."""
try:
    from ... import preflight
except ImportError:
    import preflight


def implementation(item):
    from . import mode_implementation
    return mode_implementation(item.kind, item.mode)


def operations(item):
    return f"nx_stm32u575_{implementation(item)}_ops"


def includes(result):
    del result
    return ['#include "stm32u575_provider.h"', 'void _init(void);',
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
        for pin in item['pins']:
            name, number = pin['pin'][1], int(pin['pin'][2:])
            groups.setdefault(name, set()).add(number)
    return [(name, sum(1 << n for n in numbers),
        sum(3 << (2 * n) for n in numbers),
        sum(15 << (4 * n) for n in numbers if n < 8),
        sum(15 << (4 * (n - 8)) for n in numbers if n >= 8))
        for name, numbers in sorted(groups.items())]


def _uart(result):
    return any(item.kind == 'uart' for item in result.controllers)


def _fields(bits, fields, af0, af1):
    return [('MODER', 'mode', fields), ('OTYPER', 'type', bits),
            ('OSPEEDR', 'speed', fields), ('PUPDR', 'pull', fields),
            ('AFR[0]', 'af0', af0), ('AFR[1]', 'af1', af1), ('ODR', 'odr', bits)]


def state_declarations(result):
    leases = _leases(result)
    if not leases:
        return []
    source = ['static uint32_t s_nx_platform_gpio_clocks_added;']
    if _uart(result):
        source += ['static uint32_t s_nx_platform_uart_clocks_added;']
    for name, bits, fields, af0, af1 in leases:
        gpio, lease = 'GPIO' + name, 's_nx_gpio' + name.lower() + '_lease'
        source += ['static struct {',
            '    uint32_t mode, type, speed, pull, af0, af1, odr;',
            f'    bool captured; }} {lease};',
            f'static nx_result_t restore_gpio{name.lower()}(void) {{',
            f'    if (!{lease}.captured) {{ return NX_SUCCESS; }}',
            f'    NX_U575_WRITE(&{gpio}->MODER, (NX_U575_READ(&{gpio}->MODER) & ~{fields}u) | {fields}u);',
            '    nx_arch_dsb();',
            f'    if ((NX_U575_READ(&{gpio}->MODER) & {fields}u) != {fields}u) {{ return NX_ERROR_IO; }}',
            f'    NX_U575_WRITE(&{gpio}->BSRR, {lease}.odr | (({bits}u & ~{lease}.odr) << 16u));',
            '    nx_arch_dsb();',
            f'    if ((NX_U575_READ(&{gpio}->ODR) & {bits}u) != {lease}.odr) {{ return NX_ERROR_IO; }}']
        for register, field, mask in [
                *_fields(bits, fields, af0, af1)[4:6],
                *_fields(bits, fields, af0, af1)[1:4],
                _fields(bits, fields, af0, af1)[0]]:
            if not mask:
                continue
            source += [f'    NX_U575_WRITE(&{gpio}->{register}, (NX_U575_READ(&{gpio}->{register}) & ~{mask}u) | {lease}.{field});',
                '    nx_arch_dsb();',
                f'    if ((NX_U575_READ(&{gpio}->{register}) & {mask}u) != {lease}.{field}) {{ return NX_ERROR_IO; }}']
        source += [f'    {lease}.captured = false;', '    return NX_SUCCESS;', '}']
    return source


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
    leases = _leases(result)
    if not leases:
        return []
    gates = ' | '.join('RCC_AHB2ENR1_GPIO' + n + 'EN'
                       for n, *_ in leases)
    source = [f'    s_nx_platform_gpio_clocks_added = ~NX_U575_READ(&RCC->AHB2ENR1) & ({gates});']
    if _uart(result):
        source += ['    s_nx_platform_uart_clocks_added = ~NX_U575_READ(&RCC->APB2ENR) & RCC_APB2ENR_USART1EN;']
    for name, bits, fields, af0, af1 in leases:
        gpio, lease = 'GPIO' + name, 's_nx_gpio' + name.lower() + '_lease'
        source += [f'    {lease}.captured = false;',
            f'    result.primary = nx_stm32u575_gpio_clock_enable({ord(name)-ord("A")}u);',
            '    if (result.primary != NX_SUCCESS) {',
            f'        result.cleanup = {cleanup};',
            '        result.remaining_effects = result.cleanup == NX_SUCCESS ? 0u : 1u;',
            '        return result;', '    }']
    for name, bits, fields, af0, af1 in leases:
        gpio = 'GPIO' + name
        # An unpowered port cannot be inspected by the preflight. Once its
        # shared gate is observed, admit every selected pin before any mux or
        # output effect; a rejected lease has nothing to restore.
        source += [f'    bool gpio_{name.lower()}_available = (NX_U575_READ(&{gpio}->LCKR) & {bits}u) == 0u;']
        for pin in range(16):
            if bits & (1 << pin):
                source += [f'    uint32_t gpio_{name.lower()}_pin{pin}_mode = (NX_U575_READ(&{gpio}->MODER) >> {pin * 2}u) & 3u;',
                    f'    gpio_{name.lower()}_available = gpio_{name.lower()}_available && (gpio_{name.lower()}_pin{pin}_mode == 0u || gpio_{name.lower()}_pin{pin}_mode == 3u);']
        source += [f'    if (!gpio_{name.lower()}_available) {{',
            '        result.primary = NX_ERROR_BUSY;',
            f'        result.cleanup = {cleanup};',
            '        result.remaining_effects = result.cleanup == NX_SUCCESS ? 0u : 1u;',
            '        return result;', '    }']
    # All selected banks and pins are now admitted. A later rejected pin must
    # never authorize restoration writes to an earlier untouched bank.
    for name, bits, fields, af0, af1 in leases:
        gpio, lease = 'GPIO' + name, 's_nx_gpio' + name.lower() + '_lease'
        for register, field, mask in _fields(bits, fields, af0, af1):
            if mask:
                source += [f'    {lease}.{field} = NX_U575_READ(&{gpio}->{register}) & {mask}u;']
        source += [f'    {lease}.captured = true;']
    return source


def stop_system(result, *, system_phase=None):
    source = ['    if (s_nx_platform_started) {']
    for name, *_ in reversed(_leases(result)):
        source += [f'        nx_result_t gpio_{name.lower()} = restore_gpio{name.lower()}();',
            f'        if (gpio_{name.lower()} != NX_SUCCESS) {{ return gpio_{name.lower()}; }}']
    if _uart(result):
        source += ['        NX_U575_WRITE(&RCC->APB2ENR, NX_U575_READ(&RCC->APB2ENR) & ~s_nx_platform_uart_clocks_added);',
            '        nx_arch_dsb();',
            '        if ((NX_U575_READ(&RCC->APB2ENR) & s_nx_platform_uart_clocks_added) != 0u) { return NX_ERROR_IO; }',
            '        s_nx_platform_uart_clocks_added = 0u;']
    if _leases(result):
        source += ['        NX_U575_WRITE(&RCC->AHB2ENR1, NX_U575_READ(&RCC->AHB2ENR1) & ~s_nx_platform_gpio_clocks_added);',
            '        nx_arch_dsb();',
            '        if ((NX_U575_READ(&RCC->AHB2ENR1) & s_nx_platform_gpio_clocks_added) != 0u) { return NX_ERROR_IO; }',
            '        s_nx_platform_gpio_clocks_added = 0u;']
    if system_phase is not None:
        source.append(f'        {system_phase};')
    return source + ['        nx_result_t system = nx_stm32u575_system_stop(&g_nx_stm32u575_system, 1000000u);',
        '        if (system != NX_SUCCESS) { return system; }', '    }']
