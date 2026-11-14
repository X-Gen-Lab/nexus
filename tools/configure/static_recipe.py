"""Project finite provider recipes into immutable static assembly nodes.

Recipes connect existing typed IR, static objects and named C entry points.
They contain no hardware behavior, expressions or family-specific decisions.
Ordered maps only expand a bounded, already resolved immutable IR sequence.
"""
from collections.abc import Mapping
from dataclasses import dataclass, fields, is_dataclass, replace
import re
from types import MappingProxyType

from tools.configure.calls import Address, DirectCall, InitializeSequence, Symbol
from tools.configure.providers.common import Construction
from tools.configure.static_ir import (Array, ArrayElementAddress, ConstantMacro,
    Declaration, Flags, IrqConnection, PointerMemberAddress, Record, Signed32,
    Unsigned64)


_MISSING = object()
_STEP = r"(?:[a-z][a-z0-9_]*|0|[1-9][0-9]*)"
_PATH = r"(?:item|selected|selected_children)(?:\." + _STEP + r")*"
_LOCAL_PATH = r"(?:entry(?:\." + _STEP + r")*|index)"
_CONVERSIONS = {"value", "u32", "u64", "s32", "bool", "symbol", "address", "static"}


def _fields(value, required, optional=()):
    if (not isinstance(value, Mapping) or not set(required).issubset(value) or
            set(value) - set(required) - set(optional)):
        raise ValueError("Static recipe has missing or unknown fields")


def _ordered(value, purpose):
    if not isinstance(value, (tuple, list)):
        raise ValueError(purpose + " requires an ordered sequence")
    return tuple(value)


def _uint(value, purpose, minimum=0):
    if type(value) is not int or not minimum <= value <= 0xffffffff:
        raise ValueError(purpose + " requires a uint32 constant")
    return value


def _path(value, local=False):
    pattern = "(?:" + _PATH + "|" + _LOCAL_PATH + ")" if local else _PATH
    if (not isinstance(value, str) or re.fullmatch(pattern, value) is None or
            "__" in value):
        raise ValueError("Static source requires a named typed IR path")
    return value


def _immutable(value):
    if isinstance(value, MappingProxyType):
        return
    if isinstance(value, tuple):
        return
    if (not isinstance(value, type) and is_dataclass(value) and
            value.__dataclass_params__.frozen):
        return
    raise ValueError("Static source requires immutable typed IR")


def _read(path, context):
    names = path.split(".")
    value = context.get(names[0], _MISSING)
    for name in names[1:]:
        if value is None or value is _MISSING:
            return _MISSING
        _immutable(value)
        if name.isdecimal():
            if not isinstance(value, tuple):
                raise ValueError("Static source index requires an IR tuple")
            if int(name) >= len(value):
                return _MISSING
            value = value[int(name)]
        elif isinstance(value, MappingProxyType):
            value = value.get(name, _MISSING)
        elif is_dataclass(value):
            if name not in {field.name for field in fields(value)}:
                return _MISSING
            value = getattr(value, name)
        else:
            raise ValueError("Static source cannot traverse an unnamed IR value")
    if isinstance(value, (Mapping, list, set, frozenset)):
        _immutable(value)
    return value


def _convert(value, conversion):
    if conversion == "s32":
        return Signed32(value)
    if conversion == "u64":
        return Unsigned64(value)
    if conversion == "static":
        Record((("value", value),))
        return value
    if conversion == "symbol":
        return Symbol(value)
    if conversion == "address":
        return Address(value)
    if conversion == "u32":
        return _uint(value, "Static source")
    if conversion == "bool":
        if type(value) is not bool:
            raise ValueError("Static source requires a boolean")
        return value
    if value is None or type(value) is bool:
        return value
    return _uint(value, "Static source")


def _active(node, context, roles):
    if node is None:
        return True
    value = node.resolve(context, roles)
    if type(value) is not bool:
        raise ValueError("Static selection requires a typed boolean")
    return value


@dataclass(frozen=True)
class _Literal:
    value: object

    def resolve(self, context, roles):
        return self.value


