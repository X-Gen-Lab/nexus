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
    descriptor_path: str | None = None
    git_modules: tuple = ()
    import_lock: str | None = None
    references: str | None = None
    owned_payload: tuple = ()
    configuration_helpers: tuple = ()
    device_catalog: str | None = None

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
        local = (self.descriptor_path or
                 self.source_directory + "/descriptor.py",)
        return packages + local + modules + self.configuration_helpers

    @property
    def fact_files(self):
        directory = self.source_directory
        names = ("soc.json", "routes.json")
        if self.device_catalog is not None:
            names += (self.device_catalog,)
        if self.references is not None:
            names += (self.references,)
        return tuple(f"{directory}/{name}" for name in names)


_DESCRIPTOR_MODULES = MappingProxyType({
    "stm32f4": "tools.configure.stm32f4_descriptor",
})


def maintained(family, *, test_support=False):
    """Load one production family or an explicitly requested host fixture."""
    selected = _DESCRIPTOR_MODULES.get(family)
    if family == "native" and test_support:
        selected = "tests.support.native.descriptor"
    if selected is None:
        fail(f"Unmaintained SoC integration descriptor: {family}")
    module = import_module(selected)
    result = getattr(module, "DESCRIPTOR", None)
    if (module.__name__ != selected or type(result) is not FamilyDescriptor or
            result.family != family):
        fail(f"Invalid local descriptor contract: {family}")
    return result


def family_names():
    """Project explicit built-in identities without importing local metadata."""
    return tuple(_DESCRIPTOR_MODULES)


def families():
    """Preserve the explicit stable family order without another registry."""
    return tuple(maintained(family) for family in _DESCRIPTOR_MODULES)


def source_directory(family):
    """Return the canonical layout only after exact descriptor admission."""
    return maintained(family).source_directory


def for_selection(selection):
    """Use the resolved descriptor instead of selecting an external name again."""
    context = getattr(selection, "provider_context", None)
    if context is not None:
        if context.descriptor.family != selection.soc_family:
            fail("Selected descriptor context differs from its resolved family")
        return context.descriptor
    return maintained(selection.soc_family)
