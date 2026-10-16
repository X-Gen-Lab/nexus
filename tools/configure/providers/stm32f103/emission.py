"""Exact F1 CRL pin leases; clocks and lifecycle stay statically assembled."""


def implementation(item):
    from . import mode_implementation
    return mode_implementation(item.kind, item.mode)


def operations(item):
    return f"nx_stm32f103_{implementation(item)}_ops"


def includes(result):
    del result
    return ['#include "stm32f103_provider.h"', 'void _init(void);',
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


def _spi(result):
    return any(item.kind == "spi" for item in result.controllers)


def _exti(result):
    return tuple(item for item in result.controllers if item.kind == "exti")


def _pins(result):
    pins = {int(pin["pin"][2:])
            for item in result.controllers for pin in item["pins"]}
    for device in result["devices"]:
        if device.get("cs_binding") is not None:
            binding = result["board_bindings"][device["cs_binding"]]
            pins.update(int(pin["pin"][2:])
                        for pin in result["routes"][binding["route"]]["pins"])
    return tuple(sorted(pins))


def _masks(result):
    pins = _pins(result)
    return sum(1 << pin for pin in pins), sum(15 << (4 * pin) for pin in pins)


def state_declarations(result):
    if not _pins(result):
        return []
    bits, fields = _masks(result)
    source = ['static uint32_t s_nx_f103_gpio_gates_added;',
              'static uint32_t s_nx_f103_saved_crl;',
              'static uint32_t s_nx_f103_saved_odr;',
              'static bool s_nx_f103_pins_captured;']
    if _uart(result):
        source.append('static uint32_t s_nx_f103_uart_gate_added;')
    floating = sum(4 << (4 * pin) for pin in _pins(result))
    source += ['static nx_result_t restore_f103_pins(void) {',
               '    if (!s_nx_f103_pins_captured) { return NX_SUCCESS; }',
               f'    NX_F103_WRITE(&GPIOA->CRL, (NX_F103_READ(&GPIOA->CRL) & ~{fields}u) | {floating}u);',
               '    nx_arch_dsb();',
               f'    if ((NX_F103_READ(&GPIOA->CRL) & {fields}u) != {floating}u) {{ return NX_ERROR_IO; }}',
               f'    NX_F103_WRITE(&GPIOA->BSRR, s_nx_f103_saved_odr | (({bits}u & ~s_nx_f103_saved_odr) << 16u));',
               '    nx_arch_dsb();',
               f'    if ((NX_F103_READ(&GPIOA->ODR) & {bits}u) != s_nx_f103_saved_odr) {{ return NX_ERROR_IO; }}',
               f'    NX_F103_WRITE(&GPIOA->CRL, (NX_F103_READ(&GPIOA->CRL) & ~{fields}u) | s_nx_f103_saved_crl);',
               '    nx_arch_dsb();',
               f'    if ((NX_F103_READ(&GPIOA->CRL) & {fields}u) != s_nx_f103_saved_crl) {{ return NX_ERROR_IO; }}',
               '    s_nx_f103_pins_captured = false;',
               '    return NX_SUCCESS;', '}']
    return source


def start_system(result):
    source = ['    result.primary = nx_stm32f103_system_preflight(&g_nx_stm32f103_system);',
              '    if (result.primary != NX_SUCCESS) { return result; }']
    if _uart(result):
        source += ['    if ((NX_F103_READ(&RCC->APB1ENR) & RCC_APB1ENR_USART2EN) != 0u || !NX_F103_IRQ_AVAILABLE(USART2_IRQn)) {',
                   '        result.primary = NX_ERROR_BUSY; return result;', '    }',
                   '    if ((NX_F103_READ(&RCC->APB2ENR) & RCC_APB2ENR_AFIOEN) != 0u && (NX_F103_READ(&AFIO->MAPR) & AFIO_MAPR_USART2_REMAP) != 0u) {',
                   '        result.primary = NX_ERROR_BUSY; return result;', '    }']
    for item in _exti(result):
        line = int(item.controller[-1])
        source += [f'    if (((NX_F103_READ(&EXTI->IMR) | NX_F103_READ(&EXTI->EMR) | NX_F103_READ(&EXTI->RTSR) | NX_F103_READ(&EXTI->FTSR) | NX_F103_READ(&EXTI->PR)) & {1 << line}u) != 0u || !NX_F103_IRQ_AVAILABLE(EXTI{line}_IRQn)) {{',
                   '        result.primary = NX_ERROR_BUSY; return result;', '    }']
    if _spi(result):
        source += ['    if ((NX_F103_READ(&RCC->APB2ENR) & RCC_APB2ENR_SPI1EN) != 0u || (NX_F103_READ(&RCC->APB2RSTR) & RCC_APB2RSTR_SPI1RST) != 0u || !NX_F103_IRQ_AVAILABLE(SPI1_IRQn)) {',
                   '        result.primary = NX_ERROR_BUSY; return result;', '    }']
    if _pins(result):
        bits, _ = _masks(result)
        source += ['    if ((NX_F103_READ(&RCC->APB2ENR) & RCC_APB2ENR_IOPAEN) != 0u) {',
                   f'        if ((NX_F103_READ(&GPIOA->LCKR) & {bits}u) != 0u) {{ result.primary = NX_ERROR_BUSY; return result; }}']
        for pin in _pins(result):
            field = f'((NX_F103_READ(&GPIOA->CRL) >> {4 * pin}u) & 15u)'
            source += [f'        if ({field} != 0u && {field} != 4u) {{ result.primary = NX_ERROR_BUSY; return result; }}']
        source.append('    }')
    return source + [
        '    nx_stm32f103_start_result_t system = nx_stm32f103_system_start(&g_nx_stm32f103_system, 1000000u);',
        '    if (system.primary != NX_SUCCESS) {',
        '        result.primary = system.primary;',
        '        result.cleanup = system.cleanup;',
        '        result.remaining_effects = system.remaining_effects;',
        '        s_nx_platform_started = result.remaining_effects != 0u;',
        '        return result;', '    }']


def start_resources(result, *, cleanup="nx_platform_stop()"):
    if not _pins(result):
        return []
    bits, fields = _masks(result)
    gates = 'RCC_APB2ENR_IOPAEN'
    if _uart(result) or _exti(result) or _spi(result):
        gates += ' | RCC_APB2ENR_AFIOEN'
    if _spi(result):
        gates += ' | RCC_APB2ENR_SPI1EN'
    source = [f'    s_nx_f103_gpio_gates_added = ~NX_F103_READ(&RCC->APB2ENR) & ({gates});']
    if _uart(result):
        source += ['    s_nx_f103_uart_gate_added = ~NX_F103_READ(&RCC->APB1ENR) & RCC_APB1ENR_USART2EN;']
    source += [f'    NX_F103_WRITE(&RCC->APB2ENR, NX_F103_READ(&RCC->APB2ENR) | ({gates}));',
               '    nx_arch_dsb();',
               f'    if ((NX_F103_READ(&RCC->APB2ENR) & ({gates})) != ({gates})) {{',
               '        result.primary = NX_ERROR_IO;',
               f'        result.cleanup = {cleanup};',
               '        result.remaining_effects = result.cleanup == NX_SUCCESS ? 0u : 1u;',
               '        return result;', '    }']
    conditions = [f'(NX_F103_READ(&GPIOA->LCKR) & {bits}u) != 0u']
    if _uart(result):
        conditions += ['(NX_F103_READ(&AFIO->MAPR) & AFIO_MAPR_USART2_REMAP) != 0u']
    if _spi(result):
        conditions += ['(NX_F103_READ(&AFIO->MAPR) & AFIO_MAPR_SPI1_REMAP) != 0u',
                       'NX_F103_READ(&SPI1->CR1) != 0u || NX_F103_READ(&SPI1->CR2) != 0u || (NX_F103_READ(&SPI1->SR) & (SPI_SR_BSY | SPI_SR_RXNE | SPI_SR_OVR | SPI_SR_MODF)) != 0u']
    for pin in _pins(result):
        field = f'((NX_F103_READ(&GPIOA->CRL) >> {4 * pin}u) & 15u)'
        conditions.append(f'({field} != 0u && {field} != 4u)')
    source += ['    if (' + ' || '.join(conditions) + ') {',
               '        result.primary = NX_ERROR_BUSY;',
               f'        result.cleanup = {cleanup};',
               '        result.remaining_effects = result.cleanup == NX_SUCCESS ? 0u : 1u;',
               '        return result;', '    }',
               f'    s_nx_f103_saved_crl = NX_F103_READ(&GPIOA->CRL) & {fields}u;',
               f'    s_nx_f103_saved_odr = NX_F103_READ(&GPIOA->ODR) & {bits}u;',
               '    s_nx_f103_pins_captured = true;']
    return source


def stop_system(result, *, system_phase=None):
    source = []
    if _pins(result):
        source += ['    if (s_nx_platform_started) {',
                   '        nx_result_t pins = restore_f103_pins();',
                   '        if (pins != NX_SUCCESS) { return pins; }']
        if _uart(result):
            source += ['        NX_F103_WRITE(&RCC->APB1ENR, NX_F103_READ(&RCC->APB1ENR) & ~s_nx_f103_uart_gate_added);',
                       '        nx_arch_dsb();',
                       '        if ((NX_F103_READ(&RCC->APB1ENR) & s_nx_f103_uart_gate_added) != 0u) { return NX_ERROR_IO; }',
                       '        s_nx_f103_uart_gate_added = 0u;']
        source += ['        NX_F103_WRITE(&RCC->APB2ENR, NX_F103_READ(&RCC->APB2ENR) & ~s_nx_f103_gpio_gates_added);',
                   '        nx_arch_dsb();',
                   '        if ((NX_F103_READ(&RCC->APB2ENR) & s_nx_f103_gpio_gates_added) != 0u) { return NX_ERROR_IO; }',
                   '        s_nx_f103_gpio_gates_added = 0u;', '    }']
    if system_phase is not None:
        source.append(f'    {system_phase};')
    return source + ['    if (s_nx_platform_started) {',
                     '        nx_result_t system = nx_stm32f103_system_stop(&g_nx_stm32f103_system, 1000000u);',
                     '        if (system != NX_SUCCESS) { return system; }', '    }']
