"""Project finite provider recipes into immutable static assembly nodes.

Recipes connect existing typed IR, static objects and named C entry points.
They contain no hardware behavior, expressions or family-specific decisions.
Ordered maps only expand a bounded, already resolved immutable IR sequence.
"""
from collections.abc import Mapping
from dataclasses import dataclass, replace
import re
from types import MappingProxyType

from tools.configure.generation.calls import DirectCall, InitializeSequence, Symbol
from tools.configure.generation.static_endpoints import ChildGroups, EndpointFaces
from tools.configure.generation.static_ir import IrqConnection
from tools.configure.generation.static_terms import (
    _Call, _Declaration, _Literal, _active, _fields, _immutable, _ordered, _parse,
    _path, _read, _sequence, _uint)
from tools.configure.providers.common import Construction


@dataclass(frozen=True)
class _PrepareSequence:
    role: str
    calls: tuple

    @classmethod
    def parse(cls, value, roles):
        _fields(value, {"role", "sequence"})
        Symbol(value["role"])
        calls = tuple(_Call.parse(call, roles) for call in
                      _ordered(value["sequence"], "Static prepare sequence"))
        if not calls:
            raise ValueError("Static prepare sequence requires fixed calls")
        return cls(value["role"], calls)

    def resolve(self, context, roles):
        function = "nx_" + self.role + "_" + context["identifier"]
        return InitializeSequence(function, tuple(call.resolve(context, roles)
                                                   for call in self.calls))


@dataclass(frozen=True)
class _Irq:
    name: object
    calls: tuple
    barrier: str | None
    enabled: object | None

    @classmethod
    def parse(cls, value, roles, *, local=False):
        _fields(value, {"name", "calls", "barrier"}, {"enabled"})
        name = (_parse(value["name"], roles, conversion="symbol", local=local)
                if isinstance(value["name"], Mapping)
                else _Literal(Symbol(value["name"])))
        calls = tuple(_Call.parse(call, roles, local=local) for call in
                      _ordered(value["calls"], "Static IRQ calls"))
        if not calls:
            raise ValueError("Static IRQ requires explicit calls")
        barrier = value["barrier"]
        if barrier is not None:
            Symbol(barrier)
        return cls(name, calls, barrier,
                   _parse(value["enabled"], roles, local=local)
                   if "enabled" in value else None)

    def resolve(self, context, roles):
        if not _active(self.enabled, context, roles):
            return None
        name = self.name.resolve(context, roles)
        if not isinstance(name, Symbol):
            raise ValueError("Static IRQ requires a symbol name")
        return IrqConnection(name.name, tuple(call.resolve(context, roles)
                                             for call in self.calls),
                             Symbol(self.barrier) if self.barrier else None)


@dataclass(frozen=True)
class _IrqMap:
    source: str
    limit: int
    value: _Irq

    @classmethod
    def parse(cls, value, roles):
        _fields(value, {"map"})
        mapping = value["map"]
        _fields(mapping, {"source", "limit", "value"})
        return cls(_path(mapping["source"]),
                   _uint(mapping["limit"], "Static IRQ map bound", 1),
                   _Irq.parse(mapping["value"], roles, local=True))

    def resolve(self, context, roles):
        values = (self.value.resolve({**context, "entry": entry, "index": index}, roles)
                  for index, entry in enumerate(_sequence(self.source, self.limit, context)))
        return tuple(value for value in values if value is not None)


