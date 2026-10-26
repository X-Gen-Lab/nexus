"""Exact H743 V-cut clock policy, independently bound to the C implementation."""
from collections.abc import Mapping

from tools.configure import clocks
from tools.configure.providers.common import fail, integer


def normalize(profile):
    if not isinstance(profile, Mapping):
        fail("Clock profile must be an object")
    if "domains" in profile:
        return _normalize_h743_domains(profile)
    return clocks.normalize(profile)


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




def i2c_timing_clock(clock, clock_plan):
    """I2C1 uses the reviewed joint mux or the fixed APB1 source."""
    from tools.configure.providers.common import I2cTimingClock
    if clock_plan is None:
        return I2cTimingClock(clock["apb1_hz"], clock["apb1_hz"])
    kernel = clock_plan["domains"]["i2c1"]
    return I2cTimingClock(kernel["nominal_hz"], clock["apb1_hz"],
                          kernel["minimum_hz"], kernel["maximum_hz"],
                          clock_plan["domains"]["apb1"]["minimum_hz"], True)
