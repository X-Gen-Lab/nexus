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
    if item.kind == "i2c":
        return "nx_i2c_v2_ops"
    return f"nx_stm32g474_{implementation(item)}_ops"


def includes(result):
    source = ['#include "stm32g474_provider.h"']
    if any(item.kind in {"adc", "flash"} for item in result.controllers):
        source.append('#include "stm32g474_acquisition.h"')
    if any(item.kind == "i2c" for item in result.controllers):
        source.append('#include "i2c_v2.h"')
    if any(item.kind == "pwm_group" for item in result.controllers):
        source.append('#include "stm32g474_pwm_group.h"')
    if any(item.kind in {"timer_capture", "encoder"} for item in result.controllers):
        source.append('#include "stm32g474_timer.h"')
    if any(item.kind == "pwm_adc" for item in result.controllers):
        source.append('#include "stm32g474_pwm_adc.h"')
    if any(item.kind == "can_fd" for item in result.controllers):
        source.append('#include "stm32g474_fdcan.h"')
    return source + ['void _init(void);',
            'void _fini(void);',
            r'/** \brief C runtime hooks; constructors use .init_array. */',
            'void _init(void) {}', 'void _fini(void) {}']


def instance_declarations(item, result):
    del item, result
    return []


def shared_irqs(result):
    del result
    return []


def _syscfg(result):
    return any(item.kind in {"exti", "flash", "pwm_group", "pwm_adc"} for item in result.controllers)


def _peripheral_gates(result):
    selected = {item.controller for item in result.controllers
                if item.kind in {"uart", "spi", "pwm", "pwm_group", "pwm_adc", "i2c", "timer_capture", "encoder"}}
    if any(item.kind in {"adc", "pwm_adc"} for item in result.controllers):
        selected.add("ADC12")
    if any(item.kind == "pwm_adc" for item in result.controllers):
        selected.update({"DMA1", "DMAMUX1"})
    if any(item.kind == "can_fd" for item in result.controllers):
        selected.add("FDCAN")
    if any(item.kind == "adc" and item.mode == "trigger-dma"
           for item in result.controllers):
        selected.update({"TIM6", "DMA1", "DMAMUX1"})
    groups = (("APB1ENR1", ("USART2", "USART3", "UART4", "UART5", "SPI2", "TIM3", "TIM4", "TIM5", "I2C1", "TIM6", "FDCAN")),
              ("APB1ENR2", ("LPUART1",)), ("APB2ENR", ("USART1", "SPI1", "TIM1")))
    groups += (("AHB2ENR", ("ADC12",)), ("AHB1ENR", ("DMA1", "DMAMUX1")))
    return tuple((register, " | ".join(
        f"RCC_{register}_{controller}EN" for controller in controllers
        if controller in selected)) for register, controllers in groups
        if selected.intersection(controllers))


def state_declarations(result):
    source = [f'static uint32_t s_nx_platform_peripheral_{register.lower()}_added;'
              for register, _ in _peripheral_gates(result)]
    if _syscfg(result):
        source += ['static uint32_t s_nx_platform_syscfg_clock_added;']
    if any(item.kind == "i2c" for item in result.controllers):
        source += ['static bool s_nx_platform_i2c_mux_owned;']
    if any(item.kind in {"adc", "pwm_adc"} for item in result.controllers):
        source += ['static bool s_nx_platform_adc_mux_owned;']
    return source


def start_system(result):
    source = preflight.emit(result)
    if any(item.kind in {"pwm_group", "pwm_adc"} for item in result.controllers):
        source += ['    if ((NX_G474_READ(&RCC->APB2ENR) & RCC_APB2ENR_TIM1EN) != 0u ||',
                   '        ((NX_G474_READ(&RCC->APB2ENR) & RCC_APB2ENR_SYSCFGEN) != 0u && (NX_G474_READ(&COMP1->CSR) & ~(uint32_t)COMP_CSR_VALUE) != 0u) ||',
                   '        ((NX_G474_READ(&RCC->APB2ENR) & RCC_APB2ENR_TIM15EN) != 0u && NX_G474_READ(&TIM15->DIER) != 0u) ||',
                   '        !NX_G474_IRQ_AVAILABLE(TIM1_BRK_TIM15_IRQn)) {',
                   '        result.primary = NX_ERROR_BUSY; return result;', '    }']
    for item in result.controllers:
        if item.kind not in {"timer_capture", "encoder"}:
            continue
        timer = item.controller
        source += [f'    if ((NX_G474_READ(&RCC->APB1ENR1) & RCC_APB1ENR1_{timer}EN) != 0u ||',
                   f'        (NX_G474_READ(&RCC->APB1RSTR1) & RCC_APB1RSTR1_{timer}RST) != 0u ||',
                   f'        !NX_G474_IRQ_AVAILABLE({timer}_IRQn)) {{',
                   '        result.primary = NX_ERROR_BUSY; return result;', '    }']
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


