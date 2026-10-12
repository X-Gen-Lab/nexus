"""Explicit immutable family integration contracts, never source build graphs.

These descriptors select reviewed configuration modules and SDK identities.
CMake remains the sole owner of production translation units and dependencies.
No descriptor performs file discovery, downloads or runtime registration.
"""
from dataclasses import dataclass
import importlib.util
from pathlib import Path
from types import MappingProxyType

from .common import fail

try:
    from ...maintenance.source_catalogue import SOURCE_DEPENDENCIES
except ImportError:
    # The CLI also runs as a standalone script in a relocated Source SDK.
    # Load one exact maintained catalogue, never search an ambient sys.path.
    path = Path(__file__).resolve().parents[2] / "maintenance/source_catalogue.py"
    spec = importlib.util.spec_from_file_location("nexus_source_catalogue", path)
    if spec is None or spec.loader is None:
        fail("Missing maintained source dependency catalogue")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    SOURCE_DEPENDENCIES = module.SOURCE_DEPENDENCIES


@dataclass(frozen=True)
class FamilyDescriptor:
    """Inputs required by one maintained family, without CPU-family inference."""

    family: str
    git_modules: tuple = ()
    import_lock: str | None = None
    references: str | None = None

    @property
    def git_dependencies(self):
        sources = {item.identifier: item for item in SOURCE_DEPENDENCIES}
        if any(identifier not in sources for _, identifier in self.git_modules):
            fail("Unregistered family source dependency")
        return tuple((key, sources[identifier].path)
                     for key, identifier in self.git_modules)

    @property
    def configuration_modules(self):
        directory = f"tools/configure/providers/{self.family}"
        return tuple(f"{directory}/{name}" for name in (
            "__init__.py", "bindings.py", "emission.py"))

    @property
    def fact_files(self):
        directory = f"soc/{self.family}"
        names = ("soc.json", "routes.json")
        if self.references is not None:
            names += (self.references,)
        return tuple(f"{directory}/{name}" for name in names)


_DESCRIPTORS = MappingProxyType({
    "native": FamilyDescriptor("native"),
    "stm32f407": FamilyDescriptor("stm32f407", (
        ("cmsis_core", "cmsis5"),
        ("cmsis_device_f4", "stm32f4_cmsis")),
        references="references.json"),
    "gd32f470": FamilyDescriptor("gd32f470",
        import_lock="vendors/gigadevice/gd32f4xx/source.lock.json",
        references="references.json"),
    "stm32g474": FamilyDescriptor("stm32g474", (
        ("cmsis_core", "cmsis5"),
        ("cmsis_device_g4", "stm32g4_cmsis")), references="references.json"),
})


def maintained(family):
    """Reject unknown families instead of inheriting another MCU's contract."""
    result = _DESCRIPTORS.get(family)
    if result is None:
        fail(f"Unmaintained SoC integration descriptor: {family}")
    return result


def families():
    """Return only explicitly maintained descriptors, in stable order."""
    return tuple(_DESCRIPTORS.values())
