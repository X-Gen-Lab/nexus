"""One explicit clock input identity, separate from Board oscillator facts."""
from collections.abc import Mapping

from tools.configure.providers.common import fail, integer


def normalize(profile):
    """Normalize legacy HSE input once; reject two competing authorities."""
    if not isinstance(profile, Mapping):
        fail("Clock profile must be an object")
    if "domains" in profile:
        return _normalize_h743_domains(profile)
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


def _normalize_h743_domains(profile):
    """Check the single reviewed H743 V-cut PLL and local kernel envelope.

    Values belong to the maintained SoC profile. The relationships below bind
    them to the fixed hardware implementation; they are not authored choices or
    measured oscillator, peripheral throughput or board qualification.
    """
    fields = {"source", "input_hz", "input_min_hz", "input_max_hz",
              "domains", "reviewed_revision_ids"}
    if set(profile) != fields or profile["source"] != "hsi":
        fail("H743 clock domains require one exact internal input identity")
    nominal, lower, upper = (integer(profile[key], 1, context="Clock " + key)
                             for key in ("input_hz", "input_min_hz",
                                         "input_max_hz"))
    if (nominal, lower, upper) != (64000000, 62000000, 65000000):
        fail("H743 clock input differs from the reviewed V-cut envelope")
    revisions = profile["reviewed_revision_ids"]
    if (not isinstance(revisions, (list, tuple)) or len(revisions) != 1 or
            type(revisions[0]) is not int or revisions[0] != 0x2003):
        fail("H743 high clocks admit reviewed revision V only; no Y/W fallback")
    cpu = (nominal * 6, lower * 6 * 98 // 100, upper * 6 * 102 // 100)
    hclk = tuple(value // 2 for value in cpu)
    apb = tuple(value // 2 for value in hclk)
    expected = {"cpu": ("pll1p", cpu), "hclk": ("cpu", hclk),
                **{f"apb{i}": ("hclk", apb) for i in range(1, 5)},
                "tim2": ("hclk", hclk), "tim3": ("hclk", hclk),
                "usart3": ("hsi", (nominal, lower, upper)),
                "i2c1": ("hsi", (nominal, lower, upper)),
                "spi4": ("apb2", apb),
                "adc12_input": ("per_ck", (nominal, lower, upper))}
    domains = profile["domains"]
    if not isinstance(domains, Mapping) or set(domains) != set(expected):
        fail("H743 clock profile requires exact reviewed distinct domains")
    for name, (source, frequencies) in expected.items():
        domain = domains[name]
        if (not isinstance(domain, Mapping) or set(domain) != {
                "source", "nominal_hz", "minimum_hz", "maximum_hz"} or
                domain["source"] != source):
            fail("H743 clock domain source differs from its selected mux: " + name)
        actual = tuple(integer(domain[key], 1, context="Clock " + name + key)
                       for key in ("nominal_hz", "minimum_hz", "maximum_hz"))
        if actual != frequencies:
            fail("H743 clock domain differs from the reviewed envelope: " + name)
    return {"source": profile["source"], "input_hz": nominal,
            "core_hz": domains["cpu"]["nominal_hz"],
            "apb1_hz": domains["apb1"]["nominal_hz"],
            "apb2_hz": domains["apb2"]["nominal_hz"]}


def plan(profile_name, profile):
    """Project reviewed distinct domains without changing fixed-profile JSON."""
    if "domains" not in profile:
        if profile_name == "hsi64-pll384-cache-off":
            fail("H743 high clock profile requires its reviewed distinct domains")
        return None
    if profile_name != "hsi64-pll384-cache-off":
        fail("Clock domain plan is not reviewed for this selected profile")
    _normalize_h743_domains(profile)
    return {"profile": profile_name,
            "input": {"source": profile["source"],
                      "nominal_hz": profile["input_hz"],
                      "minimum_hz": profile["input_min_hz"],
                      "maximum_hz": profile["input_max_hz"]},
            "domains": {name: dict(value)
                        for name, value in profile["domains"].items()},
            "reviewed_revision_ids": list(profile["reviewed_revision_ids"]),
            "provenance": "reviewed_source_assumptions",
            "physical_qualified": False}


def validate_board(clocks):
    """Internal oscillators are SoC facts; external oscillator data is optional."""
    if not isinstance(clocks, Mapping) or set(clocks) - {"hse_hz", "lse_hz"}:
        fail("Board clocks require reviewed external oscillator facts")
    for name, frequency in clocks.items():
        integer(frequency, 1, context="Board " + name)


def match_board(profile, clocks):
    """An external input must have the exact reviewed Board frequency."""
    validate_board(clocks)
    if (profile["source"] == "hse" and
            profile["input_hz"] != clocks.get("hse_hz")):
        fail("Clock profile does not match the Board oscillator")
