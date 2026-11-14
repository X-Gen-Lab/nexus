"""Finite serial-NOR read profiles; no controller, pad or device default."""
from collections.abc import Mapping
from tools.configure.providers.common import fail, integer

PARAMETERS = frozenset(("part", "prescaler", "cs_high_cycles",
                       "read_budget_us", "prepare_budget_us", "stop_budget_us"))


def validate(parameters, *, hclk_hz, max_hclk_hz, profile, mapped):
    expected = PARAMETERS | ({"read_slots"} if mapped else set())
    if not isinstance(parameters, Mapping) or set(parameters) != expected:
        fail("QSPI requires every explicit exact-part endpoint parameter")
    if mapped:
        integer(parameters["read_slots"], 1, 8, "QSPI map read slots")
    if parameters["part"] != profile["part"]:
        fail("QSPI part must match the explicit reviewed read profile")
    integer(hclk_hz, 1, max_hclk_hz, "QSPI selected HCLK")
    prescaler = integer(parameters["prescaler"], 0, 255, "QSPI prescaler")
    cycles = integer(parameters["cs_high_cycles"], 1, 8, "QSPI NCS high cycles")
    if hclk_hz > profile["max_read_hz"] * (prescaler + 1):
        fail("QSPI READ03 exceeds the exact endpoint's clock maximum")
    if cycles * (prescaler + 1) * 1000000000 < hclk_hz * profile["cs_high_ns"]:
        fail("QSPI NCS high period is shorter than the required endpoint minimum")
    for field in ("read_budget_us", "prepare_budget_us", "stop_budget_us"):
        integer(parameters[field], 1, 1000000, "QSPI " + field)
