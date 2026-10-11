"""Static provider wiring and family system lifecycle emission."""
from . import bindings


def implementation(item):
    from . import mode_implementation
    return mode_implementation(item.kind, item.mode)


def includes(result):
    selections = result.controllers
    source = []
    source.append('#include "stm32f407_provider.h"')
    if any(item["mode"].startswith("dma") for item in selections):
        source.append('#include "stm32f407_dma.h"')
    if any(item["mode"] in {"irq-blocks", "trigger-dma"} for item in selections):
        source.append('#include "stm32f407_stream.h"')
    source += ['void _init(void);', 'void _fini(void);',
               r'/** \brief C runtime hooks; constructors use .init_array. */',
               'void _init(void) {}', 'void _fini(void) {}']
    return source


def instance_declarations(item, result):
    source = []
    name = item.id.replace("-", "_")
    kind = item.kind
    if kind in {"spi", "i2c"}:
        source.append('static nx_stm32'
                      f"_{implementation(item)}_endpoint_state_t "
                      f"s_nx_endpoint_{name};")
    return source


def _gpio_clock_mask(result):
    selections = result.controllers
    pins = [pin["pin"] for item in selections for pin in item["pins"]]
    for device in result["devices"]:
        if "cs_binding" in device:
            binding = result["board_bindings"][device["cs_binding"]]
            pins.extend(pin["pin"] for pin in result["routes"][binding["route"]]["pins"])
    return sum(1 << index for index in {ord(pin[1]) - ord("A") for pin in pins})


def has_dma(result):
    return any(item.mode.startswith("dma") or item.mode == "trigger-dma"
               for item in result.controllers)


def state_declarations(result):
    selections = result.controllers
    source = []
    if has_dma(result):
        source.append("static uint32_t s_nx_platform_dma_clocks_added;")
    gpio_mask = _gpio_clock_mask(result)
    if gpio_mask:
        source.append('static uint32_t s_nx_platform_gpio_clocks_added;')
    if any(item["kind"] == "exti" for item in selections):
        source.append('static uint32_t s_nx_platform_syscfg_clock_added;')
    return source


def shared_irqs(result):
    return bindings.shared_irq_definitions(result.controllers)


def stop_system(result):
    selections = result.controllers
    dma_clock = has_dma(result)
    source = []
    gpio_clock_mask = _gpio_clock_mask(result)
    if gpio_clock_mask:
        source += ['    if (s_nx_platform_started) { RCC->AHB1ENR &= ~s_nx_platform_gpio_clocks_added; }']
    if any(item["kind"] == "exti" for item in selections):
        source += ['    if (s_nx_platform_started) { RCC->APB2ENR &= ~s_nx_platform_syscfg_clock_added; }']
    if dma_clock:
        source += ['    if (s_nx_platform_started) { RCC->AHB1ENR &= ~s_nx_platform_dma_clocks_added; }']
    source.append('    nx_arch_dsb();')
    source += ['    if (s_nx_platform_started && nx_stm32_system_stop(&g_nx_stm32_system, 1000000u) != 0) {',
               '        return NX_ERROR_IO;', '    }']
    return source


def start_system(result):
    selections = result.controllers
    dma_clock = has_dma(result)
    source = []
    gpio_clock_mask = _gpio_clock_mask(result)
    if dma_clock:
        source.append('    s_nx_platform_dma_clocks_added = 0u;')
    if gpio_clock_mask:
        source.append('    s_nx_platform_gpio_clocks_added = 0u;')
    if any(item["kind"] == "exti" for item in selections):
        source.append('    s_nx_platform_syscfg_clock_added = 0u;')
    source += [f'    result.primary = nx_stm32_validate_identity({result["flash"]["size"]}u);',
               '    if (result.primary != NX_SUCCESS) { return result; }']
    source += ['    nx_stm32_start_result_t system = nx_stm32_system_start(&g_nx_stm32_system, 1000000u);',
               '    if (system.primary != 0) {',
               '        result.primary = NX_ERROR_IO;',
               '        result.cleanup = system.cleanup == 0 ? NX_SUCCESS : NX_ERROR_IO;',
               '        result.remaining_effects = system.remaining_effects;',
               '        s_nx_platform_started = result.remaining_effects != 0u;',
               '        return result;', '    }']
    return source


def start_resources(result):
    selections = result.controllers
    source = []
    if has_dma(result):
        source += ['    s_nx_platform_dma_clocks_added = ~RCC->AHB1ENR & RCC_AHB1ENR_DMA2EN;',
                   '    RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;',
                   '    (void)RCC->AHB1ENR;', '    nx_arch_dsb();']
    gpio_clock_mask = _gpio_clock_mask(result)
    if gpio_clock_mask:
        source.append(f'    s_nx_platform_gpio_clocks_added = ~RCC->AHB1ENR & {gpio_clock_mask}u;')
    if any(item["kind"] == "exti" for item in selections):
        source.append('    s_nx_platform_syscfg_clock_added = ~RCC->APB2ENR & RCC_APB2ENR_SYSCFGEN;')
    return source
