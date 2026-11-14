"""Package-owned shared platform records over a narrow immutable selection.

Recipes see physical budgets and selected ownership only. Hardware algorithms,
rollback and recovery remain in C; no family adapter or configuration engine is
exposed to static object projection.
"""
from collections.abc import Mapping
from dataclasses import dataclass
import re
from types import MappingProxyType

from tools.configure import gpio_phase
from tools.configure.calls import Address, DirectCall, Symbol
from tools.configure.providers.common import fail
from tools.configure.static_ir import ControllerPowerConnection, Flags
from tools.configure.static_recipe import StaticObjects


def _fields(value, required, optional=()):
    if (not isinstance(value, Mapping) or not set(required).issubset(value) or
            set(value) - set(required) - set(optional)):
        fail("Platform binding requires exact named fields")


def _names(values):
    if (not isinstance(values, list) or any(not isinstance(value, str) or
            not value for value in values) or len(set(values)) != len(values)):
        fail("Platform selection requires distinct names")
    return tuple(values)


@dataclass(frozen=True)
class Selection:
    kinds: tuple = ()
    modes: tuple = ()
    controllers: tuple = ()
    parts: tuple = ()

    @classmethod
    def create(cls, value):
        _fields(value, (), cls.__dataclass_fields__)
        if not value:
            fail("Platform role selection requires explicit constraints")
        return cls(**{key: _names(values) for key, values in value.items()})

    def matches(self, item, part):
        return all(not allowed or actual in allowed for actual, allowed in (
            (item.kind, self.kinds), (item.mode, self.modes),
            (item.controller, self.controllers), (part, self.parts)))


@dataclass(frozen=True)
class PlatformBindingIR:
    """Only immutable values used by shared storage and fixed connections."""

    flash_bytes: int
    has_dma: bool
    has_syscfg: bool
    has_audio: bool
    has_can: bool
    has_filters: bool
    has_ethernet: bool
    ethernet_frames: bool
    has_pins: bool
    has_clocks: bool
    has_boot: bool
    dma_present: bool | None
    syscfg_present: bool | None
    can_present: bool | None
    filters_present: bool | None
    ethernet_present: bool | None
    pins_present: bool | None
    clocks_present: bool | None
    boot_present: bool | None
    dma_mask: object
    filters: tuple
    filter_count: int
    can_split: int
    permanent_port: Address | None


