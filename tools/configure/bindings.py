"""Static faces and common lifecycle order; SoC modules emit hardware wiring."""
from tools.configure.providers import for_selection
from tools.configure.ir import ConfigurationIR
from tools.configure.factory import (interface_type, public_header,
                                     controller_class, controller_ordinals)
from tools.configure import gpio_phase
from tools.configure import enhanced_endpoints
from tools.configure import stream_profiles, uart_formats
from tools.configure.calls import DirectCall, Symbol
from tools.configure.providers.common import fail


def symbol(name):
    return name.replace("-", "_")


def _sequence(calls, indent="        "):
    """Connect a fixed typed call sequence with its normal success contract."""
    source = []
    for index, call in enumerate(calls[:-1]):
        source += [f"{indent}nx_result_t status_{index} = {call};",
                   f"{indent}if (status_{index} != NX_SUCCESS) {{",
                   f"{indent}    return status_{index};", f"{indent}}}"]
    source.append(f"{indent}return {calls[-1] if calls else 'NX_SUCCESS'};")
    return source


def _platform_connections(result, provider):
    """Emit fixed hooks; model platforms explicitly have no system hardware."""
    specifications = (
        ("nx_platform_start_result_t", "system_start", "void"),
        ("nx_result_t", "resources_acquire", "void"),
        ("nx_result_t", "resources_release", "void"),
        ("nx_result_t", "system_release", "bool started"))
    source = []
    if not provider.MODEL:
        required = ("platform_declarations", *("platform_" + name
                    for _, name, _ in specifications))
        if any(not callable(getattr(provider, name, None)) for name in required):
            fail("Physical provider requires typed platform lifecycle hooks")
        source.extend(provider.platform_declarations(result))
    for result_type, name, parameters in specifications:
        source += [r"/** \brief Fixed connection to the selected platform owner. */",
                   f"static {result_type} nx_platform_{name}({parameters}) {{"]
        if provider.MODEL:
            if name == "system_release":
                source.append("    (void)started;")
            value = ("(nx_platform_start_result_t){ NX_SUCCESS, NX_SUCCESS, 0u }"
                     if name == "system_start" else "NX_SUCCESS")
        else:
            hook = getattr(provider, "platform_" + name)
            call = (hook(result, Symbol("started"))
                    if name == "system_release" else hook(result))
            if not isinstance(call, DirectCall):
                fail("Platform lifecycle hooks require fixed typed calls")
            value = call.render()
        source += [f"    return {value};", "}"]
    return source


def _controller_dispatch(initialize, admission, ordinals, retained, stop):
    """Emit only indexed typed calls and constant failure responsibility."""
    source = [r"/** \brief Exact controller admission without effects. */",
              "static nx_result_t nx_platform_admit(size_t index) {",
              "    switch (index) {"]
    for index, ordinal in enumerate(ordinals):
        calls = [call for owner, call in admission if owner == ordinal]
        source += [f"    case {index}u: {{", *_sequence(calls), "    }"]
    source += ["    default: return NX_ERROR_INVALID;", "    }", "}",
               r"/** \brief Exact constructor and static failure responsibility. */",
               "static nx_lifecycle_acquisition_t nx_platform_acquire(size_t index) {",
               "    switch (index) {"]
    for index, call in enumerate(initialize):
        source += [f"    case {index}u:",
                   "        return (nx_lifecycle_acquisition_t){ " + call +
                   ", " + str(retained[index]).lower() + " };"]
    source += ["    default:",
               "        return (nx_lifecycle_acquisition_t){ NX_ERROR_INVALID, false };",
               "    }", "}",
               r"/** \brief Exact selected controller withdrawal calls. */",
               "static nx_result_t nx_platform_release(size_t index) {",
               "    switch (index) {"]
    for index in range(len(initialize)):
        calls = [call for owner, call in stop if owner == index + 1]
        source += [f"    case {index}u: {{", *_sequence(calls), "    }"]
    return source + ["    default: return NX_ERROR_INVALID;", "    }", "}"]


