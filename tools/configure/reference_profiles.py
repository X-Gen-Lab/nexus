"""Independent, immutable source-review profiles from one explicit package.

Document hashes, applicability tables and reviewed modes are declarative facts.
This module validates their shape and relationships; it contains no vendor,
device, manual or errata identifiers and performs no file reads.
"""
from collections.abc import Mapping
from dataclasses import dataclass
import re
from types import MappingProxyType

from tools.configure.providers.common import fail, integer


@dataclass(frozen=True)
class ErrataProfile:
    """Reviewed source applicability, never execution or physical evidence."""
    document_id: str
    revision: str
    sha256: str
    revision_order: tuple[str, ...]
    sections: frozenset[str]
    applicability: Mapping
    devices: frozenset[str]
    reference_manual: str
    reviewed_revision_ids: tuple[int, ...]
    mode_scope: frozenset[str] | None
    backends: frozenset[str]
    clock_policies: frozenset[str]


@dataclass(frozen=True)
class DocumentProfile:
    """An independently reviewed document identity and its device coverage."""
    revision: str
    sha256: str
    devices: frozenset[str]


def _fields(value, required):
    if not isinstance(value, Mapping) or set(value) != set(required):
        fail("Reference profile requires exact named facts")


def _text(value):
    if not isinstance(value, str) or not value.strip():
        fail("Reference profile requires nonempty names")
    return value


def _names(value):
    if (not isinstance(value, (tuple, list)) or not value or
            any(not isinstance(item, str) or not item.strip() for item in value) or
            len(set(value)) != len(value)):
        fail("Reference profile requires distinct explicit names")
    return tuple(value)


def _sha(value):
    if not isinstance(value, str) or re.fullmatch(r"[0-9a-f]{64}", value) is None:
        fail("Reference profile document requires an exact SHA256 identity")
    return value


def _documents(value):
    if not isinstance(value, Mapping) or not value:
        fail("Reference document profiles require a named catalog")
    result = {}
    for name, row in value.items():
        _text(name)
        _fields(row, {"revision", "sha256", "devices"})
        result[name] = DocumentProfile(_text(row["revision"]), _sha(row["sha256"]),
                                       frozenset(_names(row["devices"])))
    return MappingProxyType(result)


@dataclass(frozen=True)
class ProfileSet:
    """One selected source-review snapshot, without any built-in device table."""
    datasheets: Mapping
    manuals: Mapping
    errata: Mapping

    @classmethod
    def from_validated(cls, value):
        _fields(value, {"schema_version", "datasheets", "manuals", "errata"})
        if type(value["schema_version"]) is not int or value["schema_version"] != 1:
            fail("Unsupported reference profile schema")
        datasheets, manuals = (_documents(value[name]) for name in ("datasheets", "manuals"))
        if not isinstance(value["errata"], Mapping) or not value["errata"]:
            fail("Reference errata profiles require a named catalog")
        profiles = {}
        for name, row in value["errata"].items():
            _text(name)
            _fields(row, ErrataProfile.__dataclass_fields__)
            revisions, sections = _names(row["revision_order"]), _names(row["sections"])
            if any(re.fullmatch(r"[0-9]+(?:\.[0-9]+)*", section) is None for section in sections):
                fail("Reference profile requires exact section numbers")
            applicability = row["applicability"]
            if (not isinstance(applicability, Mapping) or set(applicability) != set(sections) or
                    any(not isinstance(codes, str) or len(codes) != len(revisions) or
                        set(codes) - set("ANDP-") for codes in applicability.values())):
                fail("Reference profile applicability must cover every section and revision")
            devices = frozenset(_names(row["devices"]))
            manual = row["reference_manual"]
            if not isinstance(manual, str) or manual not in manuals or not devices.issubset(manuals[manual].devices):
                fail("Reference profile manual must cover its exact devices")
            identifiers = row["reviewed_revision_ids"]
            if (not isinstance(identifiers, (list, tuple)) or not identifiers or
                    any(type(item) is not int for item in identifiers) or
                    len(set(identifiers)) != len(identifiers)):
                fail("Reference profile requires distinct silicon revision IDs")
            identifiers = tuple(integer(item, 0, 0xffffffff, "Reviewed silicon revision")
                                for item in identifiers)
            scope = row["mode_scope"]
            if scope is not None:
                scope = frozenset(_names(scope))
                if any(re.fullmatch(r"[a-z][a-z0-9_]*:[a-z0-9][a-z0-9-]*", mode) is None for mode in scope):
                    fail("Reference profile mode scope requires exact kind:mode names")
            backends = frozenset(_names(row["backends"]))
            if backends - {"baremetal", "freertos", "threadx"}:
                fail("Reference profile selects an unknown production backend")
            profiles[name] = ErrataProfile(_text(row["document_id"]), _text(row["revision"]),
                _sha(row["sha256"]), revisions, frozenset(sections),
                MappingProxyType(dict(applicability)), devices, manual, identifiers,
                scope, backends, frozenset(_names(row["clock_policies"])))
        return cls(datasheets, manuals, MappingProxyType(profiles))

    def select(self, name):
        """Require an explicitly declared audit profile, with no fallback."""
        selected = self.errata.get(name) if isinstance(name, str) else None
        if selected is None:
            fail("Reference audit profile is not maintained")
        return selected

    def _document(self, document, profiles, kind):
        name = document.get("document_id")
        profile = profiles.get(name) if isinstance(name, str) else None
        if (profile is None or (document["revision"], document["sha256"]) !=
                (profile.revision, profile.sha256)):
            fail("Reference " + kind + " identity has no maintained review profile")
        return profile

    def datasheet(self, document):
        return self._document(document, self.datasheets, "datasheet")

    def manual(self, document):
        return self._document(document, self.manuals, "manual")
