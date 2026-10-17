"""Explicit configuration-code inputs, distinct from the CMake source graph."""
import hashlib
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
COMMON = (
    "bundle", "dependencies", "sdk", "cpu", "kernel", "metadata", "factory", "ir",
    "threadx", "lowpower", "providers/__init__", "providers/common",
    "providers/descriptor",
    "resource_graph", "trigger_graph", "dma_topology", "io_traits",
    "enhanced_endpoints", "uart_formats", "stream_profiles",
)
PLATFORM = (
    "configure", "authored", "bindings", "emission", "can", "memory", "contracts",
    "usb_device", "ethernet", "timer_advanced", "secondary_controllers",
    "fact_references", "clocks", "physical_facts", "preflight", "gpio_phase",
    "i2c_timing", "i2c_timing_selection",
    "providers/hardware",
)


SDK = (
    "cmake/package/package_source_sdk.py",
    "tools/maintenance/cmsis_device_lock.py",
    "tools/maintenance/stm32f1_hal_lock.py",
    "tools/maintenance/stm32f4_hal_lock.py",
    "tools/maintenance/stm32g4_lock.py", "tools/maintenance/threadx_lock.py",
    "scripts/ci/vendor_import_identity.py", "scripts/ci/verify_vendor_source.py",
)


def _names(mode, family, *, sdk=False):
    if mode not in {"platform", "runtime"}:
        raise ValueError("Unknown configuration-code dependency scope")
    names = ["tools/configure/" + name + ".py" for name in COMMON]
    names.append("tools/maintenance/source_catalogue.py")
    if sdk:
        names.extend(SDK)
    if mode == "runtime":
        names.append("tools/configure/runtime.py")
    else:
        try:
            from .providers import descriptor
        except ImportError:
            from providers import descriptor
        names.extend("tools/configure/" + name + ".py" for name in PLATFORM)
        providers = (descriptor.families() if family is None else
                     (descriptor.maintained(family),))
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


def snapshot(*, mode, family=None, sdk=False):
    """Bind code before resolution; unknown family binds all maintained code."""
    paths = {name: str(ROOT / name) for name in _names(mode, family, sdk=sdk)}
    records = [{"path": name,
                "sha256": hashlib.sha256(Path(path).read_bytes()).hexdigest()}
               for name, path in paths.items()]
    return paths, _identity(mode, records)


def selected(paths, identity, *, family):
    """Retain the pre-resolution bytes of the selected family, never rehash."""
    names = _names("platform", family,
                   sdk="cmake/package/package_source_sdk.py" in paths)
    records = {record["path"]: record for record in identity["files"]}
    if not set(names).issubset(records):
        raise ValueError("Configuration code dependency closure changed during resolution")
    return ({name: paths[name] for name in names},
            _identity("platform", [records[name] for name in names]))


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
