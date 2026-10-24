"""Compile/link bundle emission and shared private provider declarations."""
import json
try:
    from .providers.common import fail
except ImportError:
    from providers.common import fail


def dma_regions(name, result, include_flash=False, *, engine=None):
    """Emit immutable exact silicon DMA domains without runtime allocation."""
    if include_flash and any("dma_engines" in region
                             for region in result["memory"]):
        # RAM bus reachability never implies access to the Flash bus. New
        # heterogeneous engines need reviewed Flash facts before enabling it.
        fail("Engine-specific Flash reachability is not declared")
    regions = []
    for region in result["memory"]:
        if "dma_engines" in region:
            if engine is None or engine not in region["dma_engines"]:
                fail("DMA memory projection requires an exact declared engine")
            reachable = region["dma_engines"][engine]
        else:
            reachable = region["dma"]
        if reachable:
            regions.append(region)
    if not regions and not include_flash:
        return [], 0
    lines = [f"static const nx_dma_memory_region_t s_nx_dma_regions_{name}[] = {{"]
    for region in regions:
        lines.append(f"    {{ (uintptr_t)0x{region['origin']:08x}u, {region['size']}u, NX_DMA_MEMORY_READ | NX_DMA_MEMORY_WRITE }},")
    if include_flash:
        flash = result["flash"]
        lines.append(f"    {{ (uintptr_t)0x{flash['origin']:08x}u, {flash['size']}u, NX_DMA_MEMORY_READ }},")
    lines.append("};")
    return lines, len(regions) + int(include_flash)