def start_resources(result, *, cleanup="nx_platform_stop()"):
    source = []
    if any(item.kind == "i2c" for item in result.controllers):
        source += ['    s_nx_platform_i2c_mux_owned = true;',
                   '    NX_G474_WRITE(&RCC->CCIPR, NX_G474_READ(&RCC->CCIPR) | RCC_CCIPR_I2C1SEL_1);',
                   '    nx_arch_dsb();',
                   '    if ((NX_G474_READ(&RCC->CCIPR) & RCC_CCIPR_I2C1SEL) != RCC_CCIPR_I2C1SEL_1) {',
                   '        result.primary = NX_ERROR_IO;',
                   f'        result.cleanup = {cleanup};',
                   '        result.remaining_effects = result.cleanup == NX_SUCCESS ? 0u : 1u;',
                   '        return result;', '    }']
    if any(item.kind in {"adc", "pwm_adc"} for item in result.controllers):
        source += ['    s_nx_platform_adc_mux_owned = true;',
                   '    NX_G474_WRITE(&RCC->CCIPR, NX_G474_READ(&RCC->CCIPR) | RCC_CCIPR_ADC12SEL_1);',
                   '    nx_arch_dsb();',
                   '    if ((NX_G474_READ(&RCC->CCIPR) & RCC_CCIPR_ADC12SEL) != RCC_CCIPR_ADC12SEL_1) {',
                   '        result.primary = NX_ERROR_IO;',
                   f'        result.cleanup = {cleanup};',
                   '        result.remaining_effects = result.cleanup == NX_SUCCESS ? 0u : 1u;',
                   '        return result;', '    }']
    for register, gates in _peripheral_gates(result):
        slot = f's_nx_platform_peripheral_{register.lower()}_added'
        source += [f'    {slot} = ~NX_G474_READ(&RCC->{register}) & ({gates});',
                   f'    NX_G474_WRITE(&RCC->{register}, NX_G474_READ(&RCC->{register}) | ({gates}));',
                   '    nx_arch_dsb();',
                   f'    if ((NX_G474_READ(&RCC->{register}) & ({gates})) != ({gates})) {{',
                   '        result.primary = NX_ERROR_IO;',
                   f'        result.cleanup = {cleanup};',
                   '        result.remaining_effects = result.cleanup == NX_SUCCESS ? 0u : 1u;',
                   '        return result;', '    }']
    if _syscfg(result):
        source += ['    s_nx_platform_syscfg_clock_added = ~NX_G474_READ(&RCC->APB2ENR) & RCC_APB2ENR_SYSCFGEN;',
                   '    NX_G474_WRITE(&RCC->APB2ENR, NX_G474_READ(&RCC->APB2ENR) | RCC_APB2ENR_SYSCFGEN);',
                   '    nx_arch_dsb();',
                   '    if ((NX_G474_READ(&RCC->APB2ENR) & RCC_APB2ENR_SYSCFGEN) == 0u) {',
                   '        result.primary = NX_ERROR_IO;',
                   f'        result.cleanup = {cleanup};',
                   '        result.remaining_effects = result.cleanup == NX_SUCCESS ? 0u : 1u;',
                   '        return result;', '    }']
    if any(item.kind == "flash" for item in result.controllers):
        source += ['    if ((NX_G474_FLASH_REMAP() & SYSCFG_MEMRMP_FB_MODE) != 0u || (NX_G474_READ(&FLASH->OPTR) & FLASH_OPTR_BFB2) != 0u) {',
                   '        result.primary = NX_ERROR_UNSUPPORTED;',
                   f'        result.cleanup = {cleanup};',
                   '        result.remaining_effects = result.cleanup == NX_SUCCESS ? 0u : 1u;',
                   '        return result;', '    }']
    return source


def stop_system(result, *, system_phase=None):
    source = gpio_phase.release(result)
    if _syscfg(result):
        source += ['    if (s_nx_platform_syscfg_clock_added != 0u) {',
                   '        NX_G474_WRITE(&RCC->APB2ENR, NX_G474_READ(&RCC->APB2ENR) & ~s_nx_platform_syscfg_clock_added);',
                   '        nx_arch_dsb();',
                   '        if ((NX_G474_READ(&RCC->APB2ENR) & s_nx_platform_syscfg_clock_added) != 0u) { return NX_ERROR_IO; }',
                   '        s_nx_platform_syscfg_clock_added = 0u;', '    }']
    for register, _ in reversed(_peripheral_gates(result)):
        slot = f's_nx_platform_peripheral_{register.lower()}_added'
        source += [f'    if ({slot} != 0u) {{',
                   f'        NX_G474_WRITE(&RCC->{register}, NX_G474_READ(&RCC->{register}) & ~{slot});',
                   '        nx_arch_dsb();',
                   f'        if ((NX_G474_READ(&RCC->{register}) & {slot}) != 0u) {{ return NX_ERROR_IO; }}',
                   f'        {slot} = 0u;', '    }']
    if any(item.kind == "i2c" for item in result.controllers):
        source += ['    if (s_nx_platform_i2c_mux_owned) {',
                   '        NX_G474_WRITE(&RCC->CCIPR, NX_G474_READ(&RCC->CCIPR) & ~(uint32_t)RCC_CCIPR_I2C1SEL);',
                   '        nx_arch_dsb();',
                   '        if ((NX_G474_READ(&RCC->CCIPR) & RCC_CCIPR_I2C1SEL) != 0u) { return NX_ERROR_IO; }',
                   '        s_nx_platform_i2c_mux_owned = false;', '    }']
    if any(item.kind in {"adc", "pwm_adc"} for item in result.controllers):
        source += ['    if (s_nx_platform_adc_mux_owned) {',
                   '        NX_G474_WRITE(&RCC->CCIPR, NX_G474_READ(&RCC->CCIPR) & ~(uint32_t)RCC_CCIPR_ADC12SEL);',
                   '        nx_arch_dsb();',
                   '        if ((NX_G474_READ(&RCC->CCIPR) & RCC_CCIPR_ADC12SEL) != 0u) { return NX_ERROR_IO; }',
                   '        s_nx_platform_adc_mux_owned = false;', '    }']
    if system_phase is not None:
        source.append(f'    {system_phase};')
    return source + ['    if (s_nx_platform_started) {',
                     '        nx_result_t system = nx_stm32g474_system_stop(&g_nx_stm32g474_system, 1000000u);',
                     '        if (system != NX_SUCCESS) { return system; }',
                     '    }']
