"""One explicit clock input identity, separate from Board oscillator facts."""
from collections.abc import Mapping

from tools.configure.providers.common import fail, integer


def clock_parameters_schema(record):
    """Describe public PLL input grammar, without selecting silicon limits.

    Exact ranges, divisors, source names and output availability belong to the
    selected clock model and package facts. Editor acceptance grants no support.
    """
    required = ("source", "input_hz", "pll_m", "pll_n", "pll_p", "pll_q",
                "ahb_div", "apb1_div", "apb2_div", "adc_div", "supply_mv", "css")
    fields = {name: {"type": "integer", "minimum": 1} for name in required}
    fields.update(source={"type": "string", "minLength": 1},
                  css={"type": "boolean"},
                  sysclk_source={"enum": ["pll", "input"], "default": "pll"},
                  pll48={"type": "boolean", "default": False})
    auxiliary = record({"m", "n", "sai1", "ltdc"}, {"m", "n"})
    auxiliary["properties"].update(m={"type": "integer", "minimum": 1},
                                   n={"type": "integer", "minimum": 1})
    for output, divider in (("sai1", "q"), ("ltdc", "r")):
        shape = record({divider, "post_divider"}, {divider, "post_divider"})
        shape["properties"].update({name: {"type": "integer", "minimum": 1}
                                    for name in (divider, "post_divider")})
        auxiliary["properties"][output] = shape
    auxiliary["anyOf"] = [{"required": ["sai1"]}, {"required": ["ltdc"]}]
    fields["pll_sai"] = auxiliary
    return {"type": "object", "additionalProperties": False,
            "required": list(required), "properties": fields}


def normalize(profile):
    """Normalize legacy HSE input once; reject two competing authorities."""
    if not isinstance(profile, Mapping):
        fail("Clock profile must be an object")
    required = {"core_hz"}
    numeric = {"apb1_hz", "apb2_hz", "timebase", "flash_wait_states"}
    legacy = "hse_hz" in profile
    inputs = {"hse_hz"} if legacy else {"source", "input_hz"}
    if ((required | inputs) - set(profile) or
            set(profile) - required - inputs - numeric):
        fail("Clock profile requires one exact source/input identity")
    source = "hse" if legacy else profile["source"]
    if not isinstance(source, str) or source not in {"hse", "hsi", "msis", "model"}:
        fail("Clock input source is not maintained")
    frequency = profile["hse_hz"] if legacy else profile["input_hz"]
    normalized = {"source": source,
                  "input_hz": integer(frequency, 1, context="Clock input Hz")}
    for key in required | numeric:
        if key in profile:
            normalized[key] = integer(profile[key],
                                      0 if key == "flash_wait_states" else 1,
                                      context="Clock " + key)
    return normalized


def plan(profile_name, profile):
    """Ordinary fixed clocks have no separately reviewed domain plan."""
    normalize(profile)
    return None


def validate_board(clocks):
    """Internal oscillators are SoC facts; external oscillator data is optional."""
    if not isinstance(clocks, Mapping) or set(clocks) - {
            "hse_hz", "lse_hz", "lse_bypass"}:
        fail("Board clocks require reviewed external oscillator facts")
    for name, frequency in clocks.items():
        if name == "lse_bypass":
            if type(frequency) is not bool or "lse_hz" not in clocks:
                fail("Board LSE bypass requires explicit boolean and frequency")
            continue
        integer(frequency, 1, context="Board " + name)


def match_board(profile, clocks):
    """An external input must have the exact reviewed Board frequency."""
    validate_board(clocks)
    if (profile["source"] == "hse" and
            profile["input_hz"] != clocks.get("hse_hz")):
        fail("Clock profile does not match the Board oscillator")


def controller_frequency(clock, controller, provider):
    """Read the declared bus domain instead of inferring it from a chip name."""
    hardware = provider.soc["controllers"].get(controller)
    domain = hardware.get("clock_domain") if hardware is not None else None
    frequency = clock.get(domain + "_hz") if isinstance(domain, str) else None
    if frequency is None:
        fail("Controller has no selected clock domain: " + controller)
    return integer(frequency, 1, context="Controller clock Hz")
