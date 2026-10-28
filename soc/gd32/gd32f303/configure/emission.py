"""Exact legacy GPIO lease and fixed IRC8 lifecycle; no runtime registry."""


def implementation(item):
    from .contract import mode_implementation
    return mode_implementation(item.kind, item.mode)


def operations(item):
    if item.kind == "uart" and item.mode != "irq-bytes":
        return ("nx_uart_v1_channel_dma_tx_ops" if item.mode == "dma-tx" else
                "nx_uart_v1_channel_dma_ops")
    if item.kind == "i2c":
        return "nx_i2c_v1_async_ops"
    if item.kind in {"adc", "pwm", "flash"}:
        return f"nx_legacy_{item.kind}_ops"
    return f"nx_gd32f303_{implementation(item)}_ops"


def includes(result):
    del result
    return ['#include "gd32f303_provider.h"', '#include "gd32f303_transport.h"', 'void _init(void);',
            'void _fini(void);',
            r'/** \brief Newlib runtime hooks; no dynamic constructor policy. */',
            'void _init(void) {}', 'void _fini(void) {}']


def instance_declarations(item, result):
    del item, result
    return []


def shared_irqs(result):
    del result
    return []


def _pins(result):
    pins = {int(pin["pin"][2:])
            for item in result.controllers for pin in item.pins
            if pin["pin"].startswith("PA")}
    for child in result.devices:
        if child.kind == "spi" and child.options.cs_binding is not None:
            binding = result.board_bindings[child.options.cs_binding]
            pins.update(int(pin["pin"][2:])
                        for pin in result.routes[binding["route"]]["pins"])
    return sum(1 << pin for pin in sorted(pins))


def _selection(result):
    kinds = {"uart", "spi", "adc", "pwm", "flash"}
    peripherals = []
    exti = 0
    for item in result.controllers:
        if item.kind in kinds:
            peripherals.append("NX_GD32F303_PLATFORM_" + item.kind.upper())
        if item.kind == "uart" and item.mode in {"dma-tx", "dma-duplex"}:
            peripherals.append("NX_GD32F303_PLATFORM_DMA_TX")
            if item.mode == "dma-duplex":
                peripherals.append("NX_GD32F303_PLATFORM_DMA_RX")
        if item.kind == "exti":
            exti |= 1 << int(item.controller[-1])
    return _pins(result), tuple(peripherals), exti


def state_declarations(result):
    pins, peripherals, exti = _selection(result)
    if not (pins or peripherals or exti):
        return []
    selected = " | ".join(peripherals) or "0u"
    source = ["static const nx_gd32f303_platform_config_t s_nx_cold_selection = {",
              f"    .pins = {pins}u, .peripherals = {selected},",
              f"    .exti_lines = {exti}u", "};"]
    if pins:
        source += ["static nx_gd32f303_platform_resources_t s_nx_cold_resources;"]
    return source


def start_system(result):
    source = []
    if any(_selection(result)):
        source += ["    result.primary = nx_gd32f303_platform_preflight(&s_nx_cold_selection);",
                   "    if (result.primary != NX_SUCCESS) { return result; }"]

    return source + [
        "    nx_gd32f303_start_result_t system = nx_gd32f303_system_start(&g_nx_gd32f303_system, 1000000u);",
        "    if (system.primary != NX_SUCCESS) {",
        "        result.primary = system.primary;",
        "        result.cleanup = system.cleanup;",
        "        result.remaining_effects = system.remaining_effects;",
        "        s_nx_platform_started = result.remaining_effects != 0u;",
        "        return result;", "    }"]


def start_resources(result, *, cleanup="nx_platform_stop()"):
    if not _pins(result):
        return []
    return [
        "    result.primary = nx_gd32f303_platform_resources_acquire(&s_nx_cold_resources, &s_nx_cold_selection);",
        "    if (result.primary != NX_SUCCESS) {",
        f"        result.cleanup = {cleanup};",
        "        result.remaining_effects = result.cleanup == NX_SUCCESS ? 0u : 1u;",
        "        return result;", "    }"]


def stop_system(result, *, system_phase=None):
    source = []
    if _pins(result):
        source += ["    nx_result_t resources = nx_gd32f303_platform_resources_release(&s_nx_cold_resources);",
                   "    if (resources != NX_SUCCESS) { return resources; }"]
    if system_phase is not None:
        source.append(f"    {system_phase};")
    return source + ["    if (s_nx_platform_started) {",
        "        nx_result_t system = nx_gd32f303_system_stop(&g_nx_gd32f303_system);",
        "        if (system != NX_SUCCESS) { return system; }", "    }"]
