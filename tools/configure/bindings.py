"""Static faces and common lifecycle order; SoC modules emit hardware wiring."""
try:
    from .providers import maintained
    from .ir import ConfigurationIR
except ImportError:
    from providers import maintained
    from ir import ConfigurationIR


def symbol(name):
    return name.replace("-", "_")


def emit(result):
    provider = maintained(result["soc_family"])
    if not isinstance(result, ConfigurationIR):
        result = ConfigurationIR.from_validated(result)
    selections = result.controllers
    header = ['/**', ' * \\file            nexus_bindings.h',
              ' * \\brief           Fixed typed aliases selected by one assembly.',
              ' * \\author          Nexus Team', ' */',
              '#ifndef NEXUS_BINDINGS_H', '#define NEXUS_BINDINGS_H',
              '#include "nexus/core/status.h"']
    for kind in sorted({item["kind"] for item in selections}):
        header.append(f'#include "nexus/io/{"timer" if kind == "pwm" else kind}.h"')
    header += ['#ifdef __cplusplus', 'extern "C" {', '#endif', 'typedef struct {', '    nx_result_t primary;',
               '    nx_result_t cleanup;', '    uint32_t remaining_effects;',
               '} nx_platform_start_result_t;',
               '/** \\brief Startup-only; no scheduler or product worker is created.',
               ' * \\return Primary/cleanup errors and retained effects. On error',
               ' * all ports remain unusable; preserve contexts until cleanup.',
               ' */', 'nx_platform_start_result_t nx_platform_start(void);',
               '/** \\brief Stop after direct writers/IRQ consumers quiesce.',
               ' * \\return SUCCESS after drain; BUSY retains all storage and clock.',
               ' * The owner must continue service before retrying BUSY.',
               ' */', 'nx_result_t nx_platform_stop(void);']
    source = ['/**', ' * \\file            bindings.c',
              ' * \\brief           Generated fixed typed hardware construction.',
              ' * \\author          Nexus Team', ' * \\version         1.0.0',
              ' * \\date            2026-10-09',
              ' * \\copyright       Copyright (c) 2026 Nexus Team', ' */',
              '#include "nexus_bindings.h"', '#include "nexus_factory.h"',
              '#include "nexus_config.h"',
              '#include "nexus/arch/arch.h"']
    source.extend(provider.includes(result))
    source += ['static bool s_nx_platform_started;', 'static size_t s_nx_platform_initialized;']
    source.extend(provider.state_declarations(result))
    initialize = []
    stop = []
    for item in selections:
        name = symbol(item.id)
        exported_type = f"nx_{item.kind}_port_t"
        header += ['/** \\brief Fixed static identity; lifecycle stays explicit. */',
                   f'extern const {exported_type}* const nx_binding_{name};']
        implementation = provider.implementation(item)
        prefix = provider.PREFIX
        source += [f'static nx_{prefix}_{implementation}_state_t s_nx_port_{name};',
                   f'static const {exported_type} s_nx_face_{name} = {{',
                   f'    &nx_{prefix}_{implementation}_ops, &s_nx_port_{name}', '};',
                   f'const {exported_type}* const nx_binding_{name} = &s_nx_face_{name};']
        source.extend(provider.instance_declarations(item, result))
        construction = provider.constructor(item, result, result["board_bindings"],
                                            result["routes"], result.devices)
        source.extend(construction["definitions"])
        initialize.append(construction["initialize"])
        stop.extend((len(initialize), expression)
                    for expression in construction["stop"])
    for device in result["devices"]:
        controller = next(item for item in selections if item["id"] == device["controller"])
        kind = controller["kind"]
        header.append(f'extern const nx_{kind}_endpoint_t* const nx_device_{symbol(device["id"])};')
    source.extend(provider.shared_irqs(result))
    source += ['/** \\brief Stop only the initialized selected interfaces. */',
               'nx_result_t nx_platform_stop(void) {',
               '    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) { return NX_ERROR_CONTEXT; }']
    for index in range(len(initialize), 0, -1):
        source.append(f'    if (s_nx_platform_initialized >= {index}u) {{')
        for counter, (_, expression) in enumerate(entry for entry in stop if entry[0] == index):
            source += [f'        nx_result_t status_{counter} = {expression};',
                       f'        if (status_{counter} != NX_SUCCESS) {{ return status_{counter}; }}']
        source += [f'        s_nx_platform_initialized = {index - 1}u;', '    }']
    source.extend(provider.stop_system(result))
    source += ['    s_nx_platform_started = false;', '    s_nx_platform_initialized = 0u;', '    return NX_SUCCESS;', '}']
    source += ['/** \\brief Start explicit hardware only; rollback retains error facts. */',
               'nx_platform_start_result_t nx_platform_start(void) {',
               '    nx_platform_start_result_t result = { NX_SUCCESS, NX_SUCCESS, 0u };',
               '    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {',
               '        result.primary = NX_ERROR_CONTEXT; return result; }',
               '    if (s_nx_platform_started) { result.primary = NX_ERROR_STATE; return result; }']
    source.extend(provider.start_system(result))
    source.append("    s_nx_platform_started = true;")
    source.extend(provider.start_resources(result))
    for index, expression in enumerate(initialize):
        source += [f'    result.primary = {expression};',
                   '    if (result.primary != NX_SUCCESS) {',
                   '        result.cleanup = nx_platform_stop();',
                   '        result.remaining_effects = result.cleanup == NX_SUCCESS ? 0u : 1u;',
                   '        return result;', '    }', f'    s_nx_platform_initialized = {index + 1}u;']
    source += ['    return result;', '}']
    header += ['#ifdef __cplusplus', '}', '#endif', '#endif', '']
    return '\n'.join(header), '\n'.join(source) + '\n'
