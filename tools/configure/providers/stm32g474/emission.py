"""Fixed G474 clock lifecycle and a single selected-mask GPIOA cold lease."""
try:
    from ... import preflight
except ImportError:
    import preflight


def implementation(item):
    from . import mode_implementation
    return mode_implementation(item.kind, item.mode)


def operations(item):
    return f"nx_stm32g474_{implementation(item)}_ops"


def includes(result):
    del result
    return ['#include "stm32g474_provider.h"', 'void _init(void);',
            'void _fini(void);',
            r'/** \brief C runtime hooks; constructors use .init_array. */',
            'void _init(void) {}', 'void _fini(void) {}']


def instance_declarations(item, result):
    del item, result
    return []


def shared_irqs(result):
    del result
    return []


def _masks(result):
    pins = {int(pin["pin"][2:]) for item in result.controllers
            for pin in item["pins"]}
    bits = sum(1 << pin for pin in pins)
    fields = sum(3 << (2 * pin) for pin in pins)
    af = sum(15 << (4 * pin) for pin in pins)
    return bits, fields, af


def _uart(result):
    return any(item.kind == "uart" for item in result.controllers)


def _restore_field(register, field, mask):
    return [f"    NX_G474_WRITE(&GPIOA->{register}, (NX_G474_READ(&GPIOA->{register}) & ~{mask}u) | s_nx_gpioa_lease.{field});",
            "    nx_arch_dsb();",
            f"    if ((NX_G474_READ(&GPIOA->{register}) & {mask}u) != s_nx_gpioa_lease.{field}) {{ return NX_ERROR_IO; }}"]


def state_declarations(result):
    bits, fields, af = _masks(result)
    if not bits:
        return []
    source = ['static uint32_t s_nx_platform_gpio_clocks_added;',
              'static struct {',
              '    uint32_t mode, type, speed, pull, af, odr;',
              '    bool captured;', ' } s_nx_gpioa_lease;']
    if _uart(result):
        source.append('static uint32_t s_nx_platform_uart_clocks_added;')
    source += ['static nx_result_t restore_gpioa(void) {',
               '    if (!s_nx_gpioa_lease.captured) { return NX_SUCCESS; }',
               # Disable drive before restoring mux/latch; original MODER is
               # restored last. Every write touches only the selected fields.
               f'    NX_G474_WRITE(&GPIOA->MODER, (NX_G474_READ(&GPIOA->MODER) & ~{fields}u) | {fields}u);',
               '    nx_arch_dsb();',
               f'    if ((NX_G474_READ(&GPIOA->MODER) & {fields}u) != {fields}u) {{ return NX_ERROR_IO; }}',
               f'    NX_G474_WRITE(&GPIOA->BSRR, s_nx_gpioa_lease.odr | (({bits}u & ~s_nx_gpioa_lease.odr) << 16u));',
               '    nx_arch_dsb();',
               f'    if ((NX_G474_READ(&GPIOA->ODR) & {bits}u) != s_nx_gpioa_lease.odr) {{ return NX_ERROR_IO; }}']
    for register, field, mask in (("AFR[0]", "af", af),
                                  ("OTYPER", "type", bits),
                                  ("OSPEEDR", "speed", fields),
                                  ("PUPDR", "pull", fields),
                                  ("MODER", "mode", fields)):
        source += _restore_field(register, field, mask)
    return source + ['    s_nx_gpioa_lease.captured = false;',
                     '    return NX_SUCCESS;', '}']


def start_system(result):
    source = preflight.emit(result)
    return source + ['    result.primary = nx_stm32g474_validate_identity();',
            '    if (result.primary != NX_SUCCESS) { return result; }',
            '    nx_stm32g474_start_result_t system = nx_stm32g474_system_start(&g_nx_stm32g474_system, 1000000u);',
            '    if (system.primary != NX_SUCCESS) {',
            '        result.primary = system.primary;',
            '        result.cleanup = system.cleanup;',
            '        result.remaining_effects = system.remaining_effects;',
            '        s_nx_platform_started = result.remaining_effects != 0u;',
            '        return result;', '    }']


def start_resources(result):
    bits, fields, af = _masks(result)
    if not bits:
        return []
    source = ['    s_nx_gpioa_lease.captured = false;',
              '    s_nx_platform_gpio_clocks_added = ~NX_G474_READ(&RCC->AHB2ENR) & RCC_AHB2ENR_GPIOAEN;']
    if _uart(result):
        source += ['    s_nx_platform_uart_clocks_added = ~NX_G474_READ(&RCC->APB1ENR2) & RCC_APB1ENR2_LPUART1EN;']
    source += ['    result.primary = nx_stm32g474_gpio_clock_enable(0u);',
               '    if (result.primary != NX_SUCCESS) {',
               '        result.cleanup = nx_platform_stop();',
               '        result.remaining_effects = result.cleanup == NX_SUCCESS ? 0u : 1u;',
               '        return result;', '    }']
    for register, field, mask in (("MODER", "mode", fields),
                                  ("OTYPER", "type", bits),
                                  ("OSPEEDR", "speed", fields),
                                  ("PUPDR", "pull", fields),
                                  ("AFR[0]", "af", af),
                                  ("ODR", "odr", bits)):
        source.append(f'    s_nx_gpioa_lease.{field} = NX_G474_READ(&GPIOA->{register}) & {mask}u;')
    return source + ['    s_nx_gpioa_lease.captured = true;']


def stop_system(result):
    bits, _, _ = _masks(result)
    source = []
    if bits:
        source += ['    if (s_nx_platform_started) {',
                   '        nx_result_t restored = restore_gpioa();',
                   '        if (restored != NX_SUCCESS) { return restored; }']
        if _uart(result):
            source += ['        NX_G474_WRITE(&RCC->APB1ENR2, NX_G474_READ(&RCC->APB1ENR2) & ~s_nx_platform_uart_clocks_added);',
                       '        nx_arch_dsb();',
                       '        if ((NX_G474_READ(&RCC->APB1ENR2) & s_nx_platform_uart_clocks_added) != 0u) { return NX_ERROR_IO; }',
                       '        s_nx_platform_uart_clocks_added = 0u;']
        source += ['        NX_G474_WRITE(&RCC->AHB2ENR, NX_G474_READ(&RCC->AHB2ENR) & ~s_nx_platform_gpio_clocks_added);',
                   '        nx_arch_dsb();',
                   '        if ((NX_G474_READ(&RCC->AHB2ENR) & s_nx_platform_gpio_clocks_added) != 0u) { return NX_ERROR_IO; }',
                   '        s_nx_platform_gpio_clocks_added = 0u;', '    }']
    return source + ['    if (s_nx_platform_started) {',
                     '        nx_result_t system = nx_stm32g474_system_stop(&g_nx_stm32g474_system, 1000000u);',
                     '        if (system != NX_SUCCESS) { return system; }',
                     '    }']
