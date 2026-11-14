"""Declarative private bindings over stable public I/O and finite C records.

This adapter selects no family Python module. It reads typed values and static
references only; hardware admission, lifecycle and algorithms stay in C.
"""
from collections.abc import Mapping
from dataclasses import dataclass, field, fields as dataclass_fields
import re
from types import MappingProxyType

from tools.configure.calls import Address, DirectCall, Symbol
from tools.configure.domain_bridge import DomainBridge
from tools.configure.records import freeze
from tools.configure.providers import common
from tools.configure.static_ir import Declaration, IrqConnection, Record


def _fields(value, required, optional=()):
    if (not isinstance(value, Mapping) or not set(required).issubset(value) or
            set(value) - set(required) - set(optional)):
        common.fail("Declarative binding has missing or unknown fields")


def _headers(values):
    if (not isinstance(values, (list, tuple)) or any(
            not isinstance(name, str) or name.startswith("/") or
            re.fullmatch(r"[A-Za-z0-9_/-]+\.h", name) is None or
            any(part in {"", ".", ".."} for part in name.split("/"))
            for name in values) or len(set(values)) != len(values)):
        common.fail("Data provider headers require distinct relative C header names")
    return tuple(values)


@dataclass(frozen=True)
class RecipeProfiles:
    """A finite lookup by the domain's single resolved execution-profile key."""
    profiles: Mapping
    state_type: None = None

    @classmethod
    def create(cls, value, *, fragments=None):
        from tools.configure.static_recipe import StaticRecipe
        if not isinstance(value, Mapping) or not value:
            common.fail("Static profiles require an explicit nonempty mapping")
        result = {}
        for name, recipe in value.items():
            if not isinstance(name, str) or re.fullmatch(r"[a-z][a-z0-9_-]*", name) is None:
                common.fail("Static profile requires an ordinary domain key")
            result[name] = StaticRecipe.from_validated(
                fragments.expand(recipe) if fragments is not None else recipe)
        return cls(MappingProxyType(result))

    def selected(self, item):
        key = getattr(getattr(item, "integration", None), "execution_profile", None)
        if key not in self.profiles:
            common.fail("Unsupported static execution profile: " + str(key))
        return self.profiles[key]

    def construct(self, item, result, board_bindings, routes, devices):
        return self.selected(item).construct(item, result, board_bindings, routes, devices)

    def operations(self, item):
        return self.selected(item).operations(item)


def binding_document(package, document, *, read):
    """Merge only explicitly owned static-layout and domain input fragments."""
    result = dict(document)
    software = dict(result.get("software", {}))
    bindings = list(software.get("bindings", []))
    domains = dict(result.get("domains", {}))
    libraries = {"declarations": {}, "values": {}}
    def collect(values):
        _fields(values, (), {"declarations", "values"})
        for section, declarations in values.items():
            if not isinstance(declarations, Mapping):
                common.fail("Static fragments require named declaration/value libraries")
            if set(libraries[section]).intersection(declarations):
                common.fail("Duplicate selected static fragment identity")
            libraries[section].update(declarations)
    collect(software.get("static_fragments", {}))
    collect(result.pop("static_fragments", {}))
    for path in package.binding_files:
        fragment = read(path)
        _fields(fragment, {"schema_version", "bindings"}, {"domains", "static_fragments"})
        if (type(fragment["schema_version"]) is not int or
                fragment["schema_version"] != 1 or
                not isinstance(fragment["bindings"], list)):
            common.fail("Unsupported binding fragment schema")
        bindings.extend(fragment["bindings"])
        collect(fragment.get("static_fragments", {}))
        values = fragment.get("domains", {})
        if not isinstance(values, Mapping):
            common.fail("Domain fragments require named fields")
        for key, value in values.items():
            if key == "schema_version":
                if type(value) is not int or value != 1:
                    common.fail("Unsupported domain fragment schema")
                continue
            if key in domains:
                common.fail("Duplicate selected domain policy: " + key)
            domains[key] = value
    if bindings or "software" in result:
        software["bindings"] = bindings
        if any(libraries.values()):
            software["static_fragments"] = libraries
        result["software"] = software
    if domains:
        domains.setdefault("schema_version", 1)
        result["domains"] = domains
    return result


