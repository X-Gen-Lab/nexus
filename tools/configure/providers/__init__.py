"""Explicit maintained SoC contracts; no discovery or runtime plugins."""
from importlib import import_module
from types import MappingProxyType

from tools.configure.providers import descriptor
from tools.configure.providers.common import fail

_DESCRIPTORS = MappingProxyType({item.family: item
                                for item in descriptor.families()})


def maintained(family):
    """Reject a family until its capability and emission contract is maintained."""
    if family not in _DESCRIPTORS:
        fail(f"Unmaintained SoC provider: {family}")
    # Only the selected exact family participates in the configuration import
    # graph. Python caches the explicit module; no filesystem discovery occurs.
    directory = _DESCRIPTORS[family].configuration_directory
    module = directory.replace("/", ".")
    try:
        return import_module(module)
    except ModuleNotFoundError as error:
        entry_packages = {".".join(module.split(".")[:index])
                          for index in range(1, len(module.split(".")) + 1)}
        if error.name not in entry_packages:
            raise
        fail(f"Selected provider {family} entry is missing: {module} "
             f"({directory}/__init__.py)", code="provider_module_missing",
             family=family, module=module,
             source=directory + "/__init__.py", missing_module=error.name)
