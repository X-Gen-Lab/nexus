"""Explicit maintained SoC contracts; no discovery or runtime plugins."""
from importlib import import_module

from tools.configure.providers import descriptor
from tools.configure.providers.common import fail

def maintained(family, *, test_support=False):
    """Reject a family until its capability and emission contract is maintained."""
    selected = descriptor.maintained(family, test_support=test_support)
    # Only the selected exact family participates in the configuration import
    # graph. Python caches the explicit module; no filesystem discovery occurs.
    directory = selected.configuration_directory
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


def for_selection(selection):
    """Consume the explicit resolved owner; synthetic internal fixtures are local."""
    context = getattr(selection, "provider_context", None)
    if context is not None:
        family = getattr(selection, "soc_family", None)
        if family is None:
            family = selection.provider
        if context.descriptor.family != family:
            fail("Selected provider context differs from its resolved family")
        return context.provider
    family = getattr(selection, "soc_family", None)
    return maintained(selection.provider if family is None else family)
