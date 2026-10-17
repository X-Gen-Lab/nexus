"""Static faces and common lifecycle order; SoC modules emit hardware wiring."""
try:
    from .providers import maintained
    from .ir import ConfigurationIR
    from .factory import interface_type, public_header, controller_class
    from . import gpio_phase
    from . import enhanced_endpoints
    from . import stream_profiles, uart_formats
except ImportError:
    from providers import maintained
    from ir import ConfigurationIR
    from factory import interface_type, public_header, controller_class
    import gpio_phase
    import enhanced_endpoints
    import stream_profiles
    import uart_formats


def symbol(name):
    return name.replace("-", "_")


def diagnostic_helpers():
    """Emit call-scoped metadata helpers, without retained diagnostic storage."""
    return [
        '/** \\brief Reset only the supplied call-scoped output. */',
        'static void nx_platform_diagnostic_reset(nx_platform_diagnostic_t* diagnostic) {',
        '    if (diagnostic != NULL) {',
        '        *diagnostic = (nx_platform_diagnostic_t){',
        '            NX_PLATFORM_PHASE_NONE, SIZE_MAX,',
        '            NX_PLATFORM_PHASE_NONE, SIZE_MAX };',
        '    }', '}',
        '/** \\brief Select the primary or cleanup path without replacing errors. */',
        'static void nx_platform_diagnostic_phase(nx_platform_diagnostic_t* diagnostic,',
        '                                         bool cleanup, nx_platform_phase_t phase,',
        '                                         size_t controller_index) {',
        '    if (diagnostic == NULL) { return; }',
        '    if (cleanup) {',
        '        diagnostic->cleanup_phase = phase;',
        '        diagnostic->cleanup_controller_index = controller_index;',
        '    } else {',
        '        diagnostic->phase = phase;',
        '        diagnostic->controller_index = controller_index;',
        '    }', '}',
        'static nx_result_t nx_platform_stop_impl(nx_platform_diagnostic_t* diagnostic, bool cleanup);',
        'static nx_platform_start_result_t nx_platform_start_impl(nx_platform_diagnostic_t* diagnostic);',
    ]


