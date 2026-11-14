"""Finite cold GPIO records from selected package and resource facts.

The package owns C types and gate symbols. This policy observes no registers
and emits no behavior; concrete C owns admission, capture and failed cleanup.
"""
from collections.abc import Mapping
from dataclasses import dataclass, field
from types import MappingProxyType

from tools.configure.calls import Symbol
from tools.configure.providers.common import fail, integer, owned_claims
from tools.configure.static_ir import Array, Declaration, Flags, Record


def _fields(value, required, optional=()):
    if (not isinstance(value, Mapping) or not set(required).issubset(value) or
            set(value) - set(required) - set(optional)):
        fail("GPIO phase facts require exact named fields")


def _names(value):
    if (not isinstance(value, (tuple, list)) or any(
            not isinstance(item, str) or not item for item in value) or
            len(set(value)) != len(value)):
        fail("GPIO phase names require distinct explicit identifiers")
    return tuple(value)


def _gates(value, fields):
    if not isinstance(value, Mapping) or not value or set(value) - set(fields):
        fail("GPIO phase gates require reviewed clock fields")
    result = {}
    for field, masks in value.items():
        names = _names(masks)
        if not names:
            fail("GPIO phase gate requires nonempty constant masks")
        result[field] = tuple(Symbol(name) for name in names)
    return MappingProxyType(result)


def _binding_policy(result):
    context = getattr(result, "provider_context", None)
    policy = getattr(context, "binding_policy", None)
    if policy is None:
        fail("GPIO phase requires selected static binding facts")
    return policy


