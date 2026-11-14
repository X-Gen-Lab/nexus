"""Pure free-frame serializer planning over selected package facts.

The package owns AF capabilities, kernel identity and electrical limits. Board
routes own pads and DMA requests. No family detection or hardware access occurs.
"""
from collections.abc import Mapping
from dataclasses import dataclass
from fractions import Fraction

from tools.configure.calls import Symbol
from tools.configure.providers.common import fail, integer, route_dma
from tools.configure.records import freeze
from tools.configure.resource_graph import identifier

MODES = tuple(role + "-" + direction + suffix for suffix in ("", "-blocks")
              for role in ("master", "slave")
              for direction in ("tx", "rx", "duplex"))
FIELDS = frozenset(("sample_bits", "slot_bits", "slots", "frame_sync_bits",
    "first_bit_offset", "frame_sync_high", "frame_sync_before",
    "frame_sync_channels", "first_lsb", "sample_rising", "mclk", "no_divider",
    "mckdiv", "max_bit_clock_hz", "poll_limit", "active_slots",
    "max_service_gap_us"))
DEFAULTS = dict(first_bit_offset=0, mclk=False, no_divider=False, mckdiv=0)
BOOLEANS = ("frame_sync_high", "frame_sync_before", "frame_sync_channels",
            "first_lsb", "sample_rising", "mclk", "no_divider")



def _fields(value, required):
    if not isinstance(value, Mapping) or set(value) != set(required):
        fail("SAI free-frame mechanism requires exact named facts")


