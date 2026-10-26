"""Bind caller-declared clock/electrical envelopes to selected V2 hardware.

These are authored assumptions, not measured Board facts. The solver uses one
explicit envelope; it never substitutes a nominal, zero-error oscillator model.
Only the currently implemented analog-filter-on / DNF-zero hardware mode is
admitted. Its conservative delay model follows the maintained AN4235 review.
"""
from tools.configure import i2c_timing
from tools.configure.providers.common import fail, integer


FIELDS = frozenset(("kernel_hz_min", "kernel_hz_max", "pclk_hz_min",
                    "bus_hz_min", "rise_ns_max", "fall_ns_max", "source"))


def resolve(selection, clock, provider, *, clock_plan=None):
    """Require the selected source nominal inside explicit declared bounds."""
    project = getattr(provider, "i2c_timing_clock", None)
    selected = project(clock, clock_plan) if project is not None else None
    authored = selection.get("timing")
    if authored is None:
        if selected is not None and selected.requires_explicit:
            fail("Selected I2C requires an explicit clock/electrical timing envelope")
        return None
    if selected is None or selection["mode"] != "poll-messages":
        fail("Explicit I2C timing requires a maintained polling V2 provider")
    if not isinstance(authored, dict) or set(authored) != FIELDS:
        fail("I2C timing requires exact clock/electrical/source fields; "
             "analog filter and DNF choices are locked")
    source = authored["source"]
    if not isinstance(source, str) or not source.strip():
        fail("I2C timing source must identify the declared engineering inputs")
    for name in FIELDS - {"source"}:
        integer(authored[name], 1 if "hz" in name else 0,
                1000000000, "I2C clock/electrical " + name)
    nominal = selected.nominal_hz
    if not (authored["kernel_hz_min"] <= nominal <=
            authored["kernel_hz_max"]):
        fail("I2C declared clock envelope excludes the selected source nominal")
    if authored["pclk_hz_min"] > selected.pclk_hz:
        fail("I2C declared PCLK minimum exceeds the selected source nominal")
    if selected.requires_explicit and (
            authored["kernel_hz_min"] > selected.minimum_hz or
            authored["kernel_hz_max"] < selected.maximum_hz or
            authored["pclk_hz_min"] > selected.pclk_minimum_hz):
        fail("I2C declared clock envelope does not cover the reviewed local sources")
    values = {name: value for name, value in authored.items() if name != "source"}
    values.update(bus_hz=selection.get("max_hz", 100000),
                  analog_filter_ns_min=50, analog_filter_ns_max=260,
                  digital_filter_cycles=0)
    calculation = i2c_timing.resolve(values)
    if calculation.cr1_filter_bits != 0:
        fail("I2C timing requires an unimplemented hardware filter mode")
    return {**calculation.to_dict(), "assumptions": values,
            "source": source, "provenance": "authored_declared_assumptions",
            "physical_qualified": False}
