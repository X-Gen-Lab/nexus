"""Static provider wiring and family system lifecycle emission."""
from . import bindings


def implementation(item):
    from . import mode_implementation
    return mode_implementation(item.kind, item.mode)


def includes(result):
    selections = result.controllers
    source = []
    source += ['#include "gd32f470_provider.h"', '#include "gd32f4xx.h"']
    if any(item["kind"] == "uart" and item["mode"] == "dma-tx" for item in selections):
        source.append('#include "gd32f470_dma.h"')
    if any(item["kind"] == "spi" and item["mode"] == "dma-full-duplex" for item in selections):
        source.append('#include "gd32f470_spi_dma.h"')
    if any(item["kind"] == "adc" and item["mode"] == "trigger-dma" for item in selections):
        source.append('#include "gd32f470_adc_stream.h"')
    if any(item["kind"] == "uart" and item["mode"] == "irq-blocks" for item in selections):
        source.append('#include "gd32f470_uart_stream.h"')
    source += ['void _init(void);', 'void _fini(void);',
               r'/** \brief C runtime hooks; constructors use .init_array. */',
               'void _init(void) {}', 'void _fini(void) {}']
    return source


def instance_declarations(item, result):
    source = []
    name = item.id.replace("-", "_")
    kind = item.kind
    if kind in {"spi", "i2c"}:
        source.append('static nx_gd32'
                      f"_{implementation(item)}_endpoint_state_t "
                      f"s_nx_endpoint_{name};")
    return source


def has_dma(result):
    return any(item.mode.startswith("dma") or item.mode == "trigger-dma"
               for item in result.controllers)


def state_declarations(result):
    source = []
    if has_dma(result):
        source.append("static uint32_t s_nx_platform_dma_clocks_added;")
    return source


def shared_irqs(result):
    del result
    return []


def stop_system(result):
    dma_clock = has_dma(result)
    source = []
    if dma_clock:
        source += ['    if (s_nx_platform_started) { RCU_AHB1EN &= ~s_nx_platform_dma_clocks_added; }',
                   '    nx_arch_dsb();']
    source += ['    if (s_nx_platform_started && nx_gd32_soc_stop() != 0) { return NX_ERROR_IO; }']
    return source


def start_system(result):
    dma_clock = has_dma(result)
    source = []
    if dma_clock:
        source.append('    s_nx_platform_dma_clocks_added = 0u;')
    source += ['    if (nx_gd32_soc_start() != 0) {',
               '        result.primary = NX_ERROR_IO;',
               '        result.cleanup = nx_gd32_soc_stop() == 0 ? NX_SUCCESS : NX_ERROR_IO;',
               '        result.remaining_effects = result.cleanup == NX_SUCCESS ? 0u : 1u;',
               '        s_nx_platform_started = result.remaining_effects != 0u;',
               '        return result;', '    }']
    return source


def start_resources(result):
    source = []
    if has_dma(result):
        source += ['    s_nx_platform_dma_clocks_added = ~RCU_AHB1EN & RCU_AHB1EN_DMA1EN;',
                   '    RCU_AHB1EN |= RCU_AHB1EN_DMA1EN;',
                   '    (void)RCU_AHB1EN;', '    nx_arch_dsb();']
    return source
