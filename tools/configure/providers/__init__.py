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
    return import_module(_DESCRIPTORS[family].configuration_directory.replace("/", "."))
