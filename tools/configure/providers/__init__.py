"""Selected immutable data providers and the explicit Native host fixture."""
from importlib import import_module
from pathlib import Path

from tools.configure.providers.common import fail


def maintained(family, *, test_support=False, root=None):
    """Load one explicit package through the shared, sealed data protocol."""
    if family == "native" and test_support:
        return import_module("tests.support.native.configure")
    from tools.configure import packages
    from tools.configure.data_provider import DataProvider
    root = Path(__file__).resolve().parents[3] if root is None else Path(root)
    read = packages.FactReader()
    package = packages.find(root, family, read=read)
    if package is None or package.scope != "production":
        fail(f"Unmaintained SoC integration provider: {family}")
    if "bindings" not in package.facts:
        fail("Production SoC package requires declarative software bindings")
    return DataProvider.from_package(package, read(package.fact("soc")),
                                     read(package.fact("bindings")), read=read)


def for_selection(selection):
    """Consume the resolved provider owner and reject identity disagreement."""
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
