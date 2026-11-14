"""Explicit raw-calendar initialization policy, independent of civil time."""
from collections.abc import Mapping

from tools.configure.providers.common import fail, integer


POLICY_MODES = frozenset(("adopt-retained", "provision-empty"))
OPTION_FIELDS = frozenset(("allow_set", "timeout_us", "poll_limit", "initial",
                           "lse_bypass", "periodic_wakeup"))
CALENDAR_FIELDS = ("year", "month", "day", "weekday", "hour", "minute", "second")


def validate_board(selection, board):
    """A bypass input never inherits a PCB crystal's source identity."""
    if selection["mode"] not in POLICY_MODES:
        return
    if board["clocks"].get("lse_hz") != 32768:
        fail("RTC requires exact Board LSE32768 source facts")
    if selection.get("lse_bypass", False) != board["clocks"].get(
            "lse_bypass", False):
        fail("RTC bypass source differs from the Board oscillator facts")


def selected_resources(selection, provider):
    """Reserve only the selected package's declared crystal conflicts."""
    if selection["mode"] not in POLICY_MODES or selection.get("lse_bypass", False):
        return ()
    policy = getattr(provider, "SELECTION", None)
    return tuple(policy.data["rtc"].get("crystal_resources", ())) if policy else ()



def validate_selection(selection):
    """Old raw-calendar modes cannot inherit a different backup policy."""
    if selection["mode"] not in POLICY_MODES:
        if OPTION_FIELDS & selection.keys():
            fail("RTC selected mode does not accept initialization options")
        return
    required = OPTION_FIELDS - {"initial", "lse_bypass", "periodic_wakeup"}
    if not required.issubset(selection):
        fail("RTC requires explicit allow_set, timeout_us and poll_limit")
    if type(selection["allow_set"]) is not bool:
        fail("RTC allow_set must be boolean")
    if type(selection.get("lse_bypass", False)) is not bool:
        fail("RTC lse_bypass must be boolean")
    if type(selection.get("periodic_wakeup", False)) is not bool:
        fail("RTC periodic_wakeup must be boolean")
    for field in ("timeout_us", "poll_limit"):
        integer(selection[field], 1, 0xffffffff, "RTC " + field)
    provision = selection["mode"] == "provision-empty"
    if provision != ("initial" in selection):
        fail("RTC initial calendar is required only for provision-empty")
    if not provision:
        return
    value = selection["initial"]
    if not isinstance(value, Mapping) or set(value) != set(CALENDAR_FIELDS):
        fail("RTC initial calendar requires exactly seven raw calendar fields")
    for field, low, high in (("year", 0, 99), ("month", 1, 12),
                              ("day", 1, 31), ("weekday", 1, 7),
                              ("hour", 0, 23), ("minute", 0, 59),
                              ("second", 0, 59)):
        integer(value[field], low, high, "RTC initial calendar " + field)
    days = (31, 29 if value["year"] % 4 == 0 else 28, 31, 30, 31, 30,
            31, 31, 30, 31, 30, 31)
    if value["day"] > days[value["month"] - 1]:
        fail("RTC initial calendar day exceeds the raw YY month")


def editor_schema(schema):
    """Describe finite options without implying a universal RTC provider."""
    properties = schema["properties"]
    properties["allow_set"] = {"type": "boolean"}
    properties["lse_bypass"] = {"type": "boolean", "default": False}
    properties["periodic_wakeup"] = {"type": "boolean", "default": False}
    for field in ("timeout_us", "poll_limit"):
        properties[field] = {"type": "integer", "minimum": 1,
                             "maximum": 0xffffffff}
    limits = (("year", 0, 99), ("month", 1, 12), ("day", 1, 31),
              ("weekday", 1, 7), ("hour", 0, 23), ("minute", 0, 59),
              ("second", 0, 59))
    properties["initial"] = {
        "type": "object", "additionalProperties": False,
        "required": sorted(CALENDAR_FIELDS),
        "properties": {name: {"type": "integer", "minimum": low,
                               "maximum": high} for name, low, high in limits}}
    schema["allOf"] = [
        {"if": {"properties": {"mode": {"enum": sorted(POLICY_MODES)}},
                "required": ["mode"]},
         "then": {"required": ["allow_set", "timeout_us", "poll_limit"]},
         "else": {"not": {"anyOf": [{"required": [name]}
                                      for name in sorted(OPTION_FIELDS)]}}},
        {"if": {"properties": {"mode": {"const": "provision-empty"}},
                "required": ["mode"]},
         "then": {"required": ["initial"]},
         "else": {"not": {"required": ["initial"]}}}]
