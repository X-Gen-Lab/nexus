"""Exact package pad and per-engine memory facts, without route inference."""
from collections.abc import Mapping
import re

try:
    from .providers.common import fail
except ImportError:
    from providers.common import fail

GPIO_PIN = re.compile(r"P([A-K])([0-9]|1[0-5])")


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


def package_pins(pins, conditions, allowed):
    """Conditional package pads never silently authorize ordinary GPIO routes."""
    if (not isinstance(pins, list) or not pins or
            any(not isinstance(pin, str) for pin in pins) or
            len(set(pins)) != len(pins)):
        fail("Complete unique GPIO package pins required")
    conditional = set(pins).intersection(allowed)
    if (any(not GPIO_PIN.fullmatch(pin) and pin not in conditional for pin in pins)
            or not isinstance(conditions, Mapping) or set(conditions) != conditional):
        fail("Complete package pins require exact conditional pad facts")
    fields = {"gpio", "condition", "source", "status"}
    for pad, fact in conditions.items():
        if (not isinstance(fact, Mapping) or set(fact) != fields or
                any(not isinstance(value, str) or not value.strip()
                    for value in fact.values()) or
                fact["gpio"] != pad.removesuffix("_C") or
                not GPIO_PIN.fullmatch(fact["gpio"]) or
                fact["status"] != "not_exposed"):
            fail("Conditional pad is not an exposed ordinary GPIO route")
