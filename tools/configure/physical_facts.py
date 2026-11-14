"""Exact package pad and per-engine memory facts, without route inference."""
from collections.abc import Mapping

from tools.configure.providers.common import fail


def memory_access(region, engines):
    """A family with distinct DMA engines must state every engine explicitly."""
    if engines:
        access = region.get("dma_engines")
        if ("dma" in region or not isinstance(access, Mapping) or
                set(access) != set(engines) or
                any(type(value) is not bool for value in access.values())):
            fail("Memory requires exact boolean access for every DMA engine")
    elif "dma_engines" in region or type(region.get("dma")) is not bool:
        fail("Memory DMA facts require a boolean shared reachability contract")


def package_pins(pins, conditions, allowed, *, pad_facts=None):
    """Conditional package pads never silently authorize ordinary GPIO routes."""
    if (not isinstance(pins, list) or not pins or
            any(not isinstance(pin, str) for pin in pins) or
            len(set(pins)) != len(pins)):
        fail("Complete unique GPIO package pins required")
    from tools.configure.resource_graph import identifier
    for pin in pins:
        identifier(pin, "package pin identity")
    if not isinstance(pad_facts, Mapping) or not isinstance(allowed, Mapping):
        fail("Package pins require explicit GPIO and conditional coordinates")
    conditional = set(pins).intersection(allowed)
    if (any(pin not in pad_facts and pin not in conditional for pin in pins)
            or not isinstance(conditions, Mapping) or set(conditions) != conditional):
        fail("Complete package pins require exact conditional pad facts")
    fields = {"gpio", "condition", "source", "status"}
    for pad, fact in conditions.items():
        if (not isinstance(fact, Mapping) or set(fact) != fields or
                any(not isinstance(value, str) or not value.strip()
                    for value in fact.values()) or
                fact["gpio"] != allowed[pad] or fact["gpio"] not in pad_facts or
                fact["status"] != "not_exposed"):
            fail("Conditional pad is not an exposed ordinary GPIO route")
