"""Explicit normalized SoC facts for pure validator tests, without vendor I/O."""
from pathlib import Path

from tools.configure import configure as gate
from tools.configure import packages
from tools.configure.data_provider import DataProvider


def package_for_fact(path):
    """Find the explicit package owning a test's fact path, if one exists."""
    path = Path(path)
    manifest = path / "package.json" if path.is_dir() else None
    if manifest is None or not manifest.is_file():
        manifest = next((parent / "package.json" for parent in path.parents
                         if (parent / "package.json").is_file()), None)
    return packages.load(manifest) if manifest is not None else None


def load_authored_soc(path):
    """Compose authored fields without creating the selected controller view."""
    package = package_for_fact(path)
    return (packages.read_soc(package, read=gate.load) if package is not None
             else gate.load(path))


def load_binding_facts(path):
    """Read the package's declared fragments, retaining their shared library."""
    from tools.configure.data_provider import binding_document
    package = package_for_fact(path)
    read = packages.FactReader()
    document = binding_document(package, read(package.fact("bindings")), read=read)
    return {"schema_version": 1, "bindings": document["software"]["bindings"],
            "static_fragments": document["software"]["static_fragments"],
            "domains": document["domains"]}


def load_soc_facts(path, *, part=None):
    """Read the authored package slots before testing the normalized validator."""
    package = package_for_fact(path)
    facts = load_authored_soc(path)
    if package is not None and "device_catalog" in package.facts:
        catalog = gate.load(package.fact("device_catalog"))
        facts = gate.catalog_soc_facts(
            facts, part or next(iter(facts["variants"])), catalog).soc
    return facts


def validate_soc_facts(soc, part, provider=None):
    """Pass the selected existing catalog record to the pure SoC validator.

    This fixture opens declaration data, not vendor sources. SDK authentication
    belongs to the independent locked device gate and C compile assertions.
    """
    selected = None
    package = (None if soc["id"] == "native" else
               packages.find(gate.ROOT, soc["id"]))
    if package is not None and "device_catalog" in package.facts:
        read = packages.FactReader()
        catalog = read(package.fact("device_catalog"))
        if provider is None or "irq" not in getattr(provider, "soc", {"irq": None}):
            baseline = packages.read_soc(package, read=read)
            reviewed = part if part in baseline["variants"] else next(
                iter(baseline["variants"]))
            provider = DataProvider.from_package(
                package, baseline, read(package.fact("bindings")), read=read)
            # Keep expected CPU and IRQ facts separate from test mutations.
            provider = gate.resolve_device_facts(
                baseline, reviewed, provider, catalog=catalog).provider
        if part in soc["variants"]:
            devices = gate.vendor_device_catalog(catalog)
            selected = devices.get(soc["variants"][part].get("device"))
    return gate.validate_soc(soc, part, provider, catalog_device=selected)
