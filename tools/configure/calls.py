"""Fixed C calls and static references, never arbitrary C expression fragments.

These immutable build-time values describe selected direct calls only. They do
not add firmware metadata, dispatch tables, runtime lookup or a C interpreter.
The C compiler still establishes actual declarations and parameter types.
"""
from dataclasses import dataclass
import re


_IDENTIFIER = r"[A-Za-z_][A-Za-z0-9_]*"
C_KEYWORDS = frozenset("""
auto break case char const continue default do double else enum extern float
for goto if inline int long register restrict return short signed sizeof static
struct switch typedef union unsigned void volatile while _Alignas _Alignof
_Atomic _Bool _Complex _Generic _Imaginary _Noreturn _Static_assert _Thread_local
""".split())


def _name(value, pattern, purpose):
    if (not isinstance(value, str) or re.fullmatch(pattern, value) is None or
            any(part in C_KEYWORDS for part in value.split("."))):
        raise ValueError(f"{purpose} requires an explicit C identifier")


@dataclass(frozen=True)
class Symbol:
    """A declared static array, object or enum name used without dereference."""

    name: str

    def __post_init__(self):
        _name(self.name, _IDENTIFIER, "Static symbol")

    def render(self):
        return self.name


@dataclass(frozen=True)
class Address:
    """Address of one static object or its fixed member path."""

    name: str

    def __post_init__(self):
        _name(self.name, _IDENTIFIER + r"(?:\." + _IDENTIFIER + r")*",
              "Static object address")

    def render(self):
        return "&" + self.name


@dataclass(frozen=True)
class DirectCall:
    """Call an explicit C symbol with references, constants or direct calls.

    Integers are unsigned32 values; bool and None emit true/false and NULL.
    A nested call preserves explicit runtime argument evaluation, such as
    forming an absolute deadline at startup. No branch, assignment, cast,
    operator, string literal or function-pointer expression is accepted.
    """

    function: str
    arguments: tuple = ()

    def __post_init__(self):
        _name(self.function, _IDENTIFIER, "Direct call")
        if not isinstance(self.arguments, (tuple, list)):
            raise ValueError("Direct call requires an ordered argument sequence")
        arguments = tuple(self.arguments)
        for argument in arguments:
            if isinstance(argument, (Address, Symbol, DirectCall)):
                continue
            if argument is None or type(argument) is bool:
                continue
            if type(argument) is int and 0 <= argument <= 0xffffffff:
                continue
            raise ValueError("Direct call requires typed references or constants")
        object.__setattr__(self, "arguments", arguments)

    def render(self):
        arguments = []
        for value in self.arguments:
            if isinstance(value, (Address, Symbol, DirectCall)):
                arguments.append(value.render())
            elif value is None:
                arguments.append("NULL")
            elif type(value) is bool:
                arguments.append("true" if value else "false")
            else:
                arguments.append(str(value) + "u")
        return self.function + "(" + ", ".join(arguments) + ")"


@dataclass(frozen=True)
class InitializeSequence:
    """Fixed lifecycle call order with a standard NX_SUCCESS short circuit.

    This emits assembly glue only. Every hardware step remains a named direct
    call to concrete C; callers cannot supply a statement or predicate.
    """

    function: str
    calls: tuple[DirectCall, ...]

    def __post_init__(self):
        _name(self.function, _IDENTIFIER, "Initialization sequence")
        if not isinstance(self.calls, (tuple, list)) or not self.calls:
            raise ValueError("Initialization sequence requires ordered direct calls")
        calls = tuple(self.calls)
        if any(not isinstance(call, DirectCall) for call in calls):
            raise ValueError("Initialization sequence requires explicit direct calls")
        object.__setattr__(self, "calls", calls)

    def render(self):
        result = [f"static nx_result_t {self.function}(void) {{"]
        if len(self.calls) > 1:
            result.append("    nx_result_t status;")
        for call in self.calls[:-1]:
            result += ["    status = " + call.render() + ";",
                       "    if (status != NX_SUCCESS) { return status; }"]
        return result + ["    return " + self.calls[-1].render() + ";", "}"]
