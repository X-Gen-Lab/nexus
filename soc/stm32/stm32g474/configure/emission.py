"""Fixed G474 clock lifecycle with one exact compact platform pin lease."""
from tools.configure import preflight, gpio_phase


def implementation(item):
    from .contract import mode_implementation
    return mode_implementation(item.kind, item.mode)


def operations(item):
    if item.kind == "i2c":
        return "nx_i2c_v2_ops"
    return f"nx_stm32g474_{implementation(item)}_ops"


def includes(result):
    source = ['#include "stm32g474_provider.h"']
    if result.controllers:
        source.append('#include "stm32g474_platform_cold.h"')
    if any(item.kind == "gpio" and item.mode == "named-states"
           for item in result.controllers):
        source.append('#include "stm32g474_pin_state.h"')
    if any(item.kind in {"adc", "flash"} for item in result.controllers):
        source.append('#include "stm32g474_acquisition.h"')
    if any(item.kind == "i2c" for item in result.controllers):
        source.append('#include "i2c_v2.h"')
    if any(item.kind in {"pwm_group", "pwm_adc"} for item in result.controllers):
        source.append('#include "stm32g474_control_cold.h"')
    if any(item.kind == "pwm_group" for item in result.controllers):
        source.append('#include "stm32g474_pwm_group.h"')
    if result.pwm_shared_owners:
        source.append('#include "stm32g474_pwm_shared.h"')
    if any(item.kind in {"timer_capture", "encoder"} for item in result.controllers):
        source.append('#include "stm32g474_timer.h"')
    if any(item.kind == "pwm_adc" for item in result.controllers):
        source.append('#include "stm32g474_pwm_adc.h"')
    if any(item.kind == "can_fd" for item in result.controllers):
        source.append('#include "stm32g474_fdcan.h"')
    if any(item.kind == "adc_group" for item in result.controllers):
        source += ['#include "stm32g474_adc_group.h"',
                   '#include "stm32_identity.h"']
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
    if any(item.kind in {"adc", "pwm_adc", "adc_group"} for item in result.controllers):
        selected.add("ADC12")
    if any(item.kind == "pwm_adc" for item in result.controllers):
        selected.update({"DMA1", "DMAMUX1"})
    for item in result.controllers:
        if item.kind == "adc_group":
            if item.mode == "regular-sequence-dma":
                selected.update({"DMA1", "DMAMUX1"})
            if item.options.trigger == "tim6-trgo":
                selected.add("TIM6")
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
    if any(item.kind in {"adc", "pwm_adc", "adc_group"} for item in result.controllers):
        source += ['static bool s_nx_platform_adc_mux_owned;']
    if result.controllers and not gpio_phase.applies(result):
        from .preflight import admission_declarations
        source += admission_declarations(result, "NULL", 0)
    return source


def start_system(result):
    source = []
    if any(item.kind == "adc_group" for item in result.controllers):
        from . import adc_group
        source += adc_group.preflight(result)
    source += preflight.emit(result)
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
    if not result.controllers:
        return []
    return ['    result.primary = nx_stm32g474_platform_resources_start(&s_nx_platform_config);',
            '    if (result.primary != NX_SUCCESS) {',
            f'        result.cleanup = {cleanup};',
            '        result.remaining_effects = result.cleanup == NX_SUCCESS ? 0u : 1u;',
            '        return result;', '    }']


def stop_system(result, *, system_phase=None):
    source = gpio_phase.release(result)
    if result.controllers:
        source += ['    nx_result_t resources = nx_stm32g474_platform_resources_stop(&s_nx_platform_config);',
                   '    if (resources != NX_SUCCESS) { return resources; }']
    if system_phase is not None:
        source.append(f'    {system_phase};')
    return source + ['    if (s_nx_platform_started) {',
                     '        nx_result_t system = nx_stm32g474_system_stop(&g_nx_stm32g474_system, 1000000u);',
                     '        if (system != NX_SUCCESS) { return system; }',
                     '    }']
