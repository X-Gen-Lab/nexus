"""Explicit normalized SoC facts for pure validator tests, without vendor I/O."""
from tools.configure import configure as gate


def load_soc_facts(path, *, part=None):
    """Load catalog-derived IRQ facts before testing the normalized validator."""
    facts = gate.load(path)
    if facts["id"] == "stm32f4":
        catalog = gate.load(path.parent / "devices.json")
        facts = gate.catalog_soc_facts(
            facts, part or next(iter(facts["variants"])), catalog).soc
    return facts


def validate_soc_facts(soc, part, provider=None):
    """Pass the selected existing catalog record to the pure SoC validator.

    This fixture opens declaration data, not vendor sources. SDK authentication
    belongs to the independent locked device gate and C compile assertions.
    """
    selected = None
    if soc["id"] == "stm32f4" and (provider is None or
            "irq" not in getattr(provider, "soc", {"irq": None})):
        from tools.configure.tests.f4_fixture import provider as exact_provider
        baseline = gate.load(gate.ROOT / "soc/stm32/stm32f4/soc.json")
        reviewed = part if part in baseline["variants"] else next(
            iter(baseline["variants"]))
        # The expected CPU/IRQ contract comes from unmodified package bytes,
        # never from the deliberately corrupted facts under test.
        provider = exact_provider(reviewed)
    if soc["id"] == "stm32f4" and part in soc["variants"]:
        path = gate.ROOT / "soc/stm32/stm32f4/devices.json"
        devices = gate.vendor_device_catalog(gate.load(path))
        selected = devices.get(soc["variants"][part].get("device"))
    return gate.validate_soc(soc, part, provider, catalog_device=selected)
