"""Compile/link bundle emission and shared private provider declarations."""
import json
from pathlib import Path
import re


RESOLVED_SCHEMA_VERSION = 2


def resolved_projection(result):
    """Project selected facts; full IR still owns validation and identity."""
    from tools.configure.ir import thaw

    names = {item.binding for item in result.controllers}
    for item in result.devices:
        for field in ("binding", "cs_binding"):
            name = getattr(item.options, field, None)
            if name is not None:
                names.add(name)
    bindings = {name: result.board_bindings[name] for name in sorted(names)}
    routes = ({item.route for item in result.controllers} |
              {item["route"] for item in bindings.values()})
    projection = result.projection()
    projection.update(
        schema_version=RESOLVED_SCHEMA_VERSION,
        board_bindings=bindings,
        routes={name: result.routes[name] for name in sorted(routes)})
    context = result.provider_context
    policy = context.binding_policy if context else None
    if result.cpu_profile.arch.startswith("cortex-m") and policy and policy.irq_facts:
        claimed = {claim.key[4:] for claim in result.claims
                   if claim.key.startswith("irq:")}
        # Core exceptions belong to Arch/OS. The device catalog numbers
        # external interrupts relative to the end of the core vector table.
        projection["cortex_m_irq_vectors"] = {
            binding.handler: binding.number
            for name, binding in policy.irq_facts.items() if name in claimed}
    if result.fact_references is not None:
        path = context.package.fact("references")
        label = path.relative_to(context.directory).as_posix()
        if context.external:
            label = "soc-package/" + context.identity["id"] + "/" + label
        source = next(item for item in result.inputs if item["path"] == label)
        projection["fact_references"] = {"source": source}
    return thaw(projection)


def controller_selection(controllers):
    """Serialize the three validated identifiers used by CMake to select IP."""
    rows = []
    for item in controllers:
        fields = (item.kind, item.controller, item.mode)
        if not all(re.fullmatch(r"[A-Za-z0-9_.-]+", token) for token in fields):
            raise ValueError(f"Invalid controller selection identifiers: {fields!r}")
        rows.append("/".join(fields))
    return ";".join(dict.fromkeys(rows))


