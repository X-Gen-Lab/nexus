"""Immutable package metadata projections, never a second source build graph.

Production identities come only from package.json. The explicit Native fixture
keeps its host adapter outside production SoC packages. CMake owns translation
units; this metadata supplies no MCU-specific Python entry or fallback table.
"""
from dataclasses import dataclass
from importlib import import_module
from pathlib import Path, PurePosixPath

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
    package_manifest: str | None = None
    fact_paths: tuple = ()

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
        if not directory:
            return ()
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
        if self.fact_paths:
            return self.fact_paths
        directory = self.source_directory
        names = ("soc.json", "routes.json")
        if self.device_catalog is not None:
            names += (self.device_catalog,)
        if self.references is not None:
            names += (self.references,)
        return tuple(f"{directory}/{name}" for name in names)


ROOT = Path(__file__).resolve().parents[3]


def from_package(package, root):
    """Project one already loaded package without reopening its facts."""
    root = Path(root).resolve()
    return FamilyDescriptor(
        family=package.family, vendor=package.vendor,
        source_directory=package.source_directory.relative_to(root).as_posix(),
        configuration_directory="", git_modules=package.dependencies,
        import_lock=package.import_lock,
        references=package.facts.get("references"),
        device_catalog=package.facts.get("device_catalog"),
        package_manifest=package.manifest_path.relative_to(root).as_posix(),
        fact_paths=tuple(path.relative_to(root).as_posix()
                         for path in package.inputs))


def maintained(family, *, test_support=False, root=ROOT):
    """Query a declared data package or the explicitly selected Native model."""
    if family == "native" and test_support:
        module = import_module("tests.support.native.descriptor")
        result = getattr(module, "DESCRIPTOR", None)
        if type(result) is not FamilyDescriptor or result.family != family:
            fail("Invalid Native fixture metadata")
        return result
    from tools.configure import packages
    package = packages.find(root, family)
    if package is None or package.scope != "production":
        fail(f"Unmaintained SoC integration descriptor: {family}")
    return from_package(package, root)


def family_names(*, root=ROOT):
    """List explicit production manifest identities, with no Python registry."""
    from tools.configure import packages
    return tuple(item.family for item in packages.discover(root)
                 if item.scope == "production")


def families(*, root=ROOT):
    """Project the declared production packages in stable manifest order."""
    from tools.configure import packages
    return tuple(from_package(item, root) for item in packages.discover(root)
                 if item.scope == "production")


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
