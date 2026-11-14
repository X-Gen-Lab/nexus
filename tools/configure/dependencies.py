"""Explicit configuration-code inputs, distinct from the CMake source graph."""
import hashlib
import json
from pathlib import Path
import tomllib


ROOT = Path(__file__).resolve().parents[2]
COMMON = (
    "__init__", "bundle", "dependencies", "contracts", "sdk", "cpu", "kernel", "metadata", "factory", "ir", "records",
    "threadx", "lowpower", "providers/__init__", "providers/common",
    "providers/descriptor", "calls", "packages", "package_policy", "static_ir",
    "data_provider", "static_recipe", "static_endpoints", "static_fragments", "shared_irq", "domain_policies", "domain_bridge", "header_projection",
    "resource_graph", "trigger_graph", "dma_topology", "io_traits",
    "selected_contracts", "integration_package",
    "enhanced_endpoints", "uart_formats", "stream_profiles", "usb_profiles",
    "usb_host", "usb_otg",
)
PLATFORM = (
    "configure", "selection", "onboarding", "authored", "bindings", "emission", "experience", "can", "memory",
    "usb_device", "ethernet", "timer_advanced", "secondary_controllers", "coupled_io",
    "fact_references", "reference_profiles", "device_view", "clocks", "physical_facts", "gpio_phase",
    "i2c_timing", "i2c_timing_selection", "pin_states", "runtime_power",
    "adc", "dac", "exti", "board_hardware", "gpio_electrical", "rtc",
    "providers/hardware", "hardware_bindings", "platform_bindings", "startup_memory",
    "i2c_target", "spi_frames", "spi_connections", "uart_wires", "media_schema",
    "domains/__init__", "domains/pll_mnpq",
    "policies/adc_group", "policies/capture_audio", "policies/external_bus",
    "policies/gpio_phase", "policies/graphics", "policies/parallel_memory",
    "policies/sdram", "policies/serial_nor", "policies/selection",
    "policies/timer_waveform", "policies/sai", "policies/packet",
)


SDK = (
    "cmake/package/package_source_sdk.py",
    "tools/evidence/common.py",
    "tools/maintenance/cmsis_device_lock.py",
    "tools/maintenance/freertos_transport.py",
    "tools/maintenance/stm32f4_hal_lock.py",
    "tools/maintenance/st_usb_lock.py",
    "tools/maintenance/threadx_lock.py",
    "scripts/ci/vendor_import_identity.py", "scripts/ci/verify_vendor_source.py",
)


def source_names(mode, family, *, sdk=False, external=False,
                 test_support=False):
    """Return declared generator inputs for freshness and SDK admission."""
    if mode not in {"platform", "runtime"}:
        raise ValueError("Unknown configuration-code dependency scope")
    names = ["tools/configure/" + name + ".py" for name in COMMON]
    names.extend(("tools/__init__.py", "tools/maintenance/source_catalogue.py"))
    if sdk:
        names.extend(SDK)
    if mode == "runtime":
        names.append("tools/configure/runtime.py")
    else:
        from tools.configure.providers import descriptor
        names.extend("tools/configure/" + name + ".py" for name in PLATFORM)
        native = family == "native" and test_support
        providers = (() if external or not native else
                     (descriptor.maintained(family, test_support=True),))
        for provider in providers:
            names.extend(provider.configuration_modules)
            if provider.import_lock is not None:
                names.append("scripts/ci/verify_vendor_source.py")
    return sorted(set(names))


def _identity(mode, records):
    digest = hashlib.sha256(json.dumps(
        records, sort_keys=True, separators=(",", ":")).encode()).hexdigest()
    return {"schema_version": 1, "scope": mode,
            "identity_kind": "configuration-generator-source",
            "sha256": digest, "files": records}


def snapshot(*, mode, family=None, sdk=False, test_support=False):
    """Bind common code and only the explicitly selected legacy adapter."""
    paths = {name: str(ROOT / name) for name in source_names(
        mode, family, sdk=sdk, test_support=test_support)}
    records = [{"path": name,
                "sha256": hashlib.sha256(Path(path).read_bytes()).hexdigest()}
               for name, path in paths.items()]
    return paths, _identity(mode, records)


def family_hint(assembly, root, *, test_support=False):
    """Protect legacy code before resolution; never choose hardware or a mode.

    Pure-data packages require only the common engine closure. Resolution still
    validates the exact selectors and their identities independently.
    """
    try:
        path = Path(assembly).resolve()
        value = tomllib.loads(path.read_text(encoding="utf-8"))
        if "soc_package" in value:
            return None
        if "board_package" in value:
            board = path.parent / value["board_package"] / "board.json"
        elif value.get("board") == "native_reference" and test_support:
            board = Path(root) / "tests/support/native/board/board.json"
        else:
            board = Path(root) / "boards" / value["board"] / "board.json"
        family = json.loads(board.read_bytes())["soc_family"]
        from tools.configure import packages
        package = packages.find(root, family)
        if (package is not None and "bindings" in package.facts and
                "software" in json.loads(package.fact("bindings").read_bytes())):
            return None
    except (OSError, ValueError, KeyError, TypeError):
        return None
    return "native" if family == "native" and test_support else None


def selected(paths, identity, *, family, context=None):
    """Retain the pre-resolution bytes of the selected family, never rehash."""
    scope = (None if context is not None and context.package is not None and
             not context.descriptor.configuration_directory else family)
    names = source_names("platform", scope,
                         sdk="cmake/package/package_source_sdk.py" in paths,
                         external=context is not None and context.external,
                         test_support=(family == "native" and
                                       context is not None and
                                       not context.external))
    records = {record["path"]: record for record in identity["files"]}
    if not set(names).issubset(records):
        raise ValueError("Configuration code dependency closure changed during resolution")
    selected_paths = {name: paths[name] for name in names}
    selected_records = [records[name] for name in names]
    if context is not None and context.external:
        for path, digest in context.input_digests:
            if path.suffix == ".py":
                name = "soc-package/" + path.relative_to(context.directory).as_posix()
                selected_paths[name] = str(path)
                selected_records.append({"path": name,
                    "sha256": digest})
    return selected_paths, _identity("platform", selected_records)


def emit(output, paths, identity):
    for name, value in (("configuration_code_paths.json", paths),
                        ("generator-identity.json", identity)):
        (output / name).write_text(
            json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def verify(paths, identity):
    """Reject an edit during generation; an identity is never a trust bypass."""
    for record in identity["files"]:
        current = hashlib.sha256(Path(paths[record["path"]]).read_bytes()).hexdigest()
        if current != record["sha256"]:
            raise ValueError("Configuration code changed during generation: " +
                             record["path"])