def emit_bundle(result, output):
    """Emit compile/link inputs from resolved IR; CMake retains the code graph.

    Provider declarations depend on leaf memory contracts; this orchestration
    layer alone combines the complete compiler, linker and binding projections.
    """
    from tools.configure import cpu as cpu_profiles
    from tools.configure import kernel, memory as memory_regions, metadata
    from tools.configure.ir import thaw
    from tools.configure.providers import descriptor, for_selection
    from tools.configure.ir import ConfigurationIR
    if not isinstance(result, ConfigurationIR):
        raise TypeError("Bundle emission requires ConfigurationIR")
    write = lambda name, data: (output / name).write_text(data, encoding="utf-8")
    write("resolved.json", json.dumps(resolved_projection(result), indent=2,
                                      sort_keys=True) + "\n")
    profile, policy = result.cpu_profile, result.kernel
    header = ["/* Generated from the sole resolved configuration. */",
              "#ifndef NEXUS_CONFIG_H", "#define NEXUS_CONFIG_H",
              f'#define NEXUS_CONFIG_SHA256 "{result.configuration_sha256}"',
              f'#define NEXUS_EXACT_PART "{result.part}"',
              f'#define NEXUS_CORE_HZ {result.clock["core_hz"]}u',
              f'#define NEXUS_CLOCK_INPUT_HZ {result.clock["input_hz"]}u',
              f'#define NEXUS_CLOCK_SOURCE_{result.clock["source"].upper()} 1',
              f'#define NEXUS_MAIN_STACK_BYTES {result.memory_budget.main_stack_bytes}u',
              f'#define NEXUS_BACKEND_{result.backend.upper()} 1',
              *cpu_profiles.header_lines(profile), *kernel.header_lines(policy),
              *memory_regions.header_lines(result),
              *metadata.header_lines(thaw(result.platform_metadata))]
    probes = any(getattr(getattr(item.options, "irq", None), "probe", False)
                 for item in result.controllers)
    if probes:
        header += ["#if defined(NX_IO_CYCLE_PROBES) && NX_IO_CYCLE_PROBES != 1",
                   '#error "Probe choice must match the selected Platform usage"',
                   "#endif", "#ifndef NX_IO_CYCLE_PROBES",
                   "#define NX_IO_CYCLE_PROBES 1", "#endif"]
    if result.clock["source"] == "hse":
        header.insert(7, f'#define NEXUS_HSE_HZ {result.clock["input_hz"]}u')
    provider = for_selection(result)
    header.extend(provider.configuration_header(result))
    context = result.provider_context
    selected = (context.descriptor if context is not None else
                descriptor.maintained(result.soc_family))
    source = (context.source_directory.as_posix() if context is not None and
              context.external else selected.source_directory)
    modules = (tuple(path.as_posix() for path in context.inputs
                     if path.suffix == ".py") if context is not None and
               context.external else selected.configuration_modules)
    cmake = [f'set(NEXUS_SOC_FAMILY "{result.soc_family}")',
             'set(NEXUS_SOC_SOURCE_DIRECTORY "' +
             source + '")',
             f'set(NEXUS_EXACT_PART "{result.part}")',
             'set(NEXUS_PROVIDER_CONFIG_MODULES "' + ';'.join(
                 modules)
             + '")',
             *cpu_profiles.selection_lines(profile),
             *kernel.selection_lines(policy),
             *memory_regions.selection_lines(result),
             f'set(NEXUS_CONFIG_SHA256 "{result.configuration_sha256}")',
             f'set(NEXUS_OPTIMIZATION "{result.optimization}")',
             f'set(NEXUS_SELECTED_CONTROLLERS "{controller_selection(result.controllers)}")',
             'set(NEXUS_SELECTED_KINDS "' + ';'.join(sorted(
                 {item.kind for item in result.controllers} |
                 {item.driver.removesuffix("-endpoint") for item in result.devices
                  if item.kind == "external_bus" and
                  item.driver != "mapped_storage-endpoint"})) + '")',
             'set(NEXUS_REQUIRED_IO_DOMAINS "' +
             ';'.join(result.required_io_domains) + '")',
             'set(NEXUS_SELECTED_COMPONENTS "' + ';'.join(result.components) + '")']
    if context is None or not context.external or provider.MODEL:
        cmake.append("set(NEXUS_DISCOVER_SOC_PACKAGES ON)")
    package = context.package if context is not None else None
    if package is not None:
        cmake += [
            f'set(NEXUS_SOC_PACKAGE_ID "{package.id}")',
            f'set(NEXUS_SOC_PACKAGE_MANIFEST "{package.manifest_path.as_posix()}")',
            f'set(NEXUS_SOC_CMAKE_ENTRY "{package.cmake_entry.as_posix()}")',
            f'set(NEXUS_SOC_TARGET "{package.target}")']
    else:
        entry = (source if Path(source).is_absolute() else
                 "${NEXUS_SOURCE_DIR}/" + source)
        target = ("nexus_io_native" if selected.family == "native" else
                  "nexus_soc_" + selected.family)
        cmake += ['set(NEXUS_SOC_PACKAGE_ID "")',
                  f'set(NEXUS_SOC_CMAKE_ENTRY "{entry}/CMakeLists.txt")',
                  f'set(NEXUS_SOC_TARGET "{target}")']
    device = result.physical_part.device
    if device is not None:
        cmake += [f'set(NEXUS_VENDOR_DEVICE_DEFINE "{device.macro}")',
                  f'set(NEXUS_VENDOR_DEVICE_HEADER "{device.header}")',
                  f'set(NEXUS_VENDOR_DEVICE_STARTUP "{device.startup}")']
    # Canonical source directories also serve selected host models. These
    # paths carry no translation-unit graph or additional support decisions.
    sources = descriptor.families() if provider.MODEL else (selected,)
    if selected.family not in {item.family for item in sources}:
        sources += (selected,)
    cmake += [f'set(NEXUS_SOC_SOURCE_{item.family} "${{NEXUS_SOURCE_DIR}}/' +
              item.source_directory + '")' for item in sources]
    cmake += ['set(NEXUS_SOC_SOURCE_VARIABLES "' + ';'.join(
        'NEXUS_SOC_SOURCE_' + item.family for item in sources) + '")']
    cmake.append('set(NEXUS_RUNTIME_POWER "' + ("ON" if result.runtime_power is not None else "OFF") + '")')
    cmake.append("set(NEXUS_IO_CYCLE_PROBES " + ("ON" if probes else "OFF") + ")")
    features = getattr(provider, "build_features", None)
    if features is not None:
        for name, enabled, macro in features(result):
            cmake.append(f"set(NEXUS_FEATURE_{name.upper()} {'ON' if enabled else 'OFF'})")
            if macro is not None:
                header.append(f"#define {macro} {int(enabled)}")
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
    names = tuple(re.match(r"set\((NEXUS_[A-Za-z0-9_]+)\s", line).group(1)
                  for line in cmake)
    if len(set(names)) != len(names):
        raise ValueError("Duplicate generated build-selection variable")
    cmake.append('set(NEXUS_SELECTION_VARIABLES "' + ';'.join(names) + '")')
    write("selection.cmake", "\n".join(cmake) + "\n")
    from tools.configure.bindings import emit
    from tools.configure.factory import emit as emit_factory, emit_strong, emit_smbus
    factory_header, factory_source = emit_factory(result)
    write("nexus_factory.h", factory_header)
    write("nexus_factory_strong.h", emit_strong(result))
    bindings_header, bindings_source = emit(result)
    bindings_source += "\n" + factory_source
    smbus_header, smbus_source = emit_smbus(result)
    if smbus_header:
        write("nexus_smbus_factory.h", smbus_header)
        bindings_source += "\n" + smbus_source
    write("nexus_bindings.h", bindings_header)
    write("bindings.c", bindings_source)
    write("memory.ld", memory_regions.linker(result))
    write("resource-budget.json", json.dumps(memory_regions.resource_budget(result),
                                            indent=2, sort_keys=True) + "\n")
    write(".nexus-config-bundle", result.configuration_sha256 + "\n")
