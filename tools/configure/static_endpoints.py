"""Finite static endpoint faces connected to existing parent/child storage.

This is a configuration projection. It allocates no runtime object and chooses
no device protocol; validated endpoint IR and the selected recipe own those
facts. Symbol names have one fixed convention rather than author templates.
"""
from collections.abc import Mapping
from dataclasses import dataclass
import re
from types import MappingProxyType

from tools.configure.calls import Address, Symbol
from tools.configure.static_ir import Declaration, Record
from tools.configure.static_recipe import (
    _Call, _Declaration, _fields, _immutable, _ordered, _parse, _path, _read, _uint)


def _identifier(value, purpose):
    if not isinstance(value, str) or re.fullmatch(r"[a-z][a-z0-9_-]*", value) is None:
        raise ValueError(purpose + " requires an ordinary identifier")
    return value.replace("-", "_")


def _parents(value):
    if not isinstance(value, Mapping):
        raise ValueError("Endpoint parents require explicit role aliases")
    aliases = {}
    for alias, role in value.items():
        Symbol(alias)
        Symbol(role)
        if alias in {"port", "face"}:
            raise ValueError("Endpoint parent alias collides with a local role")
        aliases[alias] = role
    return MappingProxyType(aliases)


def _export(type_name, identifier, operations, context):
    if not isinstance(operations, Symbol):
        raise ValueError("Endpoint operations require an immutable symbol")
    face = "s_nx_device_" + identifier
    return (
        Declaration(type_name, face, Record((("ops", Address(operations.name)),
            ("context", context))), const=True),
        Declaration(type_name, "nx_device_" + identifier, Address(face),
            const=True, pointer=True, pointee_const=True, static=False),
    )


@dataclass(frozen=True)
class _Face:
    type_name: str
    operations: object
    context: object
    parents: Mapping

    @classmethod
    def from_validated(cls, value):
        _fields(value, {"type", "ops", "context"}, {"parents"})
        Symbol(value["type"])
        parents = _parents(value.get("parents", {}))
        roles = set(parents)
        return cls(value["type"], _parse(value["ops"], roles, conversion="symbol"),
                   _parse(value["context"], roles), parents)

    def project(self, child, children, parent_roles, selected, parent_counts):
        identifier = _identifier(child.id, "Endpoint identity")
        roles = {}
        for alias, parent in self.parents.items():
            if parent not in parent_roles:
                raise ValueError("Endpoint parent role is not declared: " + parent)
            Symbol(parent_roles[parent])
            roles[alias] = parent_roles[parent]
        context = {"item": child, "selected": selected,
                   "selected_children": children, "counts": {alias:
                       _uint(parent_counts[parent], "Parent array count", 1)
                       for alias, parent in self.parents.items() if parent in parent_counts}}
        operations = self.operations.resolve(context, roles)
        return _export(self.type_name, identifier, operations,
                       self.context.resolve(context, roles))


@dataclass(frozen=True)
class EndpointFaces:
    """One bounded set of typed endpoint exports, keyed by resolved driver."""

    limit: int
    cases: Mapping

    @classmethod
    def from_validated(cls, value):
        _fields(value, {"limit", "cases"})
        limit = _uint(value["limit"], "Endpoint count bound", 1)
        if not isinstance(value["cases"], Mapping) or not value["cases"]:
            raise ValueError("Endpoint faces require explicit driver cases")
        cases = {}
        for driver, selected in value["cases"].items():
            _identifier(driver, "Endpoint driver")
            cases[driver] = _Face.from_validated(selected)
        return cls(limit, MappingProxyType(cases))

    def project(self, children, parent_roles, parent_integration, *, parent_counts=None):
        """Emit faces only for selected immutable children; hardware stays C."""
        if not isinstance(children, tuple) or len(children) > self.limit:
            raise ValueError("Endpoint faces require a bounded immutable child tuple")
        if not isinstance(parent_roles, Mapping):
            raise ValueError("Endpoint parents require the selected role mapping")
        if parent_integration is not None:
            _immutable(parent_integration)
        parent_counts = {} if parent_counts is None else parent_counts
        if not isinstance(parent_counts, Mapping):
            raise ValueError("Parent array bounds require explicit named counts")
        declarations, used = [], set()
        for child in children:
            _immutable(child)
            identifier = _identifier(child.id, "Endpoint identity")
            if identifier in used:
                raise ValueError("Endpoint identities collide after C normalization")
            used.add(identifier)
            if child.driver not in self.cases:
                raise ValueError("Endpoint driver has no selected static face: " + child.driver)
            declarations.extend(self.cases[child.driver].project(
                child, children, parent_roles, parent_integration, parent_counts))
        return tuple(declarations)


@dataclass(frozen=True)
class _ChildSelection:
    """One immutable endpoint reference and its position in the parent array."""
    child: object
    index: int
    binding: object = None


@dataclass(frozen=True)
class ChildProjection:
    declarations: tuple
    prepare: tuple
    close: tuple