@dataclass(frozen=True)
class _Source:
    path: str
    conversion: str
    choices: tuple
    default: object = _MISSING

    def resolve(self, context, roles):
        value = _read(self.path, context)
        if (value is _MISSING or value is None) and self.default is not _MISSING:
            return self.default.resolve(context, roles)
        if value is _MISSING:
            raise ValueError("Missing typed static source: " + self.path)
        if self.choices:
            for name, choice in self.choices:
                if type(value) is str and value == name:
                    return choice.resolve(context, roles)
            raise ValueError("Unsupported static source enum: " + self.path)
        return _convert(value, self.conversion)


@dataclass(frozen=True)
class _Reference:
    role: str
    conversion: str
    member: str | None
    index: object

    def resolve(self, context, roles):
        if self.role not in roles:
            raise ValueError("Static reference names an unselected object role")
        name = roles[self.role]
        if self.index is not None:
            index = _uint(self.index.resolve(context, roles), "Static array index")
            if self.role not in context["counts"] or index >= context["counts"][self.role]:
                raise ValueError("Static array reference exceeds its declared storage")
            return ArrayElementAddress(Symbol(name), index)
        if self.member is not None:
            return Address(name + "." + self.member)
        return Address(name) if self.conversion == "address" else Symbol(name)


@dataclass(frozen=True)
class _Owner:
    source: str
    role: str
    conversion: str
    member: str | None = None

    def resolve(self, context, roles):
        identifier = _read(self.source, context)
        if identifier is None:
            return None
        if (not isinstance(identifier, str) or
                identifier not in context["owners"] or
                re.fullmatch(r"[a-z][a-z0-9_-]*", identifier) is None):
            raise ValueError("Static owner reference requires a selected owner identity")
        name = "s_nx_" + self.role + "_" + identifier.replace("-", "_")
        if self.member is not None:
            name += "." + self.member
        return Address(name) if self.conversion == "address" else Symbol(name)


@dataclass(frozen=True)
class _Record:
    fields: tuple

    def resolve(self, context, roles):
        return Record(tuple((name, value.resolve(context, roles))
                            for name, value in self.fields))


@dataclass(frozen=True)
class _Array:
    values: tuple

    def resolve(self, context, roles):
        return Array(tuple(value.resolve(context, roles) for value in self.values))


@dataclass(frozen=True)
class _Macro:
    function: str
    arguments: tuple

    def resolve(self, context, roles):
        return ConstantMacro(Symbol(self.function), tuple(
            value.resolve(context, roles) for value in self.arguments))


@dataclass(frozen=True)
class _PointerMember:
    base: object
    member: object

    def resolve(self, context, roles):
        member = self.member.resolve(context, roles)
        if not isinstance(member, Symbol):
            raise ValueError("Static pointer member requires a selected symbol")
        return PointerMemberAddress(self.base.resolve(context, roles), member.name)


@dataclass(frozen=True)
class _Optional:
    path: str
    value: object
    default: object

    def resolve(self, context, roles):
        selected = _read(self.path, context)
        node = self.default if selected is None or selected is _MISSING else self.value
        return node.resolve(context, roles)


@dataclass(frozen=True)
class _Flags:
    value: object

    def resolve(self, context, roles):
        values = self.value.resolve(context, roles)
        if not isinstance(values, Array):
            raise ValueError("Static flags require an ordered constant array")
        return Flags(values.values)


def _sequence(path, limit, context):
    value = _read(path, context)
    if not isinstance(value, tuple) or not 0 < len(value) <= limit:
        raise ValueError("Static mapping requires a nonempty bounded IR tuple")
    for entry in value:
        if type(entry) is str:
            continue
        if entry is None or type(entry) in {bool, int}:
            Record((("entry", entry),))
        else:
            _immutable(entry)
    return value


@dataclass(frozen=True)
class _Length:
    path: str
    limit: int

    def resolve(self, context, roles):
        return len(_sequence(self.path, self.limit, context))


@dataclass(frozen=True)
class _Map:
    path: str
    limit: int
    value: object

    def resolve(self, context, roles):
        return Array(tuple(self.value.resolve(
            {**context, "entry": entry, "index": index}, roles)
            for index, entry in enumerate(_sequence(self.path, self.limit, context))))