def _source(value):
    if (not isinstance(value, str) or
            re.fullmatch(r"(?:controller|mode|options|integration|storage)"
                         r"(?:\.[a-z][a-z0-9_]*)*", value) is None or
            "__" in value):
        common.fail("Declarative field requires a named IR value or storage reference")
    return value


def _read(source, item, storage):
    parts = source.split(".")
    value = storage if parts[0] == "storage" else getattr(item, parts[0])
    try:
        for name in parts[1:]:
            value = value[name] if isinstance(value, Mapping) else getattr(value, name)
    except (KeyError, AttributeError) as error:
        common.fail("Missing typed binding value: " + source)
    return value


@dataclass(frozen=True)
class FieldBinding:
    name: str
    source: str
    type: str
    values: Mapping

    @classmethod
    def create(cls, value):
        _fields(value, {"name", "source", "type"}, {"values"})
        Symbol(value["name"])
        if value["type"] not in {"u32", "bool", "symbol", "address"}:
            common.fail("Unmaintained static binding field type")
        values = value.get("values", {})
        if not isinstance(values, Mapping) or any(
                not isinstance(key, str) or type(item) not in {int, bool, str}
                for key, item in values.items()):
            common.fail("Binding enum values require scalar constants")
        return cls(value["name"], _source(value["source"]), value["type"], freeze(values))

    def resolve(self, item, storage):
        value = _read(self.source, item, storage)
        if self.values:
            if value not in self.values:
                common.fail("Unsupported binding enum: " + self.source)
            value = self.values[value]
        if self.type == "u32":
            return common.integer(value, context=self.name)
        if self.type == "bool":
            if type(value) is not bool:
                common.fail("Binding field requires a boolean: " + self.name)
            return value
        return Symbol(value) if self.type == "symbol" else Address(value)


@dataclass(frozen=True)
class StorageBinding:
    name: str
    type: str
    count: str

    @classmethod
    def create(cls, value):
        _fields(value, {"name", "type", "count"})
        Symbol(value["name"])
        Symbol(value["type"])
        return cls(value["name"], value["type"], _source(value["count"]))