@dataclass(frozen=True)
class _ChildGroup:
    parents: Mapping
    declarations: tuple
    prepare: tuple
    close: tuple
    face: tuple | None

    @classmethod
    def from_validated(cls, value):
        _fields(value, {"declarations", "calls"}, {"parents", "face"})
        parents = _parents(value.get("parents", {}))
        declarations = _ordered(value["declarations"], "Child declarations")
        roles = set(parents)
        for declaration in declarations:
            role = declaration.get("role")
            Symbol(role)
            if role in roles:
                raise ValueError("Child declarations and parent aliases must be distinct")
            roles.add(role)
        calls = value["calls"]
        _fields(calls, {"prepare", "close"})
        face = None
        if "face" in value:
            fields = value["face"]
            _fields(fields, {"type", "ops", "context"})
            Symbol(fields["type"])
            face = (fields["type"], _parse(fields["ops"], roles, conversion="symbol"),
                    _parse(fields["context"], roles))
        return cls(parents,
            tuple(_Declaration.parse(item, roles) for item in declarations),
            tuple(_Call.parse(item, roles) for item in _ordered(calls["prepare"], "Child prepare calls")),
            tuple(_Call.parse(item, roles) for item in _ordered(calls["close"], "Child close calls")), face)

    def project(self, child, children, index, parent_roles, selected, parent_counts, binding):
        identifier = _identifier(child.id, "Endpoint identity")
        roles = {item.role: item.object_name(identifier)
                 for item in self.declarations}
        for alias, parent in self.parents.items():
            if parent not in parent_roles:
                raise ValueError("Child parent role is not declared: " + parent)
            Symbol(parent_roles[parent])
            roles[alias] = parent_roles[parent]
        context = {"item": _ChildSelection(child, index, binding), "selected": selected,
                   "selected_children": children, "counts": {alias:
                       _uint(parent_counts[parent], "Parent array count", 1)
                       for alias, parent in self.parents.items() if parent in parent_counts}}
        active = tuple(item for item in self.declarations if item.active(context, roles))
        inactive = {item.role for item in self.declarations if item not in active}
        roles = {role: name for role, name in roles.items() if role not in inactive}
        context["counts"].update({item.role:
            _uint(item.count.resolve(context, roles), "Child array count", 1)
            for item in active if item.count is not None})
        declarations = tuple(item.resolve(context, roles) for item in active)
        if self.face is not None:
            type_name, operations, face_context = self.face
            declarations += _export(type_name, identifier,
                operations.resolve(context, roles), face_context.resolve(context, roles))
        return ChildProjection(declarations,
            tuple(item.resolve(context, roles) for item in self.prepare),
            tuple(item.resolve(context, roles) for item in self.close))


@dataclass(frozen=True)
class ChildGroups:
    """Bounded endpoint-local storage and fixed calls over existing C owners."""
    limit: int
    cases: Mapping
    bindings: str | None = None
    before_role: str | None = None

    @classmethod
    def from_validated(cls, value):
        _fields(value, {"limit", "cases"}, {"bindings", "before_role"})
        limit = _uint(value["limit"], "Child group count bound", 1)
        if not isinstance(value["cases"], Mapping) or not value["cases"]:
            raise ValueError("Child groups require explicit driver cases")
        cases = {}
        for driver, group in value["cases"].items():
            _identifier(driver, "Endpoint driver")
            cases[driver] = _ChildGroup.from_validated(group)
        before = value.get("before_role")
        if before is not None:
            Symbol(before)
        return cls(limit, MappingProxyType(cases),
                   _path(value["bindings"]) if "bindings" in value else None, before)

    def project(self, children, parent_roles, parent_integration, *, parent_counts=None):
        if not isinstance(children, tuple) or len(children) > self.limit:
            raise ValueError("Child groups require a bounded immutable child tuple")
        if not isinstance(parent_roles, Mapping):
            raise ValueError("Child parents require the selected role mapping")
        if parent_integration is not None:
            _immutable(parent_integration)
        parent_counts = {} if parent_counts is None else parent_counts
        if not isinstance(parent_counts, Mapping):
            raise ValueError("Parent array bounds require explicit named counts")
        bindings = None
        if self.bindings is not None:
            bindings = _read(self.bindings, {"selected": parent_integration})
            if not isinstance(bindings, MappingProxyType):
                raise ValueError("Child metadata requires an immutable endpoint index")
        declarations, prepare, close, used = [], [], [], set()
        for index, child in enumerate(children):
            _immutable(child)
            identity = _identifier(child.id, "Endpoint identity")
            if identity in used:
                raise ValueError("Child identities collide after C normalization")
            used.add(identity)
            if child.driver not in self.cases:
                raise ValueError("Child driver has no selected static group: " + child.driver)
            binding = None
            if bindings is not None:
                if child.id not in bindings:
                    raise ValueError("Selected child lacks its exact metadata binding: " + child.id)
                binding = bindings[child.id]
                _immutable(binding)
            built = self.cases[child.driver].project(
                child, children, index, parent_roles, parent_integration,
                parent_counts, binding)
            declarations.extend(built.declarations)
            prepare.extend(built.prepare)
            close.extend(built.close)
        return ChildProjection(tuple(declarations), tuple(prepare), tuple(close))
