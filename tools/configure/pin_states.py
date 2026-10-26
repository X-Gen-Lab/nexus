"""Strict Board pad intent resolves once into immutable register plans.

These plans describe two software states, never a power mode or physical safety.
No electrical values are inferred from a route, PCB name or unselected state.
"""
from tools.configure.providers.common import fail, integer


def _object(value, fields, label):
    if not isinstance(value, dict) or set(value) != set(fields):
        fail("Invalid pin states " + label + ": explicit fields required")


def _choice(value, choices, label):
    if not isinstance(value, str) or value not in choices:
        fail("Invalid pin states " + label)
    return choices[value]


def validate_binding(binding, kind, mask):
    """Normalize both states without retaining a second mutable authority."""
    if "pin_states" not in binding:
        return None
    if kind != "gpio":
        fail("Pin states belong only to GPIO Board bindings")
    states = binding["pin_states"]
    _object(states, ("default", "sleep"), "names")
    normalized = {}
    for name in ("default", "sleep"):
        state = states[name]
        if (not isinstance(state, dict) or
                not isinstance(state.get("mode"), str) or state["mode"] not in {
                "input", "output", "analog"}):
            fail("Invalid pin states mode")
        output = state["mode"] == "output"
        fields = ("mode", "pull", "output_type", "speed", "level") if output else (
            "mode", "pull")
        _object(state, fields, name)
        pull = _choice(state["pull"], {"none": 0, "up": 1, "down": 2}, "pull")
        mode = {"input": 0, "output": 1, "analog": 3}[state["mode"]]
        level, drive, speed = 0, 0, 0
        if output:
            level = integer(state["level"], 0, 65535, "pin states level")
            drive = _choice(state["output_type"],
                            {"push-pull": 0, "open-drain": 1}, "output type")
            speed = _choice(state["speed"],
                            {"low": 0, "medium": 1, "high": 2,
                             "very-high": 3}, "speed")
            if level & ~mask:
                fail("Invalid pin states drive or unauthorized level")
            if name == "default" and (
                    "initial" not in binding or binding["initial"] != level):
                fail("Pin states default output differs from explicit initial")
        pairs = lambda value: sum(value << (2 * bit) for bit in range(16)
                                  if mask & (1 << bit))
        normalized[name] = dict(mode=pairs(mode), output_type=mask if drive else 0,
                                speed=pairs(speed), pull=pairs(pull), levels=level)
    return normalized


def select(binding, *, family, part, controller, backend, clock):
    """The first named-state mode has one exact software admission tuple."""
    if (family, part, controller, backend, clock) != (
            "stm32g474", "STM32G474RET6", "GPIOA", "baremetal", "hse24-pll170"):
        fail("Named states require exact G474RE/GPIOA/hse24-pll170/baremetal")
    if "pin_state_resolution" not in binding:
        fail("Named states require explicit Board pin states")
    return binding["pin_state_resolution"]
