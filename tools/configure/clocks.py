"""One explicit clock input identity, separate from Board oscillator facts."""
from collections.abc import Mapping
from dataclasses import dataclass
from types import MappingProxyType

from tools.configure.providers.common import fail, integer
from tools.configure.validation import identity, obj, unique


_FIXED_DOMAINS = ("core", "apb1", "apb2", "apb1_timer", "apb2_timer", "pll48")


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
    numeric = {name + "_hz" for name in _FIXED_DOMAINS} | {
        "timebase", "flash_wait_states"}
    legacy = "hse_hz" in profile
    inputs = {"hse_hz"} if legacy else {"source", "input_hz"}
    if ((required | inputs) - set(profile) or
            set(profile) - required - inputs - numeric - {"hse_bypass"}):
        fail("Clock profile requires one exact source/input identity")
    source = "hse" if legacy else profile["source"]
    if not isinstance(source, str) or source not in {"hse", "hsi", "msis", "model"}:
        fail("Clock input source is not maintained")
    frequency = profile["hse_hz"] if legacy else profile["input_hz"]
    normalized = {"source": source,
                  "input_hz": integer(frequency, 1, context="Clock input Hz")}
    if "hse_bypass" in profile:
        bypass = profile["hse_bypass"]
        if source != "hse" or type(bypass) is not bool:
            fail("HSE bypass requires the external source and an explicit boolean")
        normalized["hse_bypass"] = bypass
    for key in required | numeric:
        if key in profile:
            normalized[key] = integer(profile[key],
                                      0 if key == "flash_wait_states" else 1,
                                      context="Clock " + key)
    return normalized


@dataclass(frozen=True)
class FixedProfilePolicy:
    """Named paths implemented by System, with explicit nominal frequencies."""

    reviews: Mapping

    @classmethod
    def from_validated(cls, value):
        obj(value, {"model", "reviews"}, context="Fixed clock policy")
        reviews = value["reviews"]
        if not isinstance(reviews, Mapping) or not reviews:
            fail("Fixed clock policy requires named device reviews")
        validated = {}
        for name, profiles in reviews.items():
            identity(name, "Fixed clock policy review")
            if not isinstance(profiles, (list, tuple)) or not profiles:
                fail("Fixed clock review requires at least one named profile")
            names = tuple(identity(profile, "Fixed clock profile") for profile in profiles)
            unique(names, "Fixed clock review")
            validated[name] = names
        return cls(MappingProxyType(validated))

    normalize = staticmethod(normalize)

    def plan(self, profile_name, profile):
        if not any(profile_name in names for names in self.reviews.values()):
            fail("Unreviewed fixed clock profile: " + profile_name)
        selected = self.normalize(profile)

        def domain(source, hz):
            return dict(source=source, nominal_hz=hz, minimum_hz=0, maximum_hz=0)

        domains = {name: domain(name, selected[name + "_hz"]) for name in _FIXED_DOMAINS
                   if name + "_hz" in selected}
        return dict(profile=profile_name,
                    input=domain(selected["source"], selected["input_hz"]), domains=domains,
                    reviewed_revision_ids=(), provenance="Selected fixed System clock facts",
                    physical_qualified=False)

    def validate_clock_policy(self, name, selected=None):
        allowed = self.reviews.get(name)
        if allowed is None:
            fail("Unknown selected device clock policy")
        if selected is not None and selected["profile"] not in allowed:
            fail("Clock profile is outside the selected device clock policy")

    @staticmethod
    def validate_consumers(selected, controllers, *, controller_domains):
        for item in controllers:
            domain = controller_domains.get(item.controller)
            if domain is not None and domain not in selected["domains"]:
                fail("Controller clock domain is absent from the selected plan: " + item.controller)

    @staticmethod
    def controller_hz(result, controller):
        return None

    @staticmethod
    def timer_domain(controller):
        fail("Timer has no declared clock domain: " + controller)


def plan(profile_name, profile):
    """Ordinary fixed clocks have no separately reviewed domain plan."""
    normalize(profile)
    return None


def validate_board(clocks):
    """Internal oscillators are SoC facts; external oscillator data is optional."""
    if not isinstance(clocks, Mapping) or set(clocks) - {
            "hse_hz", "hse_bypass", "lse_hz", "lse_bypass"}:
        fail("Board clocks require reviewed external oscillator facts")
    for name, frequency in clocks.items():
        if name.endswith("_bypass"):
            oscillator = name.removesuffix("_bypass")
            if type(frequency) is not bool or oscillator + "_hz" not in clocks:
                fail("Board " + oscillator.upper() +
                     " bypass requires explicit boolean and frequency")
            continue
        integer(frequency, 1, context="Board " + name)


def match_board(profile, clocks):
    """An external input must have the exact reviewed Board frequency."""
    validate_board(clocks)
    if (profile["source"] == "hse" and
            profile["input_hz"] != clocks.get("hse_hz")):
        fail("Clock profile does not match the Board oscillator")
    if "hse_bypass" in profile and profile["hse_bypass"] != clocks.get("hse_bypass"):
        fail("Clock profile does not match the Board HSE bypass")


def controller_frequency(clock, controller, provider):
    """Read the declared bus domain instead of inferring it from a chip name."""
    hardware = provider.soc["controllers"].get(controller)
    domain = hardware.get("clock_domain") if hardware is not None else None
    frequency = clock.get(domain + "_hz") if isinstance(domain, str) else None
    if frequency is None:
        fail("Controller has no selected clock domain: " + controller)
    return integer(frequency, 1, context="Controller clock Hz")