def _parse(value, roles, *, local=False, conversion="value", depth=0):
    if depth > 32:
        raise ValueError("Static recipe exceeds its finite nesting bound")
    if not isinstance(value, Mapping):
        return _Literal(_convert(value, conversion))
    child = lambda item, **options: _parse(
        item, roles, local=local, depth=depth + 1, **options)
    if "source" in value:
        _fields(value, {"source"}, {"as", "values", "default"})
        selected_conversion = value.get("as", conversion)
        if (not isinstance(selected_conversion, str) or
                selected_conversion not in _CONVERSIONS):
            raise ValueError("Static source conversion is not maintained")
        choices = value.get("values", {})
        if (not isinstance(choices, Mapping) or
                any(type(name) is not str for name in choices)):
            raise ValueError("Static source enum requires named choices")
        return _Source(_path(value["source"], local), selected_conversion,
            tuple((name, child(choice, conversion=selected_conversion))
                  for name, choice in choices.items()),
            child(value["default"], conversion=selected_conversion)
            if "default" in value else _MISSING)
    if "optional" in value:
        _fields(value, {"optional", "value", "default"})
        return _Optional(_path(value["optional"], local), child(value["value"]),
                         child(value["default"]))
    if "flags" in value:
        _fields(value, {"flags"})
        return _Flags(child(value["flags"]))
    if "ref" in value:
        _fields(value, {"ref", "as"}, {"member", "index"})
        role, mode = value["ref"], value["as"]
        if not isinstance(role, str) or role not in roles:
            raise ValueError("Static reference requires a declared role")
        if not isinstance(mode, str) or mode not in {"symbol", "address"}:
            raise ValueError("Static reference requires symbol or address")
        member, index = value.get("member"), value.get("index")
        if member is not None:
            Address("object." + member)
        if (member is not None or index is not None) and mode != "address":
            raise ValueError("Static member or element reference requires address")
        if member is not None and index is not None:
            raise ValueError("Static reference selects a member or an array element")
        if isinstance(index, Mapping):
            _fields(index, {"source", "as"})
            if index["as"] != "u32":
                raise ValueError("Static array index requires a direct typed uint32 source")
            _path(index["source"], local)
            index = child(index)
        elif index is not None:
            index = _Literal(_uint(index, "Static array index"))
        return _Reference(role, mode, member, index)
    if "record" in value:
        _fields(value, {"record"})
        record = []
        for pair in _ordered(value["record"], "Static record"):
            if not isinstance(pair, (tuple, list)) or len(pair) != 2:
                raise ValueError("Static record requires named ordered field pairs")
            Symbol(pair[0])
            if pair[0] in {name for name, entry in record}:
                raise ValueError("Static record fields must be unique")
            record.append((pair[0], child(pair[1])))
        return _Record(tuple(record))
    if "array" in value:
        _fields(value, {"array"})
        values = _ordered(value["array"], "Static array")
        if not values:
            raise ValueError("Static array requires at least one value")
        return _Array(tuple(child(entry) for entry in values))
    if "symbol" in value:
        _fields(value, {"symbol"})
        return _Literal(Symbol(value["symbol"]))
    if "s32" in value:
        _fields(value, {"s32"})
        return _Literal(Signed32(value["s32"]))
    if "address" in value:
        _fields(value, {"address"})
        return _Literal(Address(value["address"]))
    if "owner" in value:
        _fields(value, {"owner"})
        reference = value["owner"]
        _fields(reference, {"source", "role"}, {"as", "member"})
        Symbol(reference["role"])
        mode = reference.get("as", "address")
        if mode not in {"address", "symbol"}:
            raise ValueError("Static owner reference requires address or symbol")
        member = reference.get("member")
        if member is not None:
            if not isinstance(member, str) or mode != "address":
                raise ValueError("Static owner member requires an address path")
            Address("validation." + member)
        return _Owner(_path(reference["source"], local), reference["role"], mode, member)
    if "pointer_member" in value:
        _fields(value, {"pointer_member"})
        reference = value["pointer_member"]
        _fields(reference, {"base", "member"})
        member = (_parse(reference["member"], roles, local=local,
                         depth=depth + 1, conversion="symbol")
                  if isinstance(reference["member"], Mapping)
                  else _Literal(Symbol(reference["member"])))
        return _PointerMember(child(reference["base"], conversion="symbol"),
                              member)
    if "macro" in value:
        _fields(value, {"macro", "args"})
        Symbol(value["macro"])
        return _Macro(value["macro"], tuple(child(argument) for argument in
            _ordered(value["args"], "Static macro arguments")))
    if "length" in value:
        _fields(value, {"length", "limit"})
        return _Length(_path(value["length"], local),
                       _uint(value["limit"], "Static sequence bound", 1))
    if "map" in value:
        _fields(value, {"map"})
        if local:
            raise ValueError("Static maps do not contain nested iteration")
        mapping = value["map"]
        _fields(mapping, {"source", "limit", "value"})
        return _Map(_path(mapping["source"]),
            _uint(mapping["limit"], "Static sequence bound", 1),
            _parse(mapping["value"], roles, local=True, depth=depth + 1))
    raise ValueError("Static recipe requires a finite named value form")