@dataclass(frozen=True)
class Binding:
    kind: str
    modes: tuple[str, ...]
    implementation: str
    state_type: str | None
    config_type: str | None
    ops: str | None
    prepare: str | None
    close: str | None
    fields: tuple[FieldBinding, ...]
    storage: tuple[StorageBinding, ...]
    irq: str | None
    validator: str | None
    limits: Mapping
    admit: str | None
    recipe: object | None = None
    controllers: tuple[str, ...] = ()
    headers: tuple[str, ...] = ()
    shared_irq: object | None = None
    profile_headers: Mapping | None = None

    @classmethod
    def create(cls, value, *, fragments=None):
        recipe = None
        required = {"kind", "implementation"}
        if not isinstance(value, Mapping) or ("mode" in value) == ("modes" in value):
            common.fail("Binding declares exactly one mode or modes list")
        mode_field = "mode" if "mode" in value else "modes"
        required.add(mode_field)
        optional = {"validator", "limits", "controllers", "headers", "shared_irq", "profile_headers"}
        if "static_recipe" in value:
            _fields(value, required | {"static_recipe"}, optional)
            from tools.configure.static_recipe import StaticRecipe
            recipe = StaticRecipe.from_validated(fragments.expand(value["static_recipe"])
                if fragments is not None else value["static_recipe"])
        elif "profiles" in value:
            _fields(value, required | {"profiles"}, optional)
            recipe = RecipeProfiles.create(value["profiles"], fragments=fragments)
        else:
            _fields(value, required | {"ops", "state_type", "config_type", "prepare",
                                      "close", "fields"},
                    optional | {"storage", "irq", "admit"})
        Symbol(value["implementation"])
        for name in ("state_type", "config_type", "prepare", "close",
                     "ops", "irq", "validator", "admit"):
            if value.get(name) is not None:
                Symbol(value[name])
        modes = (value["mode"],) if mode_field == "mode" else value["modes"]
        if not isinstance(modes, (tuple, list)) or not modes or len(set(modes)) != len(modes):
            common.fail("Binding mode aliases require a nonempty unique list")
        for name in (value["kind"], *modes):
            if (not isinstance(name, str) or
                    re.fullmatch(r"[a-z][a-z0-9_-]*", name) is None):
                common.fail("Binding kind and mode require ordinary identifiers")
        controllers = value.get("controllers", ())
        if (not isinstance(controllers, (tuple, list)) or
                len(set(controllers)) != len(controllers)):
            common.fail("Binding controllers require distinct explicit symbols")
        for controller in controllers:
            Symbol(controller)
        raw_fields, raw_storage = value.get("fields", []), value.get("storage", [])
        if not isinstance(raw_fields, list) or not isinstance(raw_storage, list):
            common.fail("Binding fields and storage require ordered lists")
        fields = tuple(FieldBinding.create(item) for item in raw_fields)
        storage = tuple(StorageBinding.create(item) for item in raw_storage)
        if len({item.name for item in fields}) != len(fields):
            common.fail("Duplicate binding config field")
        if len({item.name for item in storage}) != len(storage):
            common.fail("Duplicate binding storage identity")
        limits = value.get("limits", {})
        if not isinstance(limits, Mapping):
            common.fail("Binding limits require a field mapping")
        for name, bounds in limits.items():
            Symbol(name)
            if (not isinstance(bounds, list) or len(bounds) != 2 or
                    any(type(bound) is not int for bound in bounds) or
                    not 0 <= bounds[0] <= bounds[1] <= common.UINT32_MAX):
                common.fail("Binding limits require explicit uint32 bounds")
        from tools.configure.shared_irq import SharedIrqGroup
        shared = (SharedIrqGroup.from_validated(value["shared_irq"])
                  if "shared_irq" in value else None)
        profile_headers = value.get("profile_headers", {})
        if (not isinstance(profile_headers, Mapping) or profile_headers and (
                not isinstance(recipe, RecipeProfiles) or
                set(profile_headers) - set(recipe.profiles))):
            common.fail("Profile headers require explicitly selected recipe profiles")
        profile_headers = MappingProxyType({name: _headers(headers)
            for name, headers in profile_headers.items()})
        return cls(value["kind"], tuple(modes), value["implementation"],
            recipe.state_type if recipe is not None else value["state_type"],
            value.get("config_type"), value.get("ops"), value.get("prepare"),
            value.get("close"), fields, storage, value.get("irq"),
            value.get("validator"), freeze(limits), value.get("admit"), recipe,
            tuple(controllers), _headers(value.get("headers", [])), shared, profile_headers)

    def headers_for(self, item):
        headers = self.headers
        if isinstance(self.recipe, RecipeProfiles):
            self.recipe.selected(item)
            headers += self.profile_headers.get(item.integration.execution_profile, ())
        return headers


