"""Small reviewed binding facts, without executable predicates or registers."""
from collections.abc import Mapping
from dataclasses import dataclass, replace
from types import MappingProxyType

from tools.configure.calls import Symbol
from tools.configure.providers.common import fail, integer
from tools.configure.resource_graph import identifier, normalize_resource


def _fields(value, required, optional=()):
    if (not isinstance(value, Mapping) or not set(required).issubset(value) or
            set(value) - set(required) - set(optional)):
        fail("Binding policy requires exact named fields")


@dataclass(frozen=True)
class Timebase:
    timer: str
    irq: str
    gate: str
    reset: str
    handler: str
    clock_domain: str
    counter_bits: int

    @classmethod
    def create(cls, value):
        _fields(value, cls.__dataclass_fields__)
        for name in ("timer", "irq", "gate", "reset", "handler", "clock_domain"):
            Symbol(value[name])
        if type(value["counter_bits"]) is not int or value["counter_bits"] not in {
                16, 32, 64}:
            fail("Timebase requires explicit counter width")
        return cls(**value)


@dataclass(frozen=True)
class ModeExclusion:
    device: str
    kind: str
    option: str
    equals: bool | int | str
    message: str

    @classmethod
    def create(cls, value):
        _fields(value, cls.__dataclass_fields__)
        for name in ("device", "kind", "option"):
            identifier(value[name], "Mode exclusion " + name)
        if type(value["equals"]) not in {bool, int, str}:
            fail("Mode exclusion requires one scalar comparison value")
        if not isinstance(value["message"], str) or not value["message"].strip():
            fail("Mode exclusion requires its reviewed reason")
        return cls(**value)


