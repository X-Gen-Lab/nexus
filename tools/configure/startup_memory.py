"""Explicit reset-backed memory placement and finite linker connections.

The external-bus policy owns bank geometry. This policy references that same
validated endpoint and admits image-lifetime ownership; it cannot manufacture
physical RAM, DMA reachability or a runtime memory-window loan.
"""
from collections.abc import Mapping
from dataclasses import dataclass
from types import MappingProxyType

from tools.configure.calls import Symbol
from tools.configure.ir import ExternalMemoryRegionIR
from tools.configure.providers.common import fail, integer
from tools.configure.static_ir import (
    Array, ArrayElementAddress, ConstantMacro, Declaration, Record,
)
from tools.configure.static_recipe import StaticObjects


_REGION_FIELDS = ("start", "end", "data_load", "data_start", "data_end",
                  "bss_start", "bss_end", "noinit_start", "noinit_end")


def _fields(value, required):
    if not isinstance(value, Mapping) or set(value) != set(required):
        fail("Startup memory policy requires exact named fields")


def _names(values, *, distinct=True):
    if (not isinstance(values, list) or not values or
            any(not isinstance(value, str) or not value for value in values) or
            (distinct and len(set(values)) != len(values))):
        fail("Startup memory requires explicit names")
    return tuple(values)


@dataclass(frozen=True)
class BootMemoryBindingIR:
    region_count: int
    bank: ArrayElementAddress
    regions: Array


