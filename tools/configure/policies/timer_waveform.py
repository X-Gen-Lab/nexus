"""Validate declared timer waveform routes without importing an MCU adapter."""
from collections.abc import Mapping
from dataclasses import dataclass
from types import MappingProxyType

from tools.configure.calls import Symbol
from tools.configure.providers.common import fail, integer
from tools.configure.records import freeze


_FIELDS = frozenset(("tick_hz", "period_ticks", "deadtime_ticks", "exclusive",
    "poll_limit", "break_active_high", "idle_high", "idle_complement_high",
    "irq_priority", "calls_os", "master_trigger", "synchronization"))


def _fields(value, names):
    if not isinstance(value, Mapping) or set(value) != set(names):
        fail("Timer waveform facts require exact named fields")


def _sequence(value):
    if not isinstance(value, (tuple, list)):
        fail("Timer waveform facts require an ordered sequence")
    return tuple(value)


def _boolean(value):
    if type(value) is not bool:
        fail("Timer waveform fact requires a boolean")
    return value


@dataclass(frozen=True)
class TimerWaveformPolicy:
    """Finite timer topology, timing limits and selected-owner synchronization."""

    modes: Mapping
    controllers: Mapping
    routes: Mapping
    deadtime_ranges: tuple
    master_modes: tuple
    internal_master_modes: tuple
    sync_modes: tuple
    prescaler_max: int
    poll_limit_max: int
    burst_base: int
    burst_items: int

    @property
    def KINDS(self):
        return frozenset(self.modes)

    @classmethod
    def from_validated(cls, value):
        _fields(value, {"modes", "controllers", "routes", "deadtime_ranges",
            "master_modes", "internal_master_modes", "sync_modes", "prescaler_max", "poll_limit_max",
            "burst_base", "burst_items"})
        modes = value["modes"]
        if (not isinstance(modes, Mapping) or not modes or any(
                not isinstance(key, str) or not isinstance(mode, str) or not mode
                for key, mode in modes.items())):
            fail("Timer waveform modes require declared kind/mode names")
        masters, syncs = (_sequence(value[name]) for name in ("master_modes", "sync_modes"))
        for names in (masters, syncs):
            if (not names or any(not isinstance(name, str) or not name for name in names) or
                    "none" not in names or len(set(names)) != len(names)):
                fail("Timer waveform modes require unique explicit names")
        internal_masters = _sequence(value["internal_master_modes"])
        if (any(not isinstance(name, str) or name not in masters or name == "none"
                for name in internal_masters) or
                len(set(internal_masters)) != len(internal_masters)):
            fail("Timer internal master modes require distinct declared master names")
        if not isinstance(value["controllers"], Mapping) or not value["controllers"]:
            fail("Timer waveform controller catalog is required")
        controllers = {}
        for name, row in value["controllers"].items():
            Symbol(name)
            _fields(row, {"counter_bits", "native_bits", "channel_count", "advanced", "master",
                          "clock_domain", "divided_clock_multiplier", "dma", "itr_sources"})
            for key in ("counter_bits", "native_bits"):
                integer(row[key], 1, 32, "Timer waveform " + key)
            integer(row["channel_count"], 1, 32, "Timer waveform channel count")
            for key in ("advanced", "master"):
                _boolean(row[key])
            Symbol(row["clock_domain"])
            integer(row["divided_clock_multiplier"], 1, context="Timer clock multiplier")
            for key in ("dma", "itr_sources"):
                values = _sequence(row[key])
                if (any(not isinstance(item, str) or not item for item in values) or
                        len(set(values)) != len(values)):
                    fail("Timer waveform routes require distinct explicit identities")
            controllers[name] = freeze(dict(row))
        routes = {}
        for row in _sequence(value["routes"]):
            _fields(row, {"id", "controller", "kind", "channels", "pins", "complementary",
                          "internal_trigger", "period_max", "default_master"})
            identity = row["id"]
            if (not isinstance(identity, str) or not identity or identity in routes or
                    not isinstance(row["controller"], str) or not isinstance(row["kind"], str) or
                    row["controller"] not in controllers or row["kind"] not in modes):
                fail("Timer waveform route requires a distinct declared controller and kind")
            channels = _sequence(row["channels"])
            for channel in channels:
                integer(channel, 1, controllers[row["controller"]]["channel_count"],
                        "Timer waveform channel")
            if not channels or len(set(channels)) != len(channels):
                fail("Timer waveform route requires unique channels")
            pins = []
            for pin in _sequence(row["pins"]):
                pin = _sequence(pin)
                if (len(pin) != 3 or any(not isinstance(item, str) or not item
                                          for item in pin[:2])):
                    fail("Timer waveform pin requires wire, role and alternate")
                integer(pin[2], 0, 15, "Timer waveform alternate")
                pins.append(pin)
            if not pins or len({pin[0] for pin in pins}) != len(pins):
                fail("Timer waveform pads must be nonempty and distinct")
            _boolean(row["complementary"])
            _boolean(row["internal_trigger"])
            integer(row["period_max"], 1, context="Timer waveform period maximum")
            if row["default_master"] not in masters:
                fail("Timer waveform route has an unknown master mode")
            routes[identity] = freeze({**row, "channels": tuple(channels), "pins": tuple(pins)})
        if not routes:
            fail("Timer waveform route catalog is required")
        ranges = []
        for row in _sequence(value["deadtime_ranges"]):
            row = _sequence(row)
            if len(row) != 3:
                fail("Timer deadtime range requires minimum, maximum and quantum")
            low, high, step = row
            integer(low, context="Timer deadtime minimum")
            integer(high, low, context="Timer deadtime maximum")
            integer(step, 1, context="Timer deadtime quantum")
            if ranges and low <= ranges[-1][1]:
                fail("Timer deadtime ranges must be ordered and disjoint")
            ranges.append(row)
        if not ranges:
            fail("Timer deadtime encoding ranges are required")
        limits = tuple(integer(value[name], 1, context="Timer waveform " + name)
            for name in ("prescaler_max", "poll_limit_max", "burst_base", "burst_items"))
        return cls(freeze(dict(modes)), MappingProxyType(controllers), MappingProxyType(routes),
                   tuple(ranges), masters, internal_masters, syncs, *limits)

    def _route(self, route):
        facts = self.routes.get(route["id"])
        actual = tuple((pin["pin"], pin["function"], pin["af"]) for pin in route["pins"])
        if (facts is None or route["kind"] != facts["kind"] or
                route["controller"] != facts["controller"] or actual != facts["pins"]):
            fail("Timer waveform requires its exact reviewed channel/pad route")
        return facts

    def validate(self, selection, route):
        facts = self._route(route)
        controller = self.controllers[facts["controller"]]
        required = _FIELDS - {"calls_os", "master_trigger", "synchronization"}
        if required - selection.keys():
            fail("Timer waveform requires explicit units, safe levels, budget and IRQ")
        if selection["mode"] != self.modes[facts["kind"]]:
            fail("Timer waveform mode differs from its declared controller route")
        integer(selection["tick_hz"], 1, context="Timer tick_hz")
        integer(selection["period_ticks"], 1, facts["period_max"], "Timer period_ticks")
        integer(selection["poll_limit"], 1, self.poll_limit_max, "Timer poll_limit")
        deadtime = integer(selection["deadtime_ticks"], 0,
                           self.deadtime_ranges[-1][1], "Timer deadtime")
        if (not any(low <= deadtime <= high and deadtime % step == 0
                    for low, high, step in self.deadtime_ranges) or
                (not controller["advanced"] and deadtime)):
            fail("Timer deadtime has no exact selected hardware encoding")
        for name in ("break_active_high", "idle_high", "idle_complement_high", "exclusive"):
            _boolean(selection[name])
        if not selection["exclusive"] or selection.get("calls_os", False):
            fail("Timer waveform requires one exclusive owner and non-OS IRQs")
        master = selection.get("master_trigger", facts["default_master"])
        if (not isinstance(master, str) or master not in self.master_modes or
                (master != "none" and not controller["master"]) or
                (master in self.internal_master_modes and not facts["internal_trigger"])):
            fail("Timer master trigger is unavailable for its selected channels")
        sync = selection.get("synchronization")
        if sync is not None and (
                not isinstance(sync, dict) or set(sync) != {"source", "mode"} or
                not isinstance(sync["source"], str) or not sync["source"] or
                sync["mode"] not in self.sync_modes or sync["mode"] == "none" or
                not controller["itr_sources"]):
            fail("Timer synchronization requires an explicit source and real ITR input")
        if facts["internal_trigger"] and selection["period_ticks"] < 2:
            fail("Timer internal compare requires at least two ticks")
        if facts["kind"] == "timer_sequence":
            integer(selection.get("burst_budget_hclk_cycles"), 1, context="Timer burst budget")
        elif "burst_budget_hclk_cycles" in selection:
            fail("Timer burst budget belongs only to DMA sequence mode")

    def resolve_fields(self, selection, route, clock):
        facts = self._route(route)
        controller = self.controllers[facts["controller"]]
        bus = clock[controller["clock_domain"]]
        timer_hz = bus * (1 if bus == clock["core_hz"] else controller["divided_clock_multiplier"])
        tick = selection["tick_hz"]
        if timer_hz % tick or not 1 <= timer_hz // tick <= self.prescaler_max:
            fail("Timer tick_hz must exactly divide the actual timer source within its prescaler")
        budget = selection.get("burst_budget_hclk_cycles", 0)
        if budget * tick >= selection["period_ticks"] * clock["core_hz"]:
            fail("Timer burst budget must fit strictly inside the shortest period")
        return dict(channels=facts["channels"], complementary=facts["complementary"],
            internal_trigger=facts["internal_trigger"], counter_bits=controller["counter_bits"],
            native_bits=controller["native_bits"], burst_base=self.burst_base,
            burst_items=self.burst_items, burst_budget_hclk_cycles=budget,
            master_trigger=selection.get("master_trigger", facts["default_master"]),
            sync_source=None, sync_mode="none", itr=0)

    def resolve_edges(self, selected):
        owners = {item["id"]: item for item in selected if item["kind"] in self.KINDS}
        for item in owners.values():
            sync = item.get("synchronization")
            if sync is None:
                continue
            source = owners.get(sync["source"])
            inputs = self.controllers[item["controller"]]["itr_sources"]
            if (source is None or source is item or
                    source["waveform_resolution"]["master_trigger"] == "none"):
                fail("Timer internal trigger source must be a selected distinct TRGO owner")
            if source["controller"] not in inputs:
                fail("Timer source has no real ITR/internal trigger connection to target")
            item["waveform_resolution"].update(sync_source=source["id"],
                sync_mode=sync["mode"], itr=inputs.index(source["controller"]))
        for identity in owners:
            visited, current = set(), identity
            while current is not None:
                if current in visited:
                    fail("Timer internal trigger synchronization cycle")
                visited.add(current)
                current = owners[current]["waveform_resolution"]["sync_source"]

    def dma_paths(self, route, mode):
        facts = self._route(route)
        if mode != self.modes[facts["kind"]]:
            fail("Timer waveform DMA selection differs from its declared mode")
        return self.controllers[facts["controller"]]["dma"] if facts["kind"] == "timer_sequence" else ()

    def extra_pins(self, controller):
        """Return only pads declared for this exact controller, in author order."""
        return tuple(dict.fromkeys(pin for facts in self.routes.values()
            if facts["controller"] == controller for pin, _, _ in facts["pins"]))
