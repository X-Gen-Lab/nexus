"""Reviewed Board electrical constraints, distinct from authored selections."""
from collections.abc import Mapping
from dataclasses import dataclass

from tools.configure.ir import Record
from tools.configure.providers.common import fail, integer


@dataclass(frozen=True)
class PinElectricalConstraintIR(Record):
    """A required Board fact; no default or inferred hardware state."""
    mode: str
    pull: str | None = None
    output_type: str | None = None
    speed: str | None = None
    initial: int | None = None

    def projection(self):
        return {name: value for name, value in super().projection().items()
                if value is not None}


def validate_binding(binding, route, mask):
    """Admit only meaningful pin facts and exact existing initial levels."""
    if "electrical" not in binding:
        return None
    definition = binding["electrical"]
    kind = route["kind"]
    if kind not in {"gpio", "exti"} or not isinstance(definition, Mapping):
        fail("Board electrical constraints require a GPIO or EXTI object")
    mode = definition.get("mode")
    modes = {"input", "output"} if kind == "gpio" else {"edge-event"}
    if type(mode) is not str or mode not in modes or mode not in route["modes"]:
        fail("Board electrical mode must match its reviewed route")
    allowed = {"mode", "pull"}
    if mode == "output":
        allowed.update(("output_type", "initial", "speed"))
    if definition.keys() - allowed:
        fail("Board electrical constraint contains inapplicable fields")
    for name, choices in (("pull", {"none", "up", "down"}),
                          ("output_type", {"push-pull", "open-drain"}),
                          ("speed", {"low", "medium", "high", "very-high"})):
        if name in definition and (
                type(definition[name]) is not str or
                definition[name] not in choices):
            fail(f"Board electrical {name} is unsupported")
    if "initial" in definition:
        initial = integer(definition["initial"], 0, 65535,
                          "Board electrical initial")
        if initial & ~mask or initial != binding.get("initial", 0):
            fail("Board electrical initial differs from its exact binding")
    return PinElectricalConstraintIR(**definition)


def require_selection(selection, binding):
    """Reject a contradiction; do not repair the authored or normalized mode."""
    required = binding.get("electrical")
    if required is None:
        return
    if not isinstance(required, PinElectricalConstraintIR):
        fail("Board electrical constraints were not validated")
    actual = {"mode": selection["mode"],
              "pull": selection.get("pull", "none"),
              "output_type": selection.get("output_type", "push-pull"),
              "initial": selection.get("initial", 0),
              "speed": selection.get("speed", "preserve")}
    for name, expected in required.projection().items():
        if actual[name] != expected:
            fail(f"Board electrical {binding['id']}.{name} requires "
                 f"{expected!r}; selected {actual[name]!r}")


def validate_speed(selection, route):
    """Output slew is explicit; the backup-domain pads only support LOW."""
    if "speed" not in selection:
        return
    speed = selection["speed"]
    if selection["mode"] != "output":
        fail("GPIO speed requires output mode")
    if type(speed) is not str or speed not in {
            "low", "medium", "high", "very-high"}:
        fail("Unsupported GPIO speed")
    if speed != "low" and any(
            pin["pin"] in {"PC13", "PC14", "PC15"} for pin in route["pins"]):
        fail("PC13/14/15 output speed requires low")
