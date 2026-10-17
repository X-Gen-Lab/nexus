"""Bind caller-declared clock/electrical envelopes to selected V2 hardware.

These are authored assumptions, not measured Board facts. The solver uses one
explicit envelope; it never substitutes a nominal, zero-error oscillator model.
Only the currently implemented analog-filter-on / DNF-zero hardware mode is
admitted. Its conservative delay model follows the maintained AN4235 review.
"""
try:
    from . import i2c_timing
    from .providers.common import fail, integer
except ImportError:
    import i2c_timing
    from providers.common import fail, integer


FIELDS = frozenset(("kernel_hz_min", "kernel_hz_max", "pclk_hz_min",
                    "bus_hz_min", "rise_ns_max", "fall_ns_max", "source"))


def resolve(selection, clock, provider):
    """Require the selected source nominal inside explicit declared bounds."""
    authored = selection.get("timing")
    if authored is None:
        return None
    family = provider.PREFIX
    if (family not in {"stm32g474", "stm32h743", "stm32u575"} or
            selection["mode"] != "poll-messages"):
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
    nominal = clock["apb1_hz"] if family == "stm32h743" else 16000000
    if not (authored["kernel_hz_min"] <= nominal <=
            authored["kernel_hz_max"]):
        fail("I2C declared clock envelope excludes the selected source nominal")
    if authored["pclk_hz_min"] > clock["apb1_hz"]:
        fail("I2C declared PCLK minimum exceeds the selected source nominal")
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