@dataclass(frozen=True)
class DataProvider(DomainBridge):
    """A selected data package; no runtime registry or host code extension."""

    package: object
    soc: Mapping
    software: Mapping
    domain_policies: object = None
    hardware_facts: object = None
    binding_policy: object = None
    REFERENCE_PROFILES: object = None
    bindings: tuple[Binding, ...] = field(init=False)

    MODEL = False

    @classmethod
    def from_package(cls, package, soc, document, *, read):
        """Read each declared layout input once through the sealed fact reader."""
        from tools.configure.domain_policies import DomainPolicies
        from tools.configure.hardware_bindings import load_hardware
        from tools.configure.package_policy import BindingPolicy
        selected = binding_document(package, document, read=read)
        policies = DomainPolicies.from_validated(package, soc, selected)
        hardware = load_hardware(package, read=read)
        from tools.configure.reference_profiles import ProfileSet
        profiles = (ProfileSet.from_validated(read(package.fact("review_profiles")))
                    if "review_profiles" in package.facts else None)
        return cls(package, soc, selected["software"], policies, hardware,
                   BindingPolicy.from_validated(selected), profiles)

    def __post_init__(self):
        _fields(self.software, {"enum_abi", "backends", "headers", "bindings"},
                {"freertos_port", "platform", "static_fragments"})
        if self.software["enum_abi"] not in {"short-enums", "native-int"}:
            common.fail("Data provider requires an explicit enum ABI")
        if (not isinstance(self.software["backends"], list) or
                not self.software["backends"] or
                set(self.software["backends"]) - {"baremetal", "freertos", "threadx"}):
            common.fail("Data provider requires explicit maintained backends")
        _headers(self.software["headers"])
        hooks = self.software.get("platform")
        platform = getattr(self.domain_policies, "PLATFORM", None)
        if platform is not None:
            if hooks not in (None, {}):
                common.fail("Platform binding and fixed C hooks cannot both own lifecycle")
        else:
            _fields(hooks, {"system_start", "resources_acquire", "resources_resume",
                            "resources_release", "system_release"})
            for name in hooks.values():
                Symbol(name)
        raw = self.software["bindings"]
        if not isinstance(raw, list) or not raw:
            common.fail("Data provider requires at least one binding")
        from tools.configure.static_fragments import StaticFragments
        fragments = StaticFragments.from_validated(self.software.get("static_fragments", {}))
        bindings = tuple(Binding.create(item, fragments=fragments) for item in raw)
        for index, item in enumerate(bindings):
            for other in bindings[:index]:
                if (item.kind == other.kind and set(item.modes).intersection(other.modes) and
                        (not item.controllers or not other.controllers or
                         set(item.controllers).intersection(other.controllers))):
                    common.fail("Duplicate data provider controller/mode binding")
        object.__setattr__(self, "bindings", bindings)
        object.__setattr__(self, "soc", freeze(self.soc))
        object.__setattr__(self, "software", freeze(self.software))

    def for_soc(self, soc):
        """Bind the exact normalized fact view without reparsing static layouts."""
        scoped = object.__new__(type(self))
        for declaration in dataclass_fields(self):
            value = freeze(soc) if declaration.name == "soc" else getattr(self, declaration.name)
            object.__setattr__(scoped, declaration.name, value)
        return scoped

    @property
    def PAD_FACTS(self):
        """Reference the package's one validated immutable coordinate index."""
        return self.binding_policy.pad_facts if self.binding_policy is not None else MappingProxyType({})

    @property
    def PREFIX(self):
        return self.package.vendor

    @property
    def ENUM_ABI(self):
        return self.software["enum_abi"]

    @property
    def FREERTOS_PORT(self):
        return self.software.get("freertos_port")

    @property
    def SOURCE_IMPORT_LOCK(self):
        return self.package.import_lock

    @property
    def CPU_ABI(self):
        return common.cpu_abi(self.soc["cpu"])

    @property
    def CPU_FEATURES(self):
        return {name: value for name, value in self.soc["cpu"].items()
                if name not in {"arch", "fpu", "float_abi"}}

    @property
    def DWT_CYCCNT(self):
        return self.soc["cpu"]["dwt_cyccnt"]

    @property
    def IRQ_PRIORITY_BITS(self):
        return self.soc["irq"]["priority_bits"]

    @property
    def EXTERNAL_IRQ_COUNT(self):
        return self.soc["irq"]["external_count"]

    @property
    def CLOCK_PROFILES(self):
        return self.soc["clock_profiles"]

    @property
    def CONTROLLER_INTERFACES(self):
        return {name: value.get("interfaces", {})
                for name, value in self.soc["controllers"].items()}

    @property
    def UART_TX_VALIDATORS(self):
        return {(self.package.family, mode, item.kind): Symbol(item.validator)
                for item in self.bindings if item.validator is not None
                for mode in item.modes}

    def binding(self, kind, mode, controller=None):
        candidates = tuple(item for item in self.bindings
            if item.kind == kind and mode in item.modes and
            (controller is None or not item.controllers or controller in item.controllers))
        if not candidates:
            common.fail(f"Unsupported package binding: {kind}/{mode}")
        if len(candidates) != 1 or controller is None and candidates[0].controllers:
            common.fail("Controller is required for unique package binding selection")
        return candidates[0]

    def mode_implementation(self, kind, mode, controller=None):
        if any(value.kind == kind and mode in value.modes for value in self.bindings):
            return self.binding(kind, mode, controller).implementation
        external = getattr(self.domain_policies, "EXTERNAL_BUS", None)
        modes = (external.interface_parent_modes(controller, kind, mode)
                 if external is not None else ())
        implementations = {self.binding("external_bus", parent, controller).implementation
                           for parent in modes}
        if len(implementations) == 1:
            return implementations.pop()
        common.fail(f"Unsupported or ambiguous package interface: {kind}/{mode}")

    def operations(self, item):
        binding = self.binding(item.kind, item.mode, item.controller)
        return (binding.recipe.operations(item) if binding.recipe is not None
                else binding.ops)

    def constructor(self, item, result, board_bindings, routes, devices):
        from tools.configure.ir import coherent_controller_ir
        coherent_controller_ir(item, result, self)
        return self._construct_layout(item, result, board_bindings, routes, devices)

    def _construct_layout(self, item, result, board_bindings, routes, devices):
        """Encode finite layout nodes; public construction first checks the graph."""
        binding = self.binding(item.kind, item.mode, item.controller)
        if binding.recipe is not None:
            return binding.recipe.construct(item, result, board_bindings, routes, devices)
        name = item.id.replace("-", "_")
        storage, declarations = {}, []
        for selected in binding.storage:
            symbol = "s_nx_storage_" + name + "_" + selected.name
            count = common.integer(_read(selected.count, item, storage), 1,
                                   context="Static storage capacity")
            declarations.append(Declaration(selected.type, symbol, count=count))
            storage[selected.name] = symbol
        config = "s_nx_config_" + name
        declarations.append(Declaration(binding.config_type, config,
            Record(tuple((item_field.name, item_field.resolve(item, storage))
                         for item_field in binding.fields)), const=True))
        state = Address("s_nx_port_" + name)
        irqs = ((IrqConnection(item.controller + "_IRQHandler", (
            DirectCall(binding.irq, (state,)),), exit_barrier=Symbol("nx_arch_dsb")),)
            if binding.irq is not None else ())
        return common.Construction(binding.state_type, (),
            DirectCall(binding.prepare, (state, Address(config))),
            (DirectCall(binding.close, (state,)),), True,
            admission=((DirectCall(binding.admit, (state, Address(config))),)
                       if binding.admit is not None else ()),
            static_declarations=declarations, irq_connections=irqs)

    def includes(self, result):
        headers = list(self.software["headers"])
        for item in result.controllers:
            headers.extend(self.binding(item.kind, item.mode, item.controller).headers_for(item))
        source = ['#include "' + name + '"' for name in dict.fromkeys(headers)]
        platform = getattr(self.domain_policies, "PLATFORM", None)
        if platform is not None:
            source.extend(platform.includes(result))
        return list(dict.fromkeys(source))

    def _platform(self, name, result, arguments=()):
        platform = getattr(self.domain_policies, "PLATFORM", None)
        if platform is not None:
            return getattr(platform, "platform_" + name)(result, *arguments)
        return DirectCall(self.software["platform"][name], arguments)

    def platform_system_start(self, result):
        return self._platform("system_start", result)

    def platform_resources_acquire(self, result):
        return self._platform("resources_acquire", result)

    def platform_resources_resume(self, result):
        return self._platform("resources_resume", result)

    def platform_resources_release(self, result):
        return self._platform("resources_release", result)

    def platform_system_release(self, result, started):
        return self._platform("system_release", result, (started,))

    def state_declarations(self, result):
        platform = getattr(self.domain_policies, "PLATFORM", None)
        return platform.state_declarations(result) if platform is not None else ()

    def platform_declarations(self, result):
        platform = getattr(self.domain_policies, "PLATFORM", None)
        return platform.platform_declarations(result) if platform is not None else ()

    def platform_power_controllers(self, result, initialization_indices):
        platform = getattr(self.domain_policies, "PLATFORM", None)
        return (platform.platform_power_controllers(result, initialization_indices)
                if platform is not None else ())

    instance_declarations = staticmethod(lambda item, result: [])
    def shared_irqs(self, result):
        from tools.configure.shared_irq import connections
        selected = []
        for item in result.controllers:
            binding = self.binding(item.kind, item.mode, item.controller)
            if binding.shared_irq is not None:
                selected.append((binding.shared_irq, item))
        return connections(tuple(selected))