@dataclass(frozen=True)
class SaiPolicy:
    data: Mapping

    @classmethod
    def from_validated(cls, value):
        _fields(value, {"mechanism", "controllers", "bounds", "sample_bits", "slot_bits", "frame_bits_max", "kernel", "bus_clock", "bus_margin", "frame_divider", "mclk_hz"})
        if value["mechanism"] != "free-frame-v1":
            fail("SAI free-frame mechanism is not reviewed")
        if not isinstance(value["controllers"], Mapping) or not value["controllers"]:
            fail("SAI requires explicit controller capabilities")
        for owner, row in value["controllers"].items():
            Symbol(owner)
            _fields(row, {"alternates", "dma_roles"})
            if not isinstance(row["alternates"], (tuple, list)) or not row["alternates"]:
                fail("SAI requires explicit reviewed alternates")
            for alternate in row["alternates"]:
                integer(alternate, 0, 15, "SAI alternate")
            _fields(row["dma_roles"], {"A", "B"})
            for paths in row["dma_roles"].values():
                if (not isinstance(paths, (tuple, list)) or not paths or
                        any(type(path) is not str for path in paths) or
                        len(set(paths)) != len(paths)):
                    fail("SAI DMA role requires explicit distinct reviewed routes")
                for path in paths:
                    identifier(path, "SAI DMA role route")
        expected = {"slots", "frame_sync_bits", "first_bit_offset", "mckdiv", "max_bit_clock_hz", "poll_limit"}
        _fields(value["bounds"], expected)
        for name, bounds in value["bounds"].items():
            if not isinstance(bounds, (tuple, list)) or len(bounds) != 2:
                fail("SAI bounds require an explicit pair")
            integer(bounds[0], context="SAI lower " + name)
            integer(bounds[1], bounds[0], context="SAI upper " + name)
        for key in ("sample_bits", "slot_bits", "mclk_hz"):
            if not isinstance(value[key], (tuple, list)) or not value[key]:
                fail("SAI geometry/electrical facts require explicit values")
            for number in value[key]:
                integer(number, 1, context="SAI " + key)
        if len(value["mclk_hz"]) != 2 or value["mclk_hz"][0] > value["mclk_hz"][1]:
            fail("SAI requires explicit MCLK bounds")
        _fields(value["kernel"], {"plan", "output", "frequency"})
        for name in (*value["kernel"].values(), value["bus_clock"]):
            Symbol(name)
        for name in ("frame_bits_max", "bus_margin", "frame_divider"):
            integer(value[name], 1, context="SAI " + name)
        return cls(freeze(value))

    @property
    def controllers(self):
        return tuple(self.data["controllers"])

    def handles(self, controller):
        return controller in self.data["controllers"]

    def source_block(self, route, mode, mclk=False):
        if route["kind"] != "audio" or not self.handles(route["controller"]):
            fail("SAI requires a reviewed free-frame controller")
        signals = tuple(pin["function"] for pin in route["pins"])
        block = "A" if signals[:2] == ("fs_a", "ck_a") else "B"
        suffix, peer = ("a", "b") if block == "A" else ("b", "a")
        expected = ("fs_" + suffix, "ck_" + suffix, "sd_" + suffix)
        if mode.removesuffix("-blocks").endswith("duplex"):
            expected += ("sd_" + peer,)
        if mclk:
            expected += ("mclk_" + suffix,)
        alternates = self.data["controllers"][route["controller"]]["alternates"]
        if signals != expected or any(pin["af"] not in alternates for pin in route["pins"]):
            fail("SAI route must select exact clock/data/optional MCLK roles")
        return block

    def validate(self, selection, route):
        """Reject unsupported personalities rather than reinterpreting I2S fields."""
        envelope = {"id", "binding", "mode", "irq_priority", "calls_os"}
        if ("irq_priority" not in selection or selection["mode"] not in MODES or
                FIELDS - DEFAULTS.keys() - {"active_slots", "max_service_gap_us"} - selection.keys() or
                selection.keys() - FIELDS - envelope):
            fail("SAI requires one supported free-frame selection")
        values = {**DEFAULTS, **selection}
        if values.get("calls_os", False):
            fail("SAI IRQs only latch hardware; calls_os must be false")
        for name in BOOLEANS:
            if type(values[name]) is not bool:
                fail("SAI " + name + " must be boolean")
        for name, bounds in self.data["bounds"].items():
            integer(values[name], *bounds, "SAI " + name)
        for name, choices in (("sample_bits", self.data["sample_bits"]),
                               ("slot_bits", self.data["slot_bits"])):
            if type(values[name]) is not int or values[name] not in choices:
                fail("SAI unsupported " + name)
        integer(values.get("active_slots", (1 << values["slots"]) - 1),
                1, (1 << values["slots"]) - 1, "SAI active_slots")
        if values["mode"].endswith("-blocks"):
            integer(values.get("max_service_gap_us"), 1, 1000000,
                    "SAI max_service_gap_us")
        elif "max_service_gap_us" in values:
            fail("SAI max_service_gap_us requires a continuous blocks mode")
        frame_bits = values["slot_bits"] * values["slots"]
        if (values["sample_bits"] > values["slot_bits"] or frame_bits > self.data["frame_bits_max"] or
                values["first_bit_offset"] > values["slot_bits"] - values["sample_bits"] or
                values["frame_sync_bits"] > frame_bits or
                (values["frame_sync_channels"] and (values["slots"] % 2 or
                    2 * values["frame_sync_bits"] != frame_bits))):
            fail("SAI frame, slot and synchronization fields are inconsistent")
        master = values["mode"].startswith("master-")
        if ((not master and (values["mclk"] or values["no_divider"] or values["mckdiv"])) or
                (values["no_divider"] and (values["mclk"] or values["mckdiv"])) or
                (master and not values["no_divider"] and
                 frame_bits & (frame_bits - 1))):
            fail("SAI role, MCLK and serializer divider are inconsistent")
        self.source_block(route, values["mode"], values["mclk"])


    def resolve_fields(self, selection, route, clock, clock_plan):
        """Calculate exact source/bus constraints; never author or operate RCC."""
        values = {**DEFAULTS, **selection}
        frame = values["slot_bits"] * values["slots"]
        master = values["mode"].startswith("master-")
        if master:
            path = self.data["kernel"]
            auxiliary = clock_plan.get(path["plan"]) if clock_plan is not None else None
            if auxiliary is None or auxiliary.get(path["output"]) is None:
                fail("SAI master requires the selected System kernel plan")
            kernel = auxiliary[path["output"]][path["frequency"]]
            divider = 1 if values["mckdiv"] == 0 else 2 * values["mckdiv"]
            bit_clock = (Fraction(kernel) if values["no_divider"] else
                         Fraction(kernel * frame, divider * self.data["frame_divider"]))
            if values["mclk"] and not self.data["mclk_hz"][0] <= Fraction(kernel, divider) <= self.data["mclk_hz"][1]:
                fail("SAI MCLK output is outside its reviewed electrical range")
            if bit_clock > values["max_bit_clock_hz"]:
                fail("SAI master bit clock exceeds the declared electrical ceiling")
        else:
            bit_clock = Fraction(values["max_bit_clock_hz"])
        if self.data["bus_margin"] * bit_clock >= clock[self.data["bus_clock"]]:
            fail("SAI APB2 must remain strictly above twice the bit clock")
        return {**{key: values[key] for key in DEFAULTS},
                "active_slots": values.get("active_slots", (1 << values["slots"]) - 1),
                "continuous": values["mode"].endswith("-blocks"),
                "max_service_gap_us": values.get("max_service_gap_us", 0),
                "source_block": self.source_block(route, values["mode"], values["mclk"])}


    def dma_paths(self, route, mode):
        """Admit ordered source/peer choices against reviewed IP role facts."""
        paths = tuple(route_dma(route, mode))
        expected = 2 if mode.removesuffix("-blocks").endswith("duplex") else 1
        if len(paths) != expected or len(set(path.rsplit(":", 1)[0] for path in paths)) != len(paths):
            fail("SAI DMA paths must select distinct source/peer directions")
        block = self.source_block(route, mode, any(
            pin["function"].startswith("mclk_") for pin in route["pins"]))
        roles = (block, "B" if block == "A" else "A")[:expected]
        reviewed = self.data["controllers"][route["controller"]]["dma_roles"]
        if any(path not in reviewed[role] for path, role in zip(paths, roles)):
            fail("SAI DMA route differs from its reviewed source/peer role")
        return paths
