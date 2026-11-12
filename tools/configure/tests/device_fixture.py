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
    if soc["id"] == "stm32f4" and part in soc["variants"]:
        path = gate.ROOT / "soc/stm32/stm32f4/devices.json"
        devices = gate.vendor_device_catalog(gate.load(path))
        selected = devices.get(soc["variants"][part].get("device"))
    return gate.validate_soc(soc, part, provider, catalog_device=selected)
