"""Explicit normalized SoC facts for pure validator tests, without vendor I/O."""
from tools.configure import configure as gate


def load_soc_facts(path, *, part=None):
    """Load catalog-derived IRQ facts before testing the normalized validator."""
    facts = gate.load(path)
    if facts["id"] == "stm32f4":
        catalog = gate.load(path.parent / "devices.json")
        facts, _ = gate.catalog_soc_facts(
            facts, part or next(iter(facts["variants"])), catalog)
    return facts
