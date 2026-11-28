"""Static pin routing encodings declared by the selected package.

The policy validates static connection values only. It does not select routes,
operate registers or grant support for another pinmux encoding.
"""
from collections.abc import Mapping
from dataclasses import dataclass
from typing import ClassVar

from tools.configure.providers.common import fail, integer


@dataclass(frozen=True)
class NumericPinMuxPolicy:
    """An immutable, explicit selector set without a vendor-derived default."""

    selectors: tuple[int, ...]
    pin_fields: ClassVar[frozenset[str]] = frozenset({"af"})

    def __post_init__(self):
        if (not isinstance(self.selectors, (tuple, list)) or
                not self.selectors):
            fail("Pinmux selectors require a nonempty explicit sequence")
        selectors = tuple(integer(value, context="Pinmux selector")
                          for value in self.selectors)
        if len(set(selectors)) != len(selectors):
            fail("Pinmux selectors require distinct numeric values")
        object.__setattr__(self, "selectors", selectors)

    @classmethod
    def from_validated(cls, value):
        """Admit only the maintained numeric encoding and its exact facts."""
        if (not isinstance(value, Mapping) or
                set(value) != {"encoding", "selectors"}):
            fail("Pinmux policy requires exact encoding and selectors fields")
        if value["encoding"] != "numeric":
            fail("Unsupported pinmux encoding")
        return cls(value["selectors"])

    def validate(self, value, context="Pinmux selector"):
        """Return a declared uint32 selector; unknown values remain rejected."""
        value = integer(value, context=context)
        if value not in self.selectors:
            fail(f"{context} is not declared by the selected pinmux policy")
        return value

    def alternate(self, pin, context="Pinmux selector"):
        return self.validate(pin["af"], context)


@dataclass(frozen=True)
class FixedRoutePinMuxPolicy:
    """A physical route whose pads have no independent AF selectors."""

    pin_fields: ClassVar[frozenset[str]] = frozenset()

    def alternate(self, pin, context="Pinmux selector"):
        if "af" in pin:
            fail(f"{context} cannot declare a numeric AF on a fixed route")
        return None


PinMuxPolicy = NumericPinMuxPolicy | FixedRoutePinMuxPolicy


def load_pinmux_policy(facts) -> PinMuxPolicy:
    """Select one reviewed encoding from the declared finite package facts."""
    if isinstance(facts, Mapping) and facts.get("encoding") == "route-fixed":
        if set(facts) != {"encoding"}:
            fail("Fixed-route pinmux policy requires only its encoding")
        return FixedRoutePinMuxPolicy()
    return NumericPinMuxPolicy.from_validated(facts)
