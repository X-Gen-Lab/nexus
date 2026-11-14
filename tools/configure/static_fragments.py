"""Single-level aliases for finite static declaration and value fragments.

There are no parameters, merges, expressions or recursive references. Expansion
only shares authored data; the existing recipe parser still owns all C layout,
role, source and constant validation in the selected instance's actual scope.
"""
from collections.abc import Mapping
from dataclasses import dataclass
import re

from tools.configure.calls import Symbol
from tools.configure.records import freeze


def _identifier(name):
    if not isinstance(name, str) or re.fullmatch(r"[a-z][a-z0-9_-]{0,63}", name) is None:
        raise ValueError("Static fragment requires an ordinary name")


def _data(value, depth=0):
    if depth > 32:
        raise ValueError("Static fragment exceeds its finite nesting bound")
    if isinstance(value, Mapping):
        if "use" in value:
            raise ValueError("Static fragments cannot reference other fragments")
        for key, child in value.items():
            if type(key) is not str:
                raise ValueError("Static fragment fields require string keys")
            _data(child, depth + 1)
    elif isinstance(value, (tuple, list)):
        for child in value:
            _data(child, depth + 1)
    elif value is not None and type(value) not in {str, int, bool}:
        raise ValueError("Static fragments contain only finite JSON values")


def _roles(value):
    result = set()
    if isinstance(value, Mapping):
        if "ref" in value:
            Symbol(value["ref"])
            result.add(value["ref"])
        for child in value.values():
            result.update(_roles(child))
    elif isinstance(value, (tuple, list)):
        for child in value:
            result.update(_roles(child))
    return result


def _copy(value):
    if isinstance(value, Mapping):
        return {key: _copy(child) for key, child in value.items()}
    if isinstance(value, (tuple, list)):
        return [_copy(child) for child in value]
    return value


@dataclass(frozen=True)
class StaticFragments:
    """Frozen named data; free role names are validated again at each use."""

    declarations: Mapping
    values: Mapping

    @classmethod
    def from_validated(cls, value):
        from tools.configure.static_recipe import _Declaration, _parse
        if not isinstance(value, Mapping) or set(value) - {"declarations", "values"}:
            raise ValueError("Static fragment library has unknown fields")
        _data(value)
        declarations, values = value.get("declarations", {}), value.get("values", {})
        if not isinstance(declarations, Mapping) or not isinstance(values, Mapping):
            raise ValueError("Static fragment libraries require named objects")
        for name, entries in declarations.items():
            _identifier(name)
            if not isinstance(entries, (tuple, list)) or not entries:
                raise ValueError("Declaration fragment requires a nonempty ordered list")
            for row in entries:
                if not isinstance(row, Mapping):
                    raise ValueError("Declaration fragment requires named fields")
                Symbol(row.get("role"))
                _Declaration.parse(row, _roles(row))
        for name, entry in values.items():
            _identifier(name)
            _parse(entry, _roles(entry))
        return cls(freeze(declarations), freeze(values))

    def expand(self, recipe):
        """Expand exact aliases once; no new hardware or mode is selected."""
        def visit(value, depth=0, *, declarations=False):
            if depth > 32:
                raise ValueError("Static recipe exceeds its finite nesting bound")
            if isinstance(value, Mapping):
                if "use" in value:
                    if set(value) != {"use"}:
                        raise ValueError("Static fragment references cannot override fields")
                    name = value["use"]
                    _identifier(name)
                    library = self.declarations if declarations else self.values
                    if name not in library:
                        raise ValueError("Unknown static fragment: " + name)
                    return _copy(library[name])
                return {key: visit(child, depth + 1, declarations=key == "declarations")
                        for key, child in value.items()}
            if isinstance(value, (tuple, list)):
                result = []
                for child in value:
                    projected = visit(child, depth + 1, declarations=declarations)
                    if declarations and isinstance(child, Mapping) and "use" in child:
                        result.extend(projected)
                    else:
                        result.append(projected)
                return result
            return value

        return visit(recipe)
