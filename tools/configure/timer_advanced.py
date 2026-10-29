"""Exact optional timer controllers and owned cold pin/vector/clock leases.

Idle levels are explicit assembly policy, not a claim about an output stage or
physical break circuit. Capture ring storage remains supplied by the caller.
"""
from dataclasses import dataclass

from tools.configure.providers.common import fail, integer


@dataclass(frozen=True)
class TimerBinding:
    """One maintained timer personality and its physical ownership facts."""

    kind: str
    controller: str
    vector: str
    irq: int
    clock_hz: int
    pins: tuple
    resources: tuple


@dataclass(frozen=True)
class TimerPolicy:
    """Explicit family timer identities, pins, resources and clock facts."""

    bindings: tuple

    def binding(self, kind):
        return next((item for item in self.bindings if item.kind == kind), None)


KINDS = {"timer_capture", "advanced_pwm"}
CAPTURE_FIELDS = {"tick_hz", "edge", "filter", "exclusive", "irq_priority", "calls_os"}
ADVANCED_FIELDS = {"tick_hz", "period_ticks", "deadtime_ticks", "break_active_high",
                   "idle_high", "idle_complement_high", "exclusive", "irq_priority", "calls_os"}


def validate_soc(soc, provider):
    for name, item in soc["controllers"].items():
        kind = item["kind"]
        if kind not in KINDS:
            continue
        policy = getattr(provider, "TIMER_ADVANCED", None)
        facts = policy.binding(kind) if policy is not None else None
        mode = "edge-capture" if kind == "timer_capture" else "complementary-pwm"
        if (facts is None or name != facts.controller or item["modes"] != [mode] or
                item["irq"] != [facts.vector] or item.get("irq_numbers") != [facts.irq] or
                type(item.get("clock_hz")) is not int or item["clock_hz"] != facts.clock_hz or
                item["pins"] != [pin for pin, _, _ in facts.pins] or
                item.get("resources") != list(facts.resources)):
            fail("Timer controller, shared vector, clock or pin facts differ from the SDK")


def validate_selection(selection, route, provider):
    kind = route["kind"]
    fields = CAPTURE_FIELDS if kind == "timer_capture" else ADVANCED_FIELDS
    if not (fields - {"calls_os"}).issubset(selection):
        fail("Timer requires explicit clock, mode, safe policy, exclusive owner and IRQ")
    policy = getattr(provider, "TIMER_ADVANCED", None)
    facts = policy.binding(kind) if policy is not None else None
    actual = tuple((pin["pin"], pin["function"], pin["af"]) for pin in route["pins"])
    if facts is None or route["controller"] != facts.controller or actual != facts.pins:
        fail("Timer selection differs from the reviewed exclusive pad route")
    tick = integer(selection["tick_hz"], 1, context="Timer tick_hz")
    if facts.clock_hz % tick or not 1 <= facts.clock_hz // tick <= 65536:
        fail("Timer tick_hz must exactly divide the selected clock within 16 bits")
    if type(selection["exclusive"]) is not bool or not selection["exclusive"]:
        fail("Timer requires an explicit exclusive controller owner")
    if kind == "timer_capture":
        if (type(selection["edge"]) is not str or
                selection["edge"] not in {"rising", "falling", "both"}):
            fail("Timer capture edge is not maintained")
        integer(selection["filter"], 0, 15, "Timer capture filter")
    else:
        integer(selection["period_ticks"], 1, 65535, "Timer period_ticks")
        deadtime = integer(selection["deadtime_ticks"], 0, 1008, "Timer deadtime")
        if not (deadtime <= 127 or
                128 <= deadtime <= 254 and deadtime % 2 == 0 or
                256 <= deadtime <= 504 and deadtime % 8 == 0 or
                512 <= deadtime <= 1008 and deadtime % 16 == 0):
            fail("Timer deadtime has no exact hardware encoding; rounding is forbidden")
        for key in ("break_active_high", "idle_high", "idle_complement_high"):
            if type(selection[key]) is not bool:
                fail("Timer safe polarity requires explicit boolean policy")