@dataclass(frozen=True)
class StartupMemoryPolicy:
    region_id: str
    endpoint: Mapping
    endpoint_driver: str
    mode: str
    parts: tuple
    backends: tuple
    minimum_stack_bytes: int
    alignment: int
    bank_role: str
    bank_type: str
    linker_type: str
    address_encoder: str
    primary_symbols: tuple
    region_symbols: Mapping
    extra_symbols: tuple
    plan: StaticObjects

    @classmethod
    def from_validated(cls, value, *, external_bus):
        _fields(value, {"schema_version", "region_id", "endpoint_profile",
            "endpoint_driver", "mode", "parts", "backends", "minimum_stack_bytes",
            "alignment", "bank_role", "bank_type", "linker_type", "address_encoder",
            "primary_symbols", "region_symbols", "extra_symbols", "plan"})
        if type(value["schema_version"]) is not int or value["schema_version"] != 1:
            fail("Unsupported startup memory policy schema")
        profiles = getattr(external_bus, "endpoints", {})
        if (not isinstance(value["endpoint_profile"], str) or
                value["endpoint_profile"] not in profiles):
            fail("Startup memory requires an existing external endpoint profile")
        endpoint = profiles[value["endpoint_profile"]]
        if (endpoint.get("protocol") != "fixed-window" or
                value["mode"] not in endpoint["modes"]):
            fail("Startup memory requires the reviewed fixed-window profile")
        for name in ("region_id", "bank_role", "bank_type", "linker_type", "address_encoder"):
            Symbol(value[name])
        _names([value["endpoint_driver"], value["mode"]])
        minimum = integer(value["minimum_stack_bytes"], 1, context="Startup MSP minimum")
        alignment = integer(value["alignment"], 1, context="Startup memory alignment")
        if alignment & (alignment - 1):
            fail("Startup memory alignment must be a power of two")
        primary = _names(value["primary_symbols"], distinct=False)
        if len(primary) != len(_REGION_FIELDS):
            fail("Startup memory requires each primary linker boundary")
        raw = value["region_symbols"]
        if not isinstance(raw, Mapping) or value["region_id"] not in raw:
            fail("Startup memory requires the external region linker boundaries")
        regions = {}
        for name, symbols in raw.items():
            Symbol(name)
            regions[name] = _names(symbols)
            if len(regions[name]) != len(_REGION_FIELDS):
                fail("Startup memory requires each selected region linker boundary")
        extra = _names(value["extra_symbols"])
        for symbol in (*primary, *extra, *(symbol for symbols in regions.values() for symbol in symbols)):
            Symbol(symbol)
        return cls(value["region_id"], endpoint, value["endpoint_driver"], value["mode"],
            _names(value["parts"]), _names(value["backends"]), minimum, alignment,
            value["bank_role"], value["bank_type"], value["linker_type"],
            value["address_encoder"], primary, MappingProxyType(regions), extra,
            StaticObjects.from_validated(value["plan"]))

    def defer_budget(self, assembly, budgets, part):
        choices = budgets.get("regions", {})
        if not isinstance(choices, Mapping) or self.region_id not in choices:
            return ()
        if (part not in self.parts or assembly.backend not in self.backends or
                assembly.layout is not None or budgets.get("mpu") is not None or
                budgets.get("image_prefix_bytes", 0) or budgets.get("image_suffix_bytes", 0)):
            fail(self.region_id + " startup requires a reviewed full reset image without layout or MPU")
        if budgets["main_stack_bytes"] < self.minimum_stack_bytes:
            fail("Startup memory requires at least " + str(self.minimum_stack_bytes) +
                 " bytes of internal MSP stack")
        if assembly.optimization.endswith("-lto"):
            fail("Startup memory LTO requires an actual post-LTO stack proof")
        choice = choices[self.region_id]
        if not isinstance(choice, Mapping) or set(choice) != {"limit_bytes"}:
            fail(self.region_id + " startup budget requires only explicit limit_bytes")
        limit = integer(choice["limit_bytes"], 1, self.endpoint["bytes"],
                        self.region_id + " startup budget limit_bytes")
        if limit % self.alignment:
            fail(self.region_id + " startup budget requires explicit aligned bytes")
        return (self.region_id,)

    def resolve(self, budget, devices, *, assembly, variant):
        children = [item for item in devices if item.driver == self.endpoint_driver and
                    getattr(item.options, "mode", None) == self.mode]
        limits = [item for item in budget.regions if item.id == self.region_id]
        if not limits and not children:
            return ()
        if not limits:
            fail("Selected startup-memory requires an explicit " + self.region_id + " budget")
        if len(children) != 1:
            fail(self.region_id + " placement requires exactly one startup-memory child")
        child = children[0]
        if (child.options.address != self.endpoint["address"] or
                child.options.bytes != self.endpoint["bytes"] or
                limits[0].limit_bytes > child.options.bytes or
                limits[0].limit_bytes % self.alignment or
                assembly.backend not in self.backends):
            fail(self.region_id + " startup differs from its exact endpoint geometry")
        return (ExternalMemoryRegionIR(self.region_id, child),)

    def excluded_pins(self, result):
        """Reset-owned endpoint pads remain outside the cold restore lease."""
        pins = []
        for region in result.external_memory:
            child = region.endpoint
            route = result.routes[result.board_bindings[child.options.binding]["route"]]
            pins.extend(pin["pin"] for pin in route["pins"])
        return tuple(pins)

    def declarations(self, result):
        if not result.external_memory:
            return ()
        region, = result.external_memory
        child = region.endpoint
        children = tuple(item for item in result.devices if item.controller == child.controller)
        bank = "g_nx_" + self.bank_role + "_" + child.controller.replace("-", "_")
        selected = {item["id"] for item in result.physical_part.memory if item["linker"]}
        selected.add(self.region_id)
        regions = (self.primary_symbols, *(symbols for name, symbols in self.region_symbols.items()
                                         if name in selected))
        externs = sorted({symbol for symbols in regions for symbol in symbols} | set(self.extra_symbols))
        address = lambda name: ConstantMacro(Symbol(self.address_encoder), (Symbol(name),))
        values = tuple(Record(tuple((field, address(name))
            for field, name in zip(_REGION_FIELDS, symbols))) for symbols in regions)
        view = BootMemoryBindingIR(len(values),
            ArrayElementAddress(Symbol(bank), children.index(child)), Array(values))
        return (Declaration(self.bank_type, bank, count=len(children), const=True,
                            static=False, extern=True),
                *(Declaration(self.linker_type, name, static=False, extern=True) for name in externs),
                *self.plan.project(view))