@dataclass(frozen=True)
class BindingPolicy:
    custom_clock_profile: str
    oscillators: Mapping
    timebase: Timebase
    exclusions: tuple[ModeExclusion, ...]
    pinmux: Mapping
    pad_facts: Mapping
    dma_facts: Mapping
    irq_facts: Mapping
    controller_irqs: Mapping

    @classmethod
    def from_validated(cls, value):
        _fields(value, {"schema_version", "custom_clock_profile", "system",
                        "mode_exclusions"}, {"pinmux", "software", "hardware", "domains", "pads"})
        if type(value["schema_version"]) is not int or value["schema_version"] != 1:
            fail("Unsupported binding policy schema")
        profile = identifier(value["custom_clock_profile"], "Custom clock profile")
        system = value["system"]
        _fields(system, {"oscillator_resources", "timebase"})
        raw = system["oscillator_resources"]
        if not isinstance(raw, Mapping):
            fail("Oscillator resources require a named mapping")
        oscillators = {}
        for name, resources in raw.items():
            identifier(name, "Oscillator source")
            if (not isinstance(resources, list) or not resources or
                    len(set(resources)) != len(resources)):
                fail("Oscillator requires distinct explicit resources")
            oscillators[name] = tuple(identifier(item, "Oscillator resource")
                                      for item in resources)
        exclusions = value["mode_exclusions"]
        if not isinstance(exclusions, list):
            fail("Mode exclusions require an explicit list")
        pinmux = value.get("pinmux", {})
        if not isinstance(pinmux, Mapping):
            fail("Pinmux constraints require a controller mapping")
        for controller, functions in pinmux.items():
            Symbol(controller)
            if (not isinstance(functions, list) or not functions or
                    any(type(item) is not int or not 0 <= item <= 15
                        for item in functions) or
                    len(set(functions)) != len(functions)):
                fail("Pinmux alternate functions require distinct values 0..15")
        hardware = value.get("hardware", {})
        if not isinstance(hardware, Mapping):
            fail("Private hardware bindings require an object")
        if "pads" in hardware:
            fail("Pad coordinates belong only in the package pads facts")
        raw_pads = value.get("pads", {})
        if not isinstance(raw_pads, Mapping):
            fail("Pad bindings require named coordinate records")
        pads, dma = {}, {}
        for name, row in raw_pads.items():
            identifier(name, "Pad binding identity")
            _fields(row, {"registers", "port_index", "number"})
            Symbol(row["registers"])
            integer(row["port_index"], 0, 255, "Pad port index")
            integer(row["number"], 0, 31, "Pad number")
            pads[name] = MappingProxyType(dict(row))
        for name, row in hardware.get("dma_lanes", {}).items():
            identifier(name, "DMA binding identity")
            _fields(row, {"registers", "stream", "irq", "handler", "flag_bank",
                          "flag_shift", "status_member", "clear_member"})
            for symbol in ("registers", "stream", "irq", "handler",
                           "status_member", "clear_member"):
                Symbol(row[symbol])
            if row["flag_bank"] not in {"L", "H"}:
                fail("DMA status bank requires an explicit L/H identity")
            integer(row["flag_shift"], 0, 31, "DMA flag shift")
            dma[name] = MappingProxyType(dict(row))
        return cls(profile, MappingProxyType(oscillators),
                   Timebase.create(system["timebase"]),
                   tuple(ModeExclusion.create(item) for item in exclusions),
                   MappingProxyType({key: tuple(items)
                                     for key, items in pinmux.items()}),
                   MappingProxyType(pads), MappingProxyType(dma),
                   MappingProxyType({}), MappingProxyType({}))

    def with_irq_bindings(self, bindings, controllers):
        """Reference the selected catalog's IRQ owner without another author."""
        from tools.configure.providers.common import irq_resource_bindings
        return replace(self, irq_facts=irq_resource_bindings(bindings), controller_irqs=MappingProxyType({
            name: tuple(value["irq"]) for name, value in controllers.items()}))

    def pad_bindings(self, pins):
        from tools.configure.ir import PadBindingIR
        if not self.pad_facts:
            return ()
        result = []
        for pin in pins:
            if pin["pin"] not in self.pad_facts:
                fail("Selected pad lacks a private static binding: " + pin["pin"])
            row = self.pad_facts[pin["pin"]]
            result.append(PadBindingIR(pin["pin"], row["registers"], row["port_index"],
                row["number"], pin["af"], pin["function"]))
        return tuple(result)

    def dma_bindings(self, lanes):
        from tools.configure.ir import DmaBindingIR
        if not self.dma_facts:
            return ()
        result = []
        for lane in lanes:
            if lane.physical_key not in self.dma_facts:
                fail("Selected DMA lane lacks a private static binding")
            row = self.dma_facts[lane.physical_key]
            result.append(DmaBindingIR(lane.engine, lane.lane, lane.request,
                lane.physical_key, **row))
        return tuple(result)

    def vector_bindings(self, irqs):
        from tools.configure.ir import InterruptBindingIR
        if not self.irq_facts:
            return ()
        result = []
        for name, _ in irqs:
            key = normalize_resource("irq:" + name).removeprefix("irq:")
            if key not in self.irq_facts:
                fail("Selected publisher lacks a reviewed startup binding")
            row = self.irq_facts[key]
            result.append(InterruptBindingIR(row.enum, row.handler, row.number))
        return tuple(result)

    def controller_vectors(self, controller):
        return self.vector_bindings(tuple((name, None)
            for name in self.controller_irqs.get(controller, ())))

    def owner_vectors(self, controller, claims, dma):
        """Project active non-DMA publishers from the canonical resource plan."""
        active = {normalize_resource(claim.key).removeprefix("irq:")
                  for claim in claims if claim.key.startswith("irq:")}
        dma_names = {normalize_resource("irq:" + lane.irq).removeprefix("irq:") for lane in dma}
        active.difference_update(dma_names)
        names = [normalize_resource("irq:" + name).removeprefix("irq:")
                 for name in self.controller_irqs.get(controller, ())
                 if normalize_resource("irq:" + name).removeprefix("irq:") in active]
        names.extend(sorted(active - set(names)))
        return self.vector_bindings(tuple((name, None) for name in names))

    def oscillator_resources(self, source):
        return self.oscillators.get(source, ())

    def validate_selection(self, device, kind, options):
        for rule in self.exclusions:
            actual = options.get(rule.option)
            if (rule.device == device and rule.kind == kind and
                    type(actual) is type(rule.equals) and actual == rule.equals):
                fail(rule.message)

    def validate_route(self, route):
        allowed = self.pinmux.get(route["controller"])
        if allowed is not None and any(pin["af"] not in allowed
                                       for pin in route["pins"]):
            fail("Route alternate-function AF differs from its binding contract")

    def header_lines(self, prefix):
        Symbol(prefix)
        result = [f"#define {prefix}_TIMEBASE_{name.upper()} " +
                  getattr(self.timebase, name)
                  for name in ("timer", "irq", "gate", "reset", "handler")]
        result.append(f"#define {prefix}_TIMEBASE_COUNTER_BITS " +
                      str(self.timebase.counter_bits) + "u")
        return result