@dataclass(frozen=True)
class _PinSnapshots:
    source: str
    key: str
    role: str
    limit: int

    @classmethod
    def parse(cls, value, roles):
        _fields(value, {"source", "key", "array_role", "limit"})
        if value["array_role"] not in roles - {"port"}:
            raise ValueError("Pin snapshots require declared static array storage")
        key = _path(value["key"], local=True)
        if not key.startswith("entry."):
            raise ValueError("Pin snapshot keys require a named mapped entry")
        return cls(_path(value["source"]), key, value["array_role"],
                   _uint(value["limit"], "Pin snapshot bound", 1))

    def resolve(self, context, roles):
        entries = _read(self.source, context)
        if not isinstance(entries, tuple) or len(entries) > self.limit:
            raise ValueError("Pin snapshots require a bounded immutable pad tuple")
        if not entries and self.role not in roles and self.role not in context["counts"]:
            return MappingProxyType({})
        if context["counts"].get(self.role) != len(entries):
            raise ValueError("Pin snapshot storage must match the selected pads")
        result = {}
        for index, entry in enumerate(entries):
            key = _read(self.key, {**context, "entry": entry})
            Symbol(key)
            if key in result:
                raise ValueError("Pin snapshot keys must be unique")
            result[key] = roles[self.role] + "[" + str(index) + "u]"
        return MappingProxyType(result)