@dataclass(frozen=True)
class PlatformBindings:
    headers: tuple
    roles: Mapping
    dma_clocks: Mapping
    objects: Mapping
    calls: Mapping
    config_symbol: str
    suspend: str
    resume: str
    retain_on_failed_resume: bool

    @classmethod
    def from_validated(cls, value):
        _fields(value, {"schema_version", "headers", "roles",
            "dma_clocks", "objects", "calls", "config_symbol", "power"})
        if type(value["schema_version"]) is not int or value["schema_version"] != 1:
            fail("Unsupported platform binding schema")
        headers = _names(value["headers"])
        if any(re.fullmatch(r"[A-Za-z0-9_/-]+\.h", name) is None or
               ".." in name.split("/") for name in headers):
            fail("Platform headers require relative private C header names")
        roles = value["roles"]
        _fields(roles, {"syscfg", "audio", "ethernet", "ethernet_frames", "power"})
        parsed_roles = {}
        for name, selections in roles.items():
            if not isinstance(selections, list):
                fail("Platform roles require explicit ordered selectors")
            parsed_roles[name] = tuple(Selection.create(item) for item in selections)
        clocks = value["dma_clocks"]
        if not isinstance(clocks, Mapping):
            fail("Platform DMA clocks require named engine symbols")
        for engine, mask in clocks.items():
            Symbol(engine)
            Symbol(mask)
        objects = value["objects"]
        _fields(objects, {"state", "platform", "permanent_close", "can_close"})
        calls = value["calls"]
        _fields(calls, {"system_start", "resources_acquire", "resources_resume",
                        "resources_release", "system_release"})
        for name in (*calls.values(), value["config_symbol"]):
            Symbol(name)
        power = value["power"]
        _fields(power, {"suspend", "resume", "retain_on_failed_resume"})
        Symbol(power["suspend"])
        Symbol(power["resume"])
        if type(power["retain_on_failed_resume"]) is not bool:
            fail("Platform power retention requires an explicit boolean")
        return cls(headers, MappingProxyType(parsed_roles), MappingProxyType(dict(clocks)),
            MappingProxyType({name: StaticObjects.from_validated(group)
                              for name, group in objects.items()}),
            MappingProxyType(dict(calls)), value["config_symbol"],
            power["suspend"], power["resume"], power["retain_on_failed_resume"])

    def _role(self, name, result):
        return any(rule.matches(item, result.part)
                   for rule in self.roles[name] for item in result.controllers)

    def selection(self, result):
        engines = sorted({lane.engine for item in result.controllers
                          for lane in item.integration.dma})
        for engine in engines:
            if engine not in self.dma_clocks:
                fail("Selected DMA engine lacks a platform clock binding: " + engine)
        masks = tuple(Symbol(self.dma_clocks[engine]) for engine in engines)
        dma, syscfg = bool(engines), self._role("syscfg", result)
        ethernet = self._role("ethernet", result)
        pins = gpio_phase.applies(result)
        domain = result.can_domain
        filters = domain.filters if domain is not None else ()
        owners = {region.endpoint.controller for region in result.external_memory}
        if len(owners) > 1:
            fail("Platform startup memory requires one permanent controller owner")
        permanent = (Address("s_nx_port_" + next(iter(owners)).replace("-", "_"))
                     if owners else None)
        present = lambda selected: True if selected else None
        return PlatformBindingIR(result.flash["size"], dma, syscfg,
            self._role("audio", result), domain is not None, bool(filters), ethernet,
            self._role("ethernet_frames", result), pins, dma or syscfg, bool(owners),
            present(dma), present(syscfg), present(domain is not None),
            present(filters), present(ethernet), present(pins), present(dma or syscfg),
            present(owners), Flags(masks) if masks else 0, filters, len(filters),
            domain.split if domain is not None else 0, permanent)

    def state_declarations(self, result):
        nodes = self.objects["state"].project(self.selection(result))
        startup = getattr(result.provider_context.domain_policies, "EXTERNAL_MEMORY", None)
        return nodes + (tuple(startup.declarations(result)) if startup is not None else ())

    def platform_declarations(self, result):
        selected = self.selection(result)
        return (*self.objects["permanent_close"].project(selected if selected.has_boot else None),
                *self.objects["can_close"].project(selected if selected.has_can else None),
                *self.objects["platform"].project(selected))

    def instance_declarations(self, item, result):
        """Device recipes own their endpoint state and public faces."""
        return ()

    def includes(self, result):
        return ['#include "' + header + '"' for header in self.headers]

    def _call(self, name, arguments=()):
        return DirectCall(self.calls[name], (Address(self.config_symbol), *arguments))

    def platform_system_start(self, result):
        return self._call("system_start")

    def platform_resources_acquire(self, result):
        return self._call("resources_acquire")

    def platform_resources_resume(self, result):
        return self._call("resources_resume")

    def platform_resources_release(self, result):
        return self._call("resources_release")

    def platform_system_release(self, result, started):
        return self._call("system_release", (started,))

    def platform_power_controllers(self, result, initialization_indices):
        return tuple(ControllerPowerConnection(index,
            DirectCall(self.suspend, (Address("s_nx_port_" + item.id.replace("-", "_")),)),
            DirectCall(self.resume, (Address("s_nx_port_" + item.id.replace("-", "_")),)),
            self.retain_on_failed_resume)
            for index, ordinal in enumerate(initialization_indices)
            for item in (result.controllers[ordinal],)
            if any(rule.matches(item, result.part) for rule in self.roles["power"]))