def emit(result):
    provider = maintained(result["soc_family"])
    if not isinstance(result, ConfigurationIR):
        result = ConfigurationIR.from_validated(result)
    selections = result.controllers
    context_guard = "nx_arch_in_isr() || nx_arch_irq_is_masked()"
    if not provider.MODEL:
        # Physical cold operations require a privileged hardware owner before
        # any system, clock, pin or vector lease can be modified.
        context_guard += " || !nx_arch_is_privileged()"
    header = ['/**', ' * \\file            nexus_bindings.h',
              ' * \\brief           Fixed typed aliases selected by one assembly.',
              ' * \\author          Nexus Team', ' */',
              '#ifndef NEXUS_BINDINGS_H', '#define NEXUS_BINDINGS_H',
              '#include "nexus/core/status.h"']
    if uart_formats.probed(result):
        header.insert(-1, '#include "nexus_config.h"')
    for kind in sorted({controller_class(item) for item in selections}):
        header.append(f'#include "nexus/io/{public_header(kind)}.h"')
    for kind in sorted({enhanced_endpoints.class_name(item)
                        for item in result.devices} &
                       {"spi_transaction_device", "i2c_long_device",
                        "spi_dma_transaction_device"}):
        header.append(f'#include "nexus/io/{public_header(kind)}.h"')
    if any(item.kind == "eth" for item in selections):
        header.append('#include "nexus/io/mdio.h"')
    header += ['#ifdef __cplusplus', 'extern "C" {', '#endif', 'typedef struct {', '    nx_result_t primary;',
               '    nx_result_t cleanup;', '    uint32_t remaining_effects;',
               '} nx_platform_start_result_t;',
               '/** \\brief Startup-only; no scheduler or product worker is created.',
               ' * \\return Primary/cleanup errors and retained effects. On error',
               ' * all ports remain unusable; preserve contexts until cleanup.',
               ' */', 'nx_platform_start_result_t nx_platform_start(void);',
               '/** \\brief Stop after direct writers/IRQ consumers quiesce.',
               ' * \\return SUCCESS after drain and release. BUSY or IO retains',
               ' * incomplete leases; preserve all contexts and retry cleanup.',
               ' * The owner must continue service before retrying BUSY.',
               ' * Complete release restores the selected original pin states;',
               ' * retain ownership if the product requires continued idle drive.',
               ' */', 'nx_result_t nx_platform_stop(void);',
               '/** \\brief Cold lifecycle stage; errors remain in the normal result. */',
               'typedef enum {',
               '    NX_PLATFORM_PHASE_NONE, NX_PLATFORM_PHASE_CONTEXT,',
               '    NX_PLATFORM_PHASE_SYSTEM, NX_PLATFORM_PHASE_RESOURCES,',
               '    NX_PLATFORM_PHASE_CONTROLLER',
               '} nx_platform_phase_t;',
               '/** \\brief Caller-owned output, accessed only during this call.',
               ' * Controller indices are zero-based resolved.json controllers',
               ' * positions, not typed factory IDs. SIZE_MAX means no controller.',
               ' * Success clears all fields; failure identifies the failing stage.',
               ' */', 'typedef struct {',
               '    nx_platform_phase_t phase;',
               '    size_t controller_index;',
               '    nx_platform_phase_t cleanup_phase;',
               '    size_t cleanup_controller_index;',
               '} nx_platform_diagnostic_t;',
               '/** \\brief Start with optional call-scoped failure metadata.',
               ' * \\param diagnostic Writable output or NULL; never retained.',
               ' * \\return The normal primary/cleanup/remaining-effects result.',
               ' * Context, deadline and ownership rules match nx_platform_start.',
               ' */',
               'nx_platform_start_result_t nx_platform_start_diagnosed(nx_platform_diagnostic_t* diagnostic);',
               '/** \\brief Stop with optional call-scoped failure metadata.',
               ' * \\param diagnostic Writable output or NULL; never retained.',
               ' * \\return The normal stop result; failure never releases a borrow.',
               ' * Context and ownership rules match nx_platform_stop.',
               ' */',
               'nx_result_t nx_platform_stop_diagnosed(nx_platform_diagnostic_t* diagnostic);']
    source = ['/**', ' * \\file            bindings.c',
              ' * \\brief           Generated fixed typed hardware construction.',
              ' * \\author          Nexus Team', ' * \\version         1.0.0',
              ' * \\date            2026-10-09',
              ' * \\copyright       Copyright (c) 2026 Nexus Team', ' */',
              '#include "nexus_config.h"',
              '#include "nexus_bindings.h"', '#include "nexus_factory.h"',
              '#include "nexus/arch/arch.h"']
    source += ["#ifndef NX_PLATFORM_REGISTER_WRITE",
               "#define NX_PLATFORM_REGISTER_WRITE(reg, value) ((reg) = (uint32_t)(value))",
               "#endif", "#ifndef NX_PLATFORM_PRIORITY_WRITE",
               "#define NX_PLATFORM_PRIORITY_WRITE(reg, value) ((reg) = (uint8_t)(value))",
               "#endif"]
    if uart_formats.probed(result):
        source.append('#include "nexus/io/cycle_probe.h"')
        source += [f"NX_IO_IRQ_PROBE_DECLARE({item.controller})"
                   for item in uart_formats.probed(result)]
    source.extend(provider.includes(result))
    source += ['static bool s_nx_platform_started;', 'static size_t s_nx_platform_initialized;']
    source.extend(provider.state_declarations(result))
    initialize = []
    retained_initialization = []
    stop = []
    pin_snapshots = {}
    for item in selections:
        name = symbol(item.id)
        exported_type = interface_type(controller_class(item), "port")
        header += ['/** \\brief Fixed static identity; lifecycle stays explicit. */',
                   f'extern const {exported_type}* const nx_binding_{name};']
        construction = provider.constructor(item, result, result["board_bindings"],
                                            result["routes"], result.devices)
        source += [f'static {construction.state_type} s_nx_port_{name};']
        source.extend(construction.face_definitions)
        face_context = construction.face_context or f"&s_nx_port_{name}"
        source += [
                   f'static const {exported_type} s_nx_face_{name} = {{',
                   f'    &{provider.operations(item)}, {face_context}', '};',
                   f'const {exported_type}* const nx_binding_{name} = &s_nx_face_{name};']
        source.extend(stream_profiles.emit_instance(item))
        source.extend(provider.instance_declarations(item, result))
        source.extend(construction.definitions)
        header.extend(construction.exports)
        if gpio_phase.applies(result):
            gpio_phase.add_snapshots(result, item,
                                     construction.pin_snapshots,
                                     pin_snapshots)
        initialize.append(construction.initialize)
        retained_initialization.append(construction.retain_on_failed_init)
        stop.extend((len(initialize), expression)
                    for expression in construction.stop)
    for device in result.devices:
        kind = device.kind
        class_name = enhanced_endpoints.class_name(device)
        header.append(f'extern const {interface_type(class_name, "endpoint")}* const nx_device_{symbol(device.id)};')
    source.extend(provider.shared_irqs(result))
    # All constructor arrays precede their shared cold lease functions.
    # Reuse those static slots directly; no pointer registry is emitted.
    source.extend(gpio_phase.declarations(result, pin_snapshots))
    source.extend(diagnostic_helpers())
    source += ['/** \\brief Stop only the initialized selected interfaces. */',
               'nx_result_t nx_platform_stop(void) {',
               '    return nx_platform_stop_impl(NULL, false);', '}',
               '/** \\brief Add caller-owned metadata to the same stop path. */',
               'nx_result_t nx_platform_stop_diagnosed(nx_platform_diagnostic_t* diagnostic) {',
               '    nx_platform_diagnostic_reset(diagnostic);',
               '    return nx_platform_stop_impl(diagnostic, false);', '}',
               '/** \\brief Shared stop body preserves primary failure metadata. */',
               'static nx_result_t nx_platform_stop_impl(nx_platform_diagnostic_t* diagnostic, bool cleanup) {',
               '    nx_platform_diagnostic_phase(diagnostic, cleanup, NX_PLATFORM_PHASE_CONTEXT, SIZE_MAX);',
               f'    if ({context_guard}) {{ return NX_ERROR_CONTEXT; }}']
    for index in range(len(initialize), 0, -1):
        source.append(f'    if (s_nx_platform_initialized >= {index}u) {{')
        source.append(
            '        nx_platform_diagnostic_phase(diagnostic, cleanup, '
            f'NX_PLATFORM_PHASE_CONTROLLER, {index - 1}u);')
        for counter, (_, expression) in enumerate(entry for entry in stop if entry[0] == index):
            source += [f'        nx_result_t status_{counter} = {expression};',
                       f'        if (status_{counter} != NX_SUCCESS) {{ return status_{counter}; }}']
        source += [f'        s_nx_platform_initialized = {index - 1}u;', '    }']
    source.append('    nx_platform_diagnostic_phase(diagnostic, cleanup, NX_PLATFORM_PHASE_RESOURCES, SIZE_MAX);')
    source.extend(provider.stop_system(
        result, system_phase='nx_platform_diagnostic_phase(diagnostic, cleanup, NX_PLATFORM_PHASE_SYSTEM, SIZE_MAX)'))
    source += ['    s_nx_platform_started = false;',
               '    s_nx_platform_initialized = 0u;',
               '    nx_platform_diagnostic_phase(diagnostic, cleanup, NX_PLATFORM_PHASE_NONE, SIZE_MAX);',
               '    return NX_SUCCESS;', '}']
    source += ['/** \\brief Start explicit hardware only; rollback retains error facts. */',
               'nx_platform_start_result_t nx_platform_start(void) {',
               '    return nx_platform_start_impl(NULL);', '}',
               '/** \\brief Add caller-owned metadata to the same startup path. */',
               'nx_platform_start_result_t nx_platform_start_diagnosed(nx_platform_diagnostic_t* diagnostic) {',
               '    nx_platform_diagnostic_reset(diagnostic);',
               '    nx_platform_start_result_t result = nx_platform_start_impl(diagnostic);',
               '    if (diagnostic != NULL && result.primary == NX_SUCCESS) {',
               '        nx_platform_diagnostic_reset(diagnostic);',
               '    } else if (diagnostic != NULL && result.cleanup != NX_SUCCESS &&',
               '               diagnostic->cleanup_phase == NX_PLATFORM_PHASE_NONE) {',
               '        diagnostic->cleanup_phase = NX_PLATFORM_PHASE_SYSTEM;',
               '        diagnostic->cleanup_controller_index = SIZE_MAX;',
               '    }', '    return result;', '}',
               '/** \\brief One shared startup implementation, without stored observers. */',
               'static nx_platform_start_result_t nx_platform_start_impl(nx_platform_diagnostic_t* diagnostic) {',
               '    nx_platform_start_result_t result = { NX_SUCCESS, NX_SUCCESS, 0u };',
               '    nx_platform_diagnostic_phase(diagnostic, false, NX_PLATFORM_PHASE_CONTEXT, SIZE_MAX);',
               f'    if ({context_guard}) {{',
               '        result.primary = NX_ERROR_CONTEXT; return result; }',
               '    if (s_nx_platform_started) { result.primary = NX_ERROR_STATE; return result; }']
    source.append('    nx_platform_diagnostic_phase(diagnostic, false, NX_PLATFORM_PHASE_SYSTEM, SIZE_MAX);')
    source.extend(provider.start_system(result))
    source.append("    s_nx_platform_started = true;")
    source.append('    nx_platform_diagnostic_phase(diagnostic, false, NX_PLATFORM_PHASE_RESOURCES, SIZE_MAX);')
    source.extend(provider.start_resources(
        result, cleanup='nx_platform_stop_impl(diagnostic, true)'))
    for index, expression in enumerate(initialize):
        source.append(
            '    nx_platform_diagnostic_phase(diagnostic, false, '
            f'NX_PLATFORM_PHASE_CONTROLLER, {index}u);')
        if retained_initialization[index]:
            # Cold failure may retain live hardware; include that instance
            # in reverse cleanup before attempting initialization.
            source.append(f"    s_nx_platform_initialized = {index + 1}u;")
        source += [f'    result.primary = {expression};',
                   '    if (result.primary != NX_SUCCESS) {',
                   '        result.cleanup = nx_platform_stop_impl(diagnostic, true);',
                   '        result.remaining_effects = result.cleanup == NX_SUCCESS ? 0u : 1u;',
                   '        return result;', '    }', f'    s_nx_platform_initialized = {index + 1}u;']
    source += ['    nx_platform_diagnostic_phase(diagnostic, false, NX_PLATFORM_PHASE_NONE, SIZE_MAX);',
               '    return result;', '}']
    header += ['#ifdef __cplusplus', '}', '#endif', '#endif', '']
    return '\n'.join(header), '\n'.join(source) + '\n'