@dataclass(frozen=True)
class StaticRecipe:
    """One validated static layout and its fixed lifecycle connections."""

    state_type: str
    ops: object
    declarations: tuple
    admission: tuple
    prepare: _Call | _PrepareSequence
    close: tuple
    irq: tuple
    retain_on_failed_init: bool
    initial_state: str | None
    pin_snapshots: _PinSnapshots | None
    endpoint_faces: EndpointFaces | None
    child_groups: ChildGroups | None
    face_context: object | None
    pre_c_effects: bool
    exports_from: tuple[str, ...]

    @classmethod
    def from_validated(cls, value):
        """Freeze author JSON, rejecting executable or unnamed value forms."""
        _fields(value, {"state_type", "declarations", "calls"}, {
            "ops", "irq", "retain_on_failed_init", "initial_state", "pin_snapshots",
            "endpoint_faces", "child_groups", "face_context", "pre_c_effects",
            "exports_from"})
        Symbol(value["state_type"])
        if value.get("initial_state") is not None:
            Symbol(value["initial_state"])
        declarations = _ordered(value["declarations"], "Static declarations")
        roles = {"port", "face"}
        for declaration in declarations:
            _fields(declaration, {"role", "type", "const"},
                    {"value", "count", "alignment", "pointer", "static",
                     "pointee_const", "extern", "enabled", "symbol"})
            role = declaration["role"]
            Symbol(role)
            if role in roles:
                raise ValueError("Static declaration roles must be unique")
            roles.add(role)
        calls = value["calls"]
        _fields(calls, {"prepare", "close"}, {"admit"})
        retain = value.get("retain_on_failed_init", True)
        if type(retain) is not bool:
            raise ValueError("Static failed-init responsibility must be boolean")
        pre_c = value.get("pre_c_effects", False)
        if type(pre_c) is not bool:
            raise ValueError("Static pre-C effects require an explicit boolean")
        exports = _ordered(value.get("exports_from", ()), "Static exports")
        if (any(not isinstance(role, str) or role not in roles - {"port", "face"}
                for role in exports) or len(set(exports)) != len(exports)):
            raise ValueError("Static exports require distinct declared object roles")
        return cls(value["state_type"], _parse(value["ops"], (), conversion="symbol")
            if "ops" in value else None,
            tuple(_Declaration.parse(declaration, roles) for declaration in declarations),
            tuple(_Call.parse(call, roles) for call in
                  _ordered(calls.get("admit", ()), "Static admission calls")),
            (_PrepareSequence.parse(calls["prepare"], roles)
             if isinstance(calls["prepare"], Mapping) and "sequence" in calls["prepare"]
             else _Call.parse(calls["prepare"], roles)),
            tuple(_Call.parse(call, roles) for call in
                  _ordered(calls["close"], "Static close calls")),
            tuple((_IrqMap.parse(irq, roles) if isinstance(irq, Mapping) and "map" in irq
                   else _Irq.parse(irq, roles)) for irq in
                  _ordered(value.get("irq", ()), "Static IRQ connections")),
            retain, value.get("initial_state"),
            _PinSnapshots.parse(value["pin_snapshots"], roles)
            if "pin_snapshots" in value else None,
            EndpointFaces.from_validated(value["endpoint_faces"])
            if "endpoint_faces" in value else None,
            ChildGroups.from_validated(value["child_groups"])
            if "child_groups" in value else None,
            _parse(value["face_context"], roles)
            if "face_context" in value else None, pre_c, exports)

    def operations(self, item):
        """Select the existing immutable table name, without firmware lookup."""
        _immutable(item)
        if self.ops is None:
            raise ValueError("Static device recipe requires its operations symbol")
        value = self.ops.resolve({"item": item,
                                  "selected": getattr(item, "integration", None)}, {})
        if not isinstance(value, Symbol):
            raise ValueError("Static operations require a selected symbol")
        return value.name

    def construct(self, item, result, board_bindings, routes, devices):
        """Project the selected immutable owner; never execute driver behavior."""
        _immutable(item)
        identifier = getattr(item, "id", None)
        if (not isinstance(identifier, str) or
                re.fullmatch(r"[a-z][a-z0-9_-]*", identifier) is None):
            raise ValueError("Static owner requires an ordinary instance identifier")
        if not isinstance(devices, tuple):
            raise ValueError("Selected children require an immutable IR tuple")
        children = tuple(child for child in devices if child.controller == item.id)
        for child in children:
            _immutable(child)
        identifier = identifier.replace("-", "_")
        roles = {role: "s_nx_" + role + "_" + identifier
                 for role in ("port", "face")}
        roles.update({entry.role: entry.object_name(identifier)
                      for entry in self.declarations})
        if len(set(roles.values())) != len(roles):
            raise ValueError("Static object symbols must be distinct")
        context = {"item": item, "selected": getattr(item, "integration", None),
                   "selected_children": children, "identifier": identifier.replace("-", "_")}
        if result is not None:
            _immutable(result)
            owners = getattr(result, "controllers", ())
            if not isinstance(owners, tuple):
                raise ValueError("Static owner selection requires an immutable IR tuple")
            context["owners"] = tuple(owner.id for owner in owners)
        else:
            context["owners"] = ()
        context["owners"] += tuple(child.id for child in children)
        if len(set(context["owners"])) != len(context["owners"]):
            raise ValueError("Selected owner identities must be distinct")
        context["counts"] = {}
        active = tuple(value for value in self.declarations
                       if value.active(context, roles))
        inactive = {value.role for value in self.declarations if value not in active}
        roles = {role: name for role, name in roles.items() if role not in inactive}
        context["counts"] = {
            value.role: _uint(value.count.resolve(context, roles), "Static count", 1)
            for value in active if value.count is not None}
        declarations = tuple(value.resolve(context, roles) for value in active)
        if self.endpoint_faces is not None:
            declarations += self.endpoint_faces.project(
                children, roles, context["selected"], parent_counts=context["counts"])
        child_prepare, child_close = (), ()
        if self.child_groups is not None:
            projected = self.child_groups.project(
                children, roles, context["selected"], parent_counts=context["counts"])
            before = self.child_groups.before_role
            if before is None:
                declarations += projected.declarations
            else:
                authored = tuple(value.role for value in self.declarations)
                if before not in authored:
                    raise ValueError("Child declaration anchor is not a parent role")
                prefix = set(authored[:authored.index(before)])
                position = sum(value.role in prefix for value in active)
                declarations = (declarations[:position] + projected.declarations +
                                declarations[position:])
            child_prepare, child_close = projected.prepare, projected.close
        if len({entry.name for entry in declarations}) != len(declarations):
            raise ValueError("Static declaration symbols must be distinct")
        irq = tuple(entry for value in self.irq for entry in (
            value.resolve(context, roles) if isinstance(value, _IrqMap)
            else (value.resolve(context, roles),)) if entry is not None)
        if len({value.name for value in irq}) != len(irq):
            raise ValueError("Static IRQ connection names must be unique")
        prepare = self.prepare.resolve(context, roles)
        if child_prepare:
            name = (prepare.function if isinstance(prepare, InitializeSequence)
                    else "nx_prepare_children_" + identifier)
            calls = (prepare.calls if isinstance(prepare, InitializeSequence)
                     else (prepare,))
            prepare = InitializeSequence(name, calls + child_prepare)
        sequences = (prepare,) if isinstance(prepare, InitializeSequence) else ()
        initialize = DirectCall(prepare.function) if sequences else prepare
        exports = []
        for value in active:
            if value.role in self.exports_from:
                declaration = value.resolve(context, roles)
                if declaration.static or declaration.extern:
                    raise ValueError("Static export requires an actual global definition")
                exports.extend(replace(declaration, initializer=None,
                    alignment=None, static=False, extern=True).render())
        return Construction(self.state_type, (), initialize,
            child_close + tuple(call.resolve(context, roles) for call in self.close),
            self.retain_on_failed_init,
            admission=tuple(call.resolve(context, roles) for call in self.admission),
            initialize_sequences=sequences,
            pin_snapshots=self.pin_snapshots.resolve(context, roles)
            if self.pin_snapshots else MappingProxyType({}),
            initial_state=Symbol(self.initial_state) if self.initial_state else None,
            face_context=self.face_context.resolve(context, roles)
            if self.face_context is not None else None,
            pre_c_effects=self.pre_c_effects,
            exports=exports,
            static_declarations=declarations, irq_connections=irq)