def emit(result):
    if not isinstance(result, ConfigurationIR):
        raise TypeError("Bindings emission requires ConfigurationIR")
    provider = for_selection(result)
    if result.runtime_power is not None:
        fail("Selected provider has no maintained runtime power lifecycle")
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
              '#include "nexus/core/platform_lifecycle.h"']
    if uart_formats.probed(result):
        header.insert(-1, '#include "nexus_config.h"')
    for kind in sorted({controller_class(item) for item in selections}):
        header.append(f'#include "nexus/io/{public_header(kind)}.h"')
    for kind in sorted({enhanced_endpoints.class_name(item)
                        for item in result.devices} &
                       {"spi_transaction_device", "i2c_long_device",
                        "spi_dma_transaction_device", "memory_window",
                        "parallel16"}):
        header.append(f'#include "nexus/io/{public_header(kind)}.h"')
    if any(item.kind in {"eth", "eth_frames"} for item in selections):
        header.append('#include "nexus/io/mdio.h"')
    header += ['#ifdef __cplusplus', 'extern "C" {', '#endif',
               '/** \\brief Startup-only; no scheduler or product worker is created.',
               ' * \\return Primary/cleanup errors and retained effects. On error',
               ' * all ports remain unusable; preserve contexts until cleanup.',
               ' * Unprovable cold rollback may mask IRQs and enter the platform',
               ' * fault observer, then park; it never returns an ownerless error.',
               ' */', 'nx_platform_start_result_t nx_platform_start(void);',
               '/** \\brief Stop after direct writers/IRQ consumers quiesce.',
               ' * \\return SUCCESS after drain and release. BUSY or IO retains',
               ' * incomplete leases; preserve all contexts and retry cleanup.',
               ' * The owner must continue service before retrying BUSY.',
               ' * Complete release restores the selected original pin states;',
               ' * retain ownership if the product requires continued idle drive.',
               ' */', 'nx_result_t nx_platform_stop(void);',
               '/** \\brief Caller-owned output, accessed only during this call.',
               ' * Controller indices are zero-based resolved.json controllers',
               ' * positions, not typed factory IDs. SIZE_MAX means no controller.',
               ' * NX_PLATFORM_CONTROLLER_INDEX_* names those ordinals. Optional',
               ' * factory capabilities provide immutable labels and bounds.',
               ' * Success clears all fields. Synchronous BUSY reentry preserves',
               ' * this output; other failures identify the failing stage.',
               ' */',
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
    header += [
        '/** \\brief Resolved controller ordinals; not factory IDs or a wire ABI. */',
        'enum {']
    header.extend(f'    {name} = {index},'
                  for index, _, name in controller_ordinals(result))
    header += [f'    NX_PLATFORM_CONTROLLER_INDEX_COUNT = {len(selections)}', '};']
    source = ['/**', ' * \\file            bindings.c',
              ' * \\brief           Generated fixed typed hardware construction.',
              ' * \\author          Nexus Team', ' * \\version         1.0.0',
              ' * \\date            2026-10-09',
              ' * \\copyright       Copyright (c) 2026 Nexus Team', ' */',
              '#include "nexus_config.h"',
              '#include "nexus_bindings.h"', '#include "nexus_factory.h"',
              '#include "nexus/arch/arch.h"']
    if uart_formats.probed(result):
        source.append('#include "nexus/io/cycle_probe.h"')
        source += [f"NX_IO_IRQ_PROBE_DECLARE({item.controller})"
                   for item in uart_formats.probed(result)]
    source.extend(provider.includes(result))
    geometry = getattr(provider, "FLASH_GEOMETRY", None)
    if geometry is not None and any(item.kind == "flash"
                                    for item in selections):
        from tools.configure.memory import flash_geometry
        source.extend(flash_geometry(
            result, symbol=geometry[0], sector_symbol=geometry[0] + "_sectors",
            program_unit=provider.FLASH_PROGRAM_UNIT,
            program_flags=geometry[1], static=False))
    source += ['static nx_platform_lifecycle_state_t s_nx_platform_lifecycle;']
    source.extend(provider.state_declarations(result))
    initialize = []
    admission = []
    initialization_indices = []
    retained_initialization = []
    stop = []
    pin_snapshots = {}
    shared_members = {member.id: owner for owner in result.pwm_shared_owners
                      for member in owner.members}
    physical_constructed = set()
    pre_c_effects = False
    if shared_members:
        pwm_shared = provider.PWM_SHARED
    for selection_index, item in enumerate(selections):
        name = symbol(item.id)
        exported_type = interface_type(controller_class(item), "port")
        header += ['/** \\brief Fixed static identity; lifecycle stays explicit. */',
                   f'extern const {exported_type}* const nx_binding_{name};']
        if item.id in shared_members:
            owner = shared_members[item.id]
            physical = pwm_shared.state_symbol(owner)
            if physical not in physical_constructed:
                construction = pwm_shared.constructor(owner)
                pre_c_effects |= construction.pre_c_effects
                initial = (" = " + construction.initial_state.render()
                           if construction.initial_state is not None else "")
                source.append(f'static {construction.state_type} {physical}{initial};')
                source.extend(construction.definitions)
                for sequence in construction.initialize_sequences:
                    source.extend(sequence.render())
                admission.extend((selection_index, call.render())
                                 for call in construction.admission)
                initialize.append(construction.initialize.render())
                initialization_indices.append(selection_index)
                retained_initialization.append(construction.retain_on_failed_init)
                stop.extend((len(initialize), expression.render())
                            for expression in construction.stop)
                physical_constructed.add(physical)
            source.extend(pwm_shared.face(item, owner))
            continue
        construction = provider.constructor(item, result, result.board_bindings,
                                            result.routes, result.devices)
        pre_c_effects |= construction.pre_c_effects
        initial = (" = " + construction.initial_state.render()
                   if construction.initial_state is not None else "")
        source += [f'static {construction.state_type} s_nx_port_{name}{initial};']
        source.extend(construction.face_definitions)
        face_context = construction.face_context or f"&s_nx_port_{name}"
        source += [
                   f'static const {exported_type} s_nx_face_{name} = {{',
                   f'    &{provider.operations(item)}, {face_context}', '};',
                   f'const {exported_type}* const nx_binding_{name} = &s_nx_face_{name};']
        source.extend(stream_profiles.emit_instance(item))
        source.extend(provider.instance_declarations(item, result))
        source.extend(construction.definitions)
        for sequence in construction.initialize_sequences:
            source.extend(sequence.render())
        admission.extend((selection_index, call.render())
                         for call in construction.admission)
        header.extend(construction.exports)
        if gpio_phase.applies(result):
            gpio_phase.add_snapshots(result, item,
                                     construction.pin_snapshots,
                                     pin_snapshots)
        initialize.append(construction.initialize.render())
        initialization_indices.append(selection_index)
        retained_initialization.append(construction.retain_on_failed_init)
        stop.extend((len(initialize), expression.render())
                    for expression in construction.stop)
    for device in result.devices:
        class_name = enhanced_endpoints.class_name(device)
        header.append(f'extern const {interface_type(class_name, "endpoint")}* const nx_device_{symbol(device.id)};')
    source.extend(provider.shared_irqs(result))
    # All constructor arrays precede their shared cold lease functions.
    # Reuse those static slots directly; no pointer registry is emitted.
    source.extend(gpio_phase.declarations(result, pin_snapshots))
    source.extend(_platform_connections(result, provider))
    source += [r'/** \brief Context is admitted, never established by bindings. */',
               'static nx_result_t nx_platform_context_check(void) {',
               f'    return ({context_guard}) ? NX_ERROR_CONTEXT : NX_SUCCESS;',
               '}']
    source.extend(_controller_dispatch(initialize, admission,
                                         initialization_indices,
                                         retained_initialization, stop))
    if initialization_indices:
        source += ['static const size_t s_nx_platform_ordinals[] = {',
                   '    ' + ', '.join(f'{index}u'
                                     for index in initialization_indices), '};']
    source += ['static const nx_platform_lifecycle_plan_t s_nx_platform_plan = {',
               f'    .controller_count = {len(initialize)}u,',
               '    .ordinals = ' + ('s_nx_platform_ordinals'
                                    if initialization_indices else 'NULL') + ',',
               '    .context_check = nx_platform_context_check,',
               '    .admit = nx_platform_admit,',
               '    .system_start = nx_platform_system_start,',
               '    .resources_acquire = nx_platform_resources_acquire,',
               '    .resources_release = nx_platform_resources_release,',
               '    .system_release = nx_platform_system_release,',
               '    .acquire = nx_platform_acquire,',
               '    .release = nx_platform_release,',
               f'    .permanent_effects = {str(pre_c_effects).lower()}', '};',
               r'/** \brief Start the static platform through its C coordinator. */',
               'nx_platform_start_result_t nx_platform_start(void) {',
               '    return nx_platform_lifecycle_start(',
               '        &s_nx_platform_plan, &s_nx_platform_lifecycle, NULL);', '}',
               r'/** \brief Add call-scoped metadata to the same C startup path. */',
               'nx_platform_start_result_t nx_platform_start_diagnosed(nx_platform_diagnostic_t* diagnostic) {',
               '    return nx_platform_lifecycle_start(',
               '        &s_nx_platform_plan, &s_nx_platform_lifecycle, diagnostic);',
               '}',
               r'/** \brief Stop and retry only the remaining C owner receipts. */',
               'nx_result_t nx_platform_stop(void) {',
               '    return nx_platform_lifecycle_stop(',
               '        &s_nx_platform_plan, &s_nx_platform_lifecycle, NULL);', '}',
               r'/** \brief Add call-scoped metadata to the same C stop path. */',
               'nx_result_t nx_platform_stop_diagnosed(nx_platform_diagnostic_t* diagnostic) {',
               '    return nx_platform_lifecycle_stop(',
               '        &s_nx_platform_plan, &s_nx_platform_lifecycle, diagnostic);',
               '}']
    header += ['#ifdef __cplusplus', '}', '#endif', '#endif', '']
    return '\n'.join(header), '\n'.join(source) + '\n'
