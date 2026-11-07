"""Explicit configuration-code inputs, distinct from the CMake source graph."""
import hashlib
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
COMMON = (
    "__init__", "bundle", "dependencies", "contracts", "sdk", "cpu", "kernel", "metadata", "factory", "ir", "i2s2",
    "threadx", "lowpower", "providers/__init__", "providers/common",
    "providers/descriptor", "calls",
    "resource_graph", "trigger_graph", "dma_topology", "io_traits",
    "selected_contracts", "integration_package",
    "enhanced_endpoints", "uart_formats", "stream_profiles", "usb_profiles",
)
PLATFORM = (
    "configure", "selection", "onboarding", "authored", "bindings", "emission", "experience", "can", "memory",
    "usb_device", "ethernet", "timer_advanced", "secondary_controllers", "coupled_io",
    "fact_references", "clocks", "physical_facts", "preflight", "gpio_phase",
    "i2c_timing", "i2c_timing_selection", "pin_states", "runtime_power",
    "adc", "exti", "board_hardware", "gpio_electrical", "rtc",
    "providers/hardware",
)


SDK = (
    "cmake/package/package_source_sdk.py",
    "tools/maintenance/cmsis_device_lock.py",
    "tools/maintenance/freertos_transport.py",
    "tools/maintenance/stm32f1_hal_lock.py",
    "tools/maintenance/stm32f4_hal_lock.py",
    "tools/maintenance/stm32g4_lock.py", "tools/maintenance/threadx_lock.py",
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
        providers = (() if external else
                     descriptor.families() if family is None else
                     (descriptor.maintained(family,
                                            test_support=test_support),))
        if family is None and test_support and not external:
            providers += (descriptor.maintained("native", test_support=True),)
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
    """Bind code before resolution; unknown family binds all maintained code."""
    paths = {name: str(ROOT / name) for name in source_names(
        mode, family, sdk=sdk, test_support=test_support)}
    records = [{"path": name,
                "sha256": hashlib.sha256(Path(path).read_bytes()).hexdigest()}
               for name, path in paths.items()]
    return paths, _identity(mode, records)


def selected(paths, identity, *, family, context=None):
    """Retain the pre-resolution bytes of the selected family, never rehash."""
    names = source_names("platform", family,
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