def emit_bundle(result, output):
    """Emit compile/link inputs from resolved IR; CMake retains the code graph.

    Imports remain local because private provider declarations also use this
    module while the maintained provider registry is being initialized.
    """
    try:
        from . import cpu as cpu_profiles
        from . import kernel, memory as memory_regions, metadata, i2s2
        from .ir import thaw
        from .providers import descriptor, maintained
    except ImportError:
        import cpu as cpu_profiles
        import kernel
        import memory as memory_regions
        import metadata
        import i2s2
        from ir import thaw
        from providers import descriptor, maintained
    write = lambda name, data: (output / name).write_text(data, encoding="utf-8")
    write("resolved.json", json.dumps(result.to_dict() if hasattr(result, "to_dict")
                                      else result, indent=2, sort_keys=True) + "\n")
    # Compatibility fixtures enter through one adapter; normal generation uses
    # the already validated immutable CPU/kernel/instance decisions directly.
    try:
        from .ir import ConfigurationIR
    except ImportError:
        from ir import ConfigurationIR
    if not isinstance(result, ConfigurationIR):
        result = ConfigurationIR.from_validated(result)
    profile, policy = result.cpu_profile, result.kernel
    header = ["/* Generated from the sole resolved configuration. */",
              "#ifndef NEXUS_CONFIG_H", "#define NEXUS_CONFIG_H",
              f'#define NEXUS_CONFIG_SHA256 "{result["configuration_sha256"]}"',
              f'#define NEXUS_EXACT_PART "{result["part"]}"',
              f'#define NEXUS_CORE_HZ {result["clock"]["core_hz"]}u',
              f'#define NEXUS_CLOCK_INPUT_HZ {result["clock"]["input_hz"]}u',
              f'#define NEXUS_CLOCK_SOURCE_{result["clock"]["source"].upper()} 1',
              f'#define NEXUS_MAIN_STACK_BYTES {result.memory_budget.main_stack_bytes}u',
              f'#define NEXUS_BACKEND_{result["backend"].upper()} 1',
              *cpu_profiles.header_lines(profile), *kernel.header_lines(policy),
              *memory_regions.header_lines(result),
              *metadata.header_lines(thaw(result["platform_metadata"]))]
    header.extend(i2s2.header_lines(result.i2s2))
    probes = any(getattr(getattr(item.options, "irq", None), "probe", False)
                 for item in result.controllers)
    if probes:
        header += ["#if defined(NX_IO_CYCLE_PROBES) && NX_IO_CYCLE_PROBES != 1",
                   '#error "Probe choice must match the selected Platform usage"',
                   "#endif", "#ifndef NX_IO_CYCLE_PROBES",
                   "#define NX_IO_CYCLE_PROBES 1", "#endif"]
    if result["clock"]["source"] == "hse":
        header.insert(7, f'#define NEXUS_HSE_HZ {result["clock"]["input_hz"]}u')
    header.extend(maintained(result.soc_family).configuration_header(result))
    cmake = [f'set(NEXUS_SOC_FAMILY "{result["soc_family"]}")',
             'set(NEXUS_SOC_SOURCE_DIRECTORY "' +
             descriptor.maintained(result["soc_family"]).source_directory + '")',
             f'set(NEXUS_EXACT_PART "{result["part"]}")',
             'set(NEXUS_PROVIDER_CONFIG_MODULES "' + ';'.join(
                 descriptor.maintained(result["soc_family"]).configuration_modules)
             + '")',
             *cpu_profiles.selection_lines(profile),
             *kernel.selection_lines(policy),
             *memory_regions.selection_lines(result),
             f'set(NEXUS_CONFIG_SHA256 "{result["configuration_sha256"]}")',
             f'set(NEXUS_OPTIMIZATION "{result["optimization"]}")',
             'set(NEXUS_SELECTED_KINDS "' + ';'.join(sorted({item['kind'] for item in result['controllers']})) + '")',
             'set(NEXUS_SELECTED_COMPONENTS "' + ';'.join(result['components']) + '")']
    # Canonical source directories also serve multi-family host models. These
    # paths carry no translation-unit graph or additional support decisions.
    cmake += [f'set(NEXUS_SOC_SOURCE_{item.family} "${{NEXUS_SOURCE_DIR}}/' +
              item.source_directory + '")' for item in descriptor.families()]
    cmake += ['set(NEXUS_SOC_SOURCE_VARIABLES "' + ';'.join(
        'NEXUS_SOC_SOURCE_' + item.family for item in descriptor.families()) + '")']
    cmake.extend(i2s2.selection_lines(result.i2s2))
    cmake.append('set(NEXUS_RUNTIME_POWER "' + ("ON" if result.runtime_power is not None else "OFF") + '")')
    cmake.append("set(NEXUS_IO_CYCLE_PROBES " + ("ON" if probes else "OFF") + ")")
    for selection in result.controllers:
        prefix = "NEXUS_" + selection.id.replace("-", "_").upper()
        header.append(f"#define {prefix}_SELECTED 1")
        if selection.kind == "gpio":
            mask = selection.options.mask
            header.append(f"#define {prefix}_MASK {mask}u")
        irq = getattr(selection.options, "irq", None)
        for field in ("rx_capacity", "baud", "irq_priority", "event_capacity"):
            value = (irq.priority if irq is not None else None) if (
                field == "irq_priority") else getattr(selection.options, field, None)
            if value is not None:
                header.append(f"#define {prefix}_{field.upper()} {value}u")
    header.extend(["#endif", ""])
    write("nexus_config.h", "\n".join(header))
    write("selection.cmake", "\n".join(cmake) + "\n")
    try:
        from .bindings import emit
    except ImportError:
        from bindings import emit
    try:
        from .factory import emit as emit_factory, emit_strong
    except ImportError:
        from factory import emit as emit_factory, emit_strong
    factory_header, factory_source = emit_factory(result)
    write("nexus_factory.h", factory_header)
    write("nexus_factory_strong.h", emit_strong(result))
    bindings_header, bindings_source = emit(result)
    bindings_source += "\n" + factory_source
    write("nexus_bindings.h", bindings_header)
    write("bindings.c", bindings_source)
    write("memory.ld", memory_regions.linker(result))
    write("resource-budget.json", json.dumps(memory_regions.resource_budget(result),
                                            indent=2, sort_keys=True) + "\n")
    write(".nexus-config-bundle", result["configuration_sha256"] + "\n")
