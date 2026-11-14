"""Finite static C declarations and IRQ connections for instance assembly.

Hardware behavior remains in ordinary C. These immutable nodes describe only
constant initialization and fixed calls; the target compiler owns C layout and
type checking. Expressions, statements and runtime initializer calls are absent.
"""
from __future__ import annotations
from dataclasses import dataclass
from typing import Union

from tools.configure.calls import Address, DirectCall, Symbol


def _sequence(values, purpose):
    if not isinstance(values, (tuple, list)):
        raise ValueError(f"{purpose} requires an ordered sequence")
    return tuple(values)


def _value(value):
    if isinstance(value, (Symbol, Address, PointerMemberAddress,
                          ArrayElementAddress, Flags, Signed32, Unsigned64,
                          ConstantMacro, Record, Array)):
        return
    if value is None or type(value) is bool:
        return
    if type(value) is int and 0 <= value <= 0xffffffff:
        return
    raise ValueError("Static initialization requires typed constants or references")


def _render(value):
    if value is None:
        return "NULL"
    if type(value) is bool:
        return "true" if value else "false"
    if type(value) is int:
        return str(value) + "u"
    return value.render()


@dataclass(frozen=True)
class PointerMemberAddress:
    """Address of one named member through a declared static pointer symbol."""

    base: Symbol
    member: str

    def __post_init__(self):
        if not isinstance(self.base, Symbol):
            raise ValueError("Static pointer member requires a Symbol base")
        Symbol(self.member)

    def render(self):
        return "&" + self.base.render() + "->" + self.member


@dataclass(frozen=True)
class ArrayElementAddress:
    """Address of one static array element at a nonnegative constant index."""

    array: Symbol
    index: int

    def __post_init__(self):
        if not isinstance(self.array, Symbol):
            raise ValueError("Static array element requires a Symbol array")
        if type(self.index) is not int or not 0 <= self.index <= 0xffffffff:
            raise ValueError("Static array element requires a uint32 index")

    def render(self):
        return "&" + self.array.render() + "[" + str(self.index) + "u]"


@dataclass(frozen=True)
class Flags:
    """A constant OR of reviewed enum symbols, without executable expressions."""

    values: tuple[Symbol, ...]

    def __post_init__(self):
        values = _sequence(self.values, "Static flags")
        if (not values or any(not isinstance(value, Symbol) for value in values)
                or len(set(values)) != len(values)):
            raise ValueError("Static flags require unique ordered enum symbols")
        object.__setattr__(self, "values", values)

    def render(self):
        return " | ".join(value.render() for value in self.values)


@dataclass(frozen=True)
class Unsigned64:
    """An explicit unsigned 64-bit literal checked before C11 rendering."""

    value: int

    def __post_init__(self):
        if type(self.value) is not int or not 0 <= self.value <= 0xffffffffffffffff:
            raise ValueError("Wide static constant requires a uint64 value")

    def render(self):
        return "UINT64_C(" + str(self.value) + ")"


@dataclass(frozen=True)
class Signed32:
    """An explicit signed 32-bit literal, including private IRQ sentinels."""

    value: int

    def __post_init__(self):
        if type(self.value) is not int or not -(2**31) <= self.value < 2**31:
            raise ValueError("Signed static constant requires an int32 value")

    def render(self):
        return "INT32_C(" + str(self.value) + ")"


@dataclass(frozen=True)
class ConstantMacro:
    """One reviewed C constant encoder applied to finite static values.

    Selected C headers own the encoding; the target compiler checks that the
    resulting initializer is constant. This node cannot contain source text,
    runtime calls, aggregate values or configuration expressions.
    """

    symbol: Symbol
    arguments: tuple[StaticValue, ...]

    def __post_init__(self):
        if not isinstance(self.symbol, Symbol):
            raise ValueError("Static macro requires a Symbol name")
        arguments = _sequence(self.arguments, "Static macro arguments")
        for value in arguments:
            _value(value)
            if isinstance(value, (Record, Array)):
                raise ValueError("Static macro requires scalar arguments")
        object.__setattr__(self, "arguments", arguments)

    def render(self):
        return self.symbol.render() + "(" + ", ".join(
            _render(value) for value in self.arguments) + ")"


@dataclass(frozen=True)
class Record:
    """Ordered designated fields; an empty record is C11 zero initialization."""

    fields: tuple[tuple[str, StaticValue], ...]

    def __post_init__(self):
        fields = []
        names = set()
        for item in _sequence(self.fields, "Record fields"):
            if not isinstance(item, (tuple, list)) or len(item) != 2:
                raise ValueError("Record fields require name/value pairs")
            name, value = item
            Symbol(name)
            if name in names:
                raise ValueError("Static record fields must be unique")
            _value(value)
            fields.append((name, value))
            names.add(name)
        object.__setattr__(self, "fields", tuple(fields))

    def render(self):
        if not self.fields:
            return "{0}"
        return "{ " + ", ".join("." + name + " = " + _render(value)
                                 for name, value in self.fields) + " }"