@dataclass(frozen=True)
class GpioPhasePolicy:
    types: Mapping
    excluded_kinds: tuple[str, ...]
    admission_kinds: tuple[str, ...]
    clock_fields: tuple[str, ...]
    controller_kinds: tuple[str, ...]
    controller_gates: Mapping
    kind_gates: Mapping
    mode_gates: tuple
    claim_gates: Mapping
    resource_gates: Mapping
    exti_lines: Mapping
    output_speeds: Mapping = field(default_factory=lambda: MappingProxyType({}))

    @classmethod
    def from_validated(cls, value):
        _fields(value, set(cls.__dataclass_fields__) - {"output_speeds"},
                {"output_speeds"})
        types = value["types"]
        _fields(types, {"receipt", "pin", "config", "dma_stream", "snapshot"})
        for name in types.values():
            Symbol(name)
        fields = _names(value["clock_fields"])
        if not fields:
            fail("GPIO phase requires its C clock fields")
        for name in fields:
            Symbol(name)
        gates = {}
        for field in ("controller_gates", "kind_gates", "claim_gates", "resource_gates"):
            raw = value[field]
            if not isinstance(raw, Mapping):
                fail("GPIO phase gate selections require named objects")
            gates[field] = MappingProxyType({name: _gates(row, fields)
                for name, row in raw.items()})
        for name in gates["controller_gates"]:
            Symbol(name)
        modes = []
        if not isinstance(value["mode_gates"], (tuple, list)):
            fail("GPIO phase mode gates require an ordered list")
        for row in value["mode_gates"]:
            _fields(row, {"controller", "modes", "clocks"})
            Symbol(row["controller"])
            modes.append((row["controller"], _names(row["modes"]),
                          _gates(row["clocks"], fields)))
        exti = value["exti_lines"]
        if not isinstance(exti, Mapping):
            fail("GPIO phase EXTI lines require explicit controller masks")
        for name, mask in exti.items():
            Symbol(name)
            integer(mask, 1, context="GPIO phase EXTI mask")
            if mask & (mask - 1):
                fail("GPIO phase EXTI line requires exactly one bit")
        speeds = value.get("output_speeds", {})
        if not isinstance(speeds, Mapping):
            fail("GPIO output speeds require explicit pad records")
        selected_speeds = {}
        from tools.configure.resource_graph import identifier
        for pad, choices in speeds.items():
            identifier(pad, "GPIO output speed pad")
            names = _names(choices)
            if not names or set(names) - {"low", "medium", "high", "very-high"}:
                fail("GPIO output speed requires explicit supported values")
            selected_speeds[pad] = names
        return cls(MappingProxyType(dict(types)), _names(value["excluded_kinds"]),
            _names(value["admission_kinds"]), fields,
            _names(value["controller_kinds"]), gates["controller_gates"],
            gates["kind_gates"], tuple(modes), gates["claim_gates"],
            gates["resource_gates"], MappingProxyType(dict(exti)),
            MappingProxyType(selected_speeds))

    def validate_output(self, selection, route):
        """Apply the selected silicon's pad limits to explicit slew intent."""
        if (route["kind"] != "gpio" or selection["mode"] != "output" or
                "speed" not in selection):
            return
        for pin in route["pins"]:
            permitted = self.output_speeds.get(pin["pin"])
            if permitted is not None and selection["speed"] not in permitted:
                fail("GPIO output speed differs from reviewed pad " + pin["pin"])

    def excluded_pins(self, result):
        owners = {item.id for item in result.controllers
                  if item.kind in self.excluded_kinds}
        pins = [claim.key.removeprefix("pin:")
                for claim in owned_claims(result, owners)
                if claim.key.startswith("pin:")]
        context = getattr(result, "provider_context", None)
        domains = getattr(context, "domain_policies", None)
        startup = getattr(domains, "EXTERNAL_MEMORY", None)
        if startup is not None:
            pins.extend(startup.excluded_pins(result))
        return tuple(pins)

    def requires_admission(self, result):
        return any(item.kind in self.admission_kinds for item in result.controllers)

    def pin_order(self, result, pin):
        row = _binding_policy(result).pad_facts.get(pin)
        if row is None:
            fail("Selected GPIO lease lacks a static pad binding: " + pin)
        return row["port_index"], row["number"]

    def snapshot_declaration(self, result, count):
        del result
        return Declaration(self.types["snapshot"], "s_nx_platform_pin_state", count=count)

    def static_declarations(self, result, pins, slots):
        declarations = [Declaration(self.types["receipt"], "s_nx_platform_pins")]
        config = None
        if pins:
            pads = _binding_policy(result).pad_bindings(tuple(
                {"pin": pin, "af": 0, "function": "lease"} for pin in pins))
            values = tuple(Record((("registers", Symbol(pad.registers)),
                ("snapshot", slots[pad.identity]), ("port_index", pad.port_index),
                ("pin", pad.number))) for pad in pads)
            config = Symbol("s_nx_platform_pins_config")
            declarations.append(Declaration(self.types["pin"], config.name,
                Array(values), const=True, count=len(values)))
        declarations.extend(self.admission_declarations(result, config, len(pins)))
        return declarations

    def admission_declarations(self, result, pins, pin_count):
        """Project exact claims; no register observation or initializer C text."""
        clocks = {name: set() for name in self.clock_fields}

        def select(gates):
            for name, masks in gates.items():
                clocks[name].update(masks)

        exti = 0
        for item in result.controllers:
            if item.kind in self.controller_kinds:
                if item.controller not in self.controller_gates:
                    fail("No reviewed cold gate for " + item.controller)
                select(self.controller_gates[item.controller])
            select(self.kind_gates.get(item.kind, {}))
            for controller, modes, gates in self.mode_gates:
                if item.controller == controller and item.mode in modes:
                    select(gates)
            if item.kind == "exti":
                if item.controller not in self.exti_lines:
                    fail("No reviewed cold EXTI mask for " + item.controller)
                exti |= self.exti_lines[item.controller]
        owners = {item.id for item in result.controllers}
        claims = tuple(owned_claims(result, owners))
        for claim in claims:
            if claim.key.startswith("controller:"):
                select(self.claim_gates.get(claim.key.removeprefix("controller:"), {}))
        for claim in result.resource_plan.claims:
            select(self.resource_gates.get(claim.key, {}))
        streams = {row.physical_key: row.stream for item in result.controllers
                   for row in item.integration.dma_bindings}
        selected = []
        for key in sorted({claim.key for claim in claims if claim.key.startswith("dma:")}):
            if key not in streams:
                fail("Selected DMA admission lacks a static lane binding: " + key)
            selected.append(Symbol(streams[key]))
        declarations = []
        if selected:
            declarations.append(Declaration(self.types["dma_stream"],
                "s_nx_platform_dma_streams", Array(tuple(selected)),
                const=True, pointer=True, count=len(selected)))
        masks = [0] * ((result.cpu_profile.irq.external_count + 31) // 32)
        vectors = sorted({claim.key.removeprefix("irq:") for claim in claims
                          if claim.key.startswith("irq:")})
        for vector in _binding_policy(result).vector_bindings(tuple(
                (name, 0) for name in vectors)):
            integer(vector.number, 0, result.cpu_profile.irq.external_count - 1,
                    "GPIO phase external IRQ")
            masks[vector.number // 32] |= 1 << (vector.number % 32)
        fields = [("pins", pins), ("pin_count", pin_count),
            ("dma_streams", Symbol("s_nx_platform_dma_streams") if selected else None),
            ("dma_stream_count", len(selected))]
        fields.extend((name, Flags(tuple(sorted(values, key=lambda item: item.name)))
                       if values else 0) for name, values in clocks.items())
        fields.extend((("irq_masks", Array(tuple(masks))), ("exti_lines", exti)))
        declarations.append(Declaration(self.types["config"], "s_nx_platform_config",
                                        Record(tuple(fields)), const=True))
        return declarations