@dataclass(frozen=True)
class _Call:
    function: object
    arguments: tuple

    @classmethod
    def parse(cls, value, roles, *, local=False):
        _fields(value, {"function", "args"})
        function = (_parse(value["function"], roles, local=local, conversion="symbol")
                    if isinstance(value["function"], Mapping)
                    else _Literal(Symbol(value["function"])))
        return cls(function, tuple(_parse(item, roles, local=local) for item in
            _ordered(value["args"], "Static call arguments")))

    def resolve(self, context, roles):
        function = self.function.resolve(context, roles)
        if not isinstance(function, Symbol):
            raise ValueError("Static call target requires a selected symbol")
        return DirectCall(function.name, tuple(value.resolve(context, roles)
                                              for value in self.arguments))


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
class _Declaration:
    role: str
    type_name: str
    const: bool
    value: object
    count: object
    alignment: int | None
    pointer: bool
    static: bool
    pointee_const: bool
    extern: bool
    enabled: object | None
    symbol: str | None

    @classmethod
    def parse(cls, value, roles):
        _fields(value, {"role", "type", "const"}, {
            "value", "count", "alignment", "pointer", "static",
            "pointee_const", "extern", "enabled", "symbol"})
        Symbol(value["type"])
        if type(value["const"]) is not bool:
            raise ValueError("Static declaration const must be boolean")
        alignment = value.get("alignment")
        pointer = value.get("pointer", False)
        static = value.get("static", True)
        pointee_const = value.get("pointee_const", False)
        extern = value.get("extern", False)
        name = value.get("symbol")
        if name is not None:
            Symbol(name)
        Declaration(value["type"], "validation", alignment=alignment,
                    pointer=pointer, static=static, pointee_const=pointee_const,
                    extern=extern)
        if extern and "value" in value:
            raise ValueError("External static object has no initializer")
        return cls(value["role"], value["type"], value["const"],
            _parse(value["value"], roles) if "value" in value else None,
            _parse(value["count"], roles) if "count" in value else None,
            alignment, pointer, static, pointee_const, extern,
            _parse(value["enabled"], roles) if "enabled" in value else None, name)

    def object_name(self, identifier):
        """Use a fixed symbol or the one private/global object convention."""
        return self.symbol or ("s_nx_" if self.static else "g_nx_") + self.role + "_" + identifier

    def active(self, context, roles):
        return _active(self.enabled, context, roles)

    def resolve(self, context, roles):
        if self.symbol is not None and roles[self.role] != self.symbol:
            raise ValueError("Static declaration symbol differs from its role binding")
        return Declaration(self.type_name, roles[self.role],
            self.value.resolve(context, roles) if self.value is not None else None,
            const=self.const,
            count=self.count.resolve(context, roles) if self.count is not None else None,
            alignment=self.alignment, pointer=self.pointer, static=self.static,
            pointee_const=self.pointee_const, extern=self.extern)


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
    endpoint_faces: object | None
    child_groups: object | None
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
        from tools.configure.static_endpoints import ChildGroups, EndpointFaces
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