@dataclass(frozen=True)
class Array:
    """Nonempty fixed-order initialization; omitted tail elements remain zero."""

    values: tuple[StaticValue, ...]

    def __post_init__(self):
        values = _sequence(self.values, "Array elements")
        if not values:
            raise ValueError("Static arrays require at least one element")
        for value in values:
            _value(value)
        object.__setattr__(self, "values", values)

    def render(self):
        return "{ " + ", ".join(_render(value) for value in self.values) + " }"


StaticValue = Union[bool, int, None, Symbol, Address, PointerMemberAddress,
                    ArrayElementAddress, Flags, Signed32, Unsigned64,
                    ConstantMacro, Record, Array]


@dataclass(frozen=True)
class Declaration:
    """One static typedef-based object with optional count and C11 alignment.

    C type definitions and operations remain in the selected driver's headers.
    A missing initializer produces zero storage, without a runtime constructor.
    """

    type_name: str
    name: str
    initializer: StaticValue = None
    const: bool = False
    count: int | None = None
    alignment: int | None = None
    static: bool = True
    pointer: bool = False
    pointee_const: bool = False
    extern: bool = False

    def __post_init__(self):
        Symbol(self.type_name)
        Symbol(self.name)
        _value(self.initializer)
        if any(type(value) is not bool for value in (
                self.const, self.static, self.pointer, self.pointee_const,
                self.extern)):
            raise ValueError("Static declaration qualifiers must be boolean")
        if self.pointee_const and not self.pointer:
            raise ValueError("Pointee const requires a pointer declaration")
        if self.extern and (self.static or self.initializer is not None or
                            self.alignment is not None):
            raise ValueError("External declaration has no definition or storage")
        if self.count is not None:
            if type(self.count) is not int or not 0 < self.count <= 0xffffffff:
                raise ValueError("Static declaration count must be positive uint32")
            if isinstance(self.initializer, Array):
                if len(self.initializer.values) > self.count:
                    raise ValueError("Static array initializer exceeds its count")
            elif self.initializer is not None:
                raise ValueError("Static array requires an Array initializer")
        elif isinstance(self.initializer, Array):
            raise ValueError("Static array declaration requires an explicit count")
        if self.alignment is not None and (
                type(self.alignment) is not int or
                not 0 < self.alignment <= 0x80000000 or
                self.alignment & (self.alignment - 1)):
            raise ValueError("Static alignment must be a positive power of two")

    def render(self):
        alignment = (f"_Alignas({self.alignment}) "
                     if self.alignment is not None else "")
        storage = "extern " if self.extern else "static " if self.static else ""
        if self.pointer:
            type_name = (("const " if self.pointee_const else "") +
                         self.type_name + "*" + (" const" if self.const else ""))
        else:
            type_name = ("const " if self.const else "") + self.type_name
        count = f"[{self.count}]" if self.count is not None else ""
        if self.extern:
            return [f"extern {type_name} {self.name}{count};"]
        initializer = ("{0}" if self.initializer is None
                       else _render(self.initializer))
        return [f"{alignment}{storage}{type_name} {self.name}{count}"
                f" = {initializer};"]


@dataclass(frozen=True)
class IrqConnection:
    """A fixed external IRQ entry with an explicit final architectural barrier."""

    name: str
    calls: tuple[DirectCall, ...]
    exit_barrier: Symbol | None = None

    def __post_init__(self):
        Symbol(self.name)
        calls = _sequence(self.calls, "IRQ connections")
        if not calls or any(not isinstance(call, DirectCall) for call in calls):
            raise ValueError("IRQ connection requires nonempty fixed direct calls")
        if self.exit_barrier is not None and not isinstance(
                self.exit_barrier, Symbol):
            raise ValueError("IRQ exit barrier requires an explicit C symbol")
        object.__setattr__(self, "calls", calls)

    def render(self):
        source = [r"/** \brief Fixed static IRQ connection. */",
                  f"void {self.name}(void) {{"]
        source += ["    " + call.render() + ";" for call in self.calls]
        if self.exit_barrier is not None:
            source.append("    " + self.exit_barrier.render() + "();")
        return source + ["}"]


@dataclass(frozen=True)
class ControllerPowerConnection:
    """Fixed suspend/resume overrides for one already assembled controller.

    The generic emitter owns indexed dispatch and acquisition-result syntax.
    Concrete C calls own all retention, drain and hardware behavior.
    """

    index: int
    suspend: DirectCall
    resume: DirectCall
    retain_on_failed_resume: bool

    def __post_init__(self):
        if type(self.index) is not int or not 0 <= self.index <= 0xffffffff:
            raise ValueError("Controller power connection requires a uint32 index")
        if not isinstance(self.suspend, DirectCall) or not isinstance(
                self.resume, DirectCall):
            raise ValueError("Controller power connection requires direct C calls")
        if type(self.retain_on_failed_resume) is not bool:
            raise ValueError("Controller power retention requires an explicit bool")
