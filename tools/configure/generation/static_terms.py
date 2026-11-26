"""Parse bounded static values, object declarations and direct C calls."""
from collections.abc import Mapping
from dataclasses import dataclass, fields, is_dataclass
import re
from types import MappingProxyType

from tools.configure.generation.calls import Address, DirectCall, Symbol
from tools.configure.generation.static_ir import (Array, ArrayElementAddress,
    ConstantMacro, Declaration, Flags, PointerMemberAddress, Record, Signed32,
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