@dataclass(frozen=True)
class StaticObjects:
    """A package-owned group of fixed shared objects and direct call sequences.

    It uses the same finite values as device recipes. The selected context is a
    dedicated immutable binding record, with no configuration engine or provider
    object exposed. An absent optional owner emits no storage or calls.
    """

    symbols: MappingProxyType
    declarations: tuple
    sequences: tuple

    @classmethod
    def from_validated(cls, value):
        _fields(value, {"symbols", "declarations"}, {"sequences"})
        symbols = value["symbols"]
        if not isinstance(symbols, Mapping) or not symbols:
            raise ValueError("Static objects require explicit named symbols")
        for role, name in symbols.items():
            Symbol(role)
            Symbol(name)
        if len(set(symbols.values())) != len(symbols):
            raise ValueError("Static object symbols must be distinct")
        roles = set(symbols)
        declarations = tuple(_Declaration.parse(item, roles) for item in
            _ordered(value["declarations"], "Static object declarations"))
        sequences = tuple(_PrepareSequence.parse(item, roles) for item in
            _ordered(value.get("sequences", ()), "Static object sequences"))
        definitions = tuple(item.role for item in (*declarations, *sequences))
        if (not definitions or len(set(definitions)) != len(definitions) or
                set(definitions) - roles):
            raise ValueError("Static object definitions require distinct named roles")
        return cls(MappingProxyType(dict(symbols)), declarations, sequences)

    def project(self, selected):
        """Return finite declarations; hardware behavior stays in concrete C."""
        if selected is None:
            return ()
        _immutable(selected)
        context = {"selected": selected, "counts": {}}
        active = tuple(item for item in self.declarations
                       if item.active(context, self.symbols))
        inactive = {item.role for item in self.declarations if item not in active}
        roles = {role: name for role, name in self.symbols.items()
                 if role not in inactive}
        context["counts"] = {item.role:
            _uint(item.count.resolve(context, roles), "Static count", 1)
            for item in active if item.count is not None}
        declarations = tuple(item.resolve(context, roles) for item in active)
        sequences = tuple(InitializeSequence(roles[item.role],
            tuple(call.resolve(context, roles) for call in item.calls))
            for item in self.sequences)
        return declarations + sequences
