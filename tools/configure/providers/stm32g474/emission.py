"""Fixed G474 clock lifecycle with one exact compact platform pin lease."""
try:
    from ... import preflight, gpio_phase
except ImportError:
    import preflight
    import gpio_phase


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


def _uart(result):
    return any(item.kind == "uart" for item in result.controllers)


def _exti(result):
    return any(item.kind == "exti" for item in result.controllers)


def state_declarations(result):
    source = (['static uint32_t s_nx_platform_uart_clocks_added;']
              if _uart(result) else [])
    if _exti(result):
        source += ['static uint32_t s_nx_platform_syscfg_clock_added;']
    return source


def start_system(result):
    source = preflight.emit(result)
    source += ['    result.primary = nx_stm32g474_validate_identity();',
               '    if (result.primary != NX_SUCCESS) { return result; }']
    source += gpio_phase.acquire(result)
    source += ['    nx_stm32g474_start_result_t system = nx_stm32g474_system_start(&g_nx_stm32g474_system, 1000000u);',
               '    if (system.primary != NX_SUCCESS) {',
               '        result.primary = system.primary;',
               '        result.cleanup = system.cleanup;',
               '        result.remaining_effects = system.remaining_effects;']
    source += gpio_phase.abort(result)
    return source + ['        s_nx_platform_started = result.remaining_effects != 0u;',
                     '        return result;', '    }']


def start_resources(result):
    source = (['    s_nx_platform_uart_clocks_added = ~NX_G474_READ(&RCC->APB1ENR2) & RCC_APB1ENR2_LPUART1EN;']
              if _uart(result) else [])
    if _exti(result):
        source += ['    s_nx_platform_syscfg_clock_added = ~NX_G474_READ(&RCC->APB2ENR) & RCC_APB2ENR_SYSCFGEN;',
                   '    NX_G474_WRITE(&RCC->APB2ENR, NX_G474_READ(&RCC->APB2ENR) | RCC_APB2ENR_SYSCFGEN);',
                   '    nx_arch_dsb();',
                   '    if ((NX_G474_READ(&RCC->APB2ENR) & RCC_APB2ENR_SYSCFGEN) == 0u) {',
                   '        result.primary = NX_ERROR_IO;',
                   '        result.cleanup = nx_platform_stop();',
                   '        result.remaining_effects = result.cleanup == NX_SUCCESS ? 0u : 1u;',
                   '        return result;', '    }']
    return source


def stop_system(result):
    source = gpio_phase.release(result)
    if _exti(result):
        source += ['    if (s_nx_platform_syscfg_clock_added != 0u) {',
                   '        NX_G474_WRITE(&RCC->APB2ENR, NX_G474_READ(&RCC->APB2ENR) & ~s_nx_platform_syscfg_clock_added);',
                   '        nx_arch_dsb();',
                   '        if ((NX_G474_READ(&RCC->APB2ENR) & s_nx_platform_syscfg_clock_added) != 0u) { return NX_ERROR_IO; }',
                   '        s_nx_platform_syscfg_clock_added = 0u;', '    }']
    if _uart(result):
        source += ['    if (s_nx_platform_started) {',
                   '        NX_G474_WRITE(&RCC->APB1ENR2, NX_G474_READ(&RCC->APB1ENR2) & ~s_nx_platform_uart_clocks_added);',
                   '        nx_arch_dsb();',
                   '        if ((NX_G474_READ(&RCC->APB1ENR2) & s_nx_platform_uart_clocks_added) != 0u) { return NX_ERROR_IO; }',
                   '        s_nx_platform_uart_clocks_added = 0u;', '    }']
    return source + ['    if (s_nx_platform_started) {',
                     '        nx_result_t system = nx_stm32g474_system_stop(&g_nx_stm32g474_system, 1000000u);',
                     '        if (system != NX_SUCCESS) { return system; }',
                     '    }']
