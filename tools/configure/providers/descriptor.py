"""Explicit immutable family integration contracts, never source build graphs.

These descriptors select reviewed configuration modules and SDK identities.
CMake remains the sole owner of production translation units and dependencies.
No descriptor performs file discovery, downloads or runtime registration.
"""
from dataclasses import dataclass
from importlib import import_module
from pathlib import PurePosixPath
from types import MappingProxyType

from tools.configure.providers.common import fail

from tools.maintenance.source_catalogue import SOURCE_DEPENDENCIES


@dataclass(frozen=True)
class FamilyDescriptor:
    """Inputs required by one maintained family, without CPU-family inference."""

    family: str
    vendor: str
    source_directory: str
    configuration_directory: str
    git_modules: tuple = ()
    import_lock: str | None = None
    references: str | None = None
    owned_payload: tuple = ()
    configuration_helpers: tuple = ()

    @property
    def git_dependencies(self):
        sources = {item.identifier: item for item in SOURCE_DEPENDENCIES}
        if any(identifier not in sources for _, identifier in self.git_modules):
            fail("Unregistered family source dependency")
        return tuple((key, sources[identifier].path)
                     for key, identifier in self.git_modules)

    @property
    def configuration_modules(self):
        directory = self.configuration_directory
        packages = tuple(str(parent / "__init__.py") for parent in
                         reversed(PurePosixPath(directory).parents)
                         if str(parent) != ".")
        modules = tuple(f"{directory}/{name}" for name in (
            "__init__.py", "contract.py", "bindings.py", "emission.py"))
        local = (self.source_directory + "/descriptor.py",)
        return packages + local + modules + self.configuration_helpers

    @property
    def fact_files(self):
        directory = self.source_directory
        names = ("soc.json", "routes.json")
        if self.references is not None:
            names += (self.references,)
        return tuple(f"{directory}/{name}" for name in names)


_DESCRIPTOR_MODULES = MappingProxyType({
    "native": "soc.native.descriptor",
    "stm32f103": "soc.stm32.stm32f103.descriptor",
    "stm32f407": "soc.stm32.stm32f407.descriptor",
    "gd32f470": "soc.gd32.gd32f470.descriptor",
    "gd32f303": "soc.gd32.gd32f303.descriptor",
    "stm32g474": "soc.stm32.stm32g474.descriptor",
    "stm32h743": "soc.stm32.stm32h743.descriptor",
    "stm32u575": "soc.stm32.stm32u575.descriptor",
})


def maintained(family):
    """Load exact local metadata, never infer or discover a family module."""
    selected = _DESCRIPTOR_MODULES.get(family)
    if selected is None:
        fail(f"Unmaintained SoC integration descriptor: {family}")
    module = import_module(selected)
    result = getattr(module, "DESCRIPTOR", None)
    if (module.__name__ != selected or type(result) is not FamilyDescriptor or
            result.family != family):
        fail(f"Invalid local descriptor contract: {family}")
    return result


def families():
    """Preserve the explicit stable family order without another registry."""
    return tuple(maintained(family) for family in _DESCRIPTOR_MODULES)


def source_directory(family):
    """Return the canonical layout only after exact descriptor admission."""
    return maintained(family).source_directory
