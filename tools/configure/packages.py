"""Declarative SoC package identity, facts and build/export entry points.

Loading a package never imports provider code or constructs a source build
graph. CMake owns translation units; exports describe package ownership only.
"""
from collections.abc import Mapping
from dataclasses import dataclass
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
from types import MappingProxyType

from tools.configure.providers.common import fail


MANIFEST = "package.json"
INTERFACE_VERSION = 1
_IDENTIFIER = re.compile(r"[A-Za-z][A-Za-z0-9_-]{0,63}")
_C_IDENTIFIER = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")


def _manifest_contract():
    identifier = {"type": "string", "pattern": "^" + _IDENTIFIER.pattern + "$"}
    relative = {"type": "string", "minLength": 1,
        "pattern": r'^(?!/)(?!.*[\\;"$\r\n])[^/]+(?:/[^/]+)*$',
        "not": {"pattern": r"(^|/)(\.|\.\.)(/|$)"}}
    root = {"anyOf": [{"const": "."}, relative]}
    fact_fields = {name: relative for name in (
        "soc", "routes", "bindings", "references", "device_catalog", "review_profiles")}
    return {
        "schema_version": (True, {"type": "integer", "const": 1}),
        "interface_version": (True, {"type": "integer", "const": INTERFACE_VERSION}),
        "id": (True, identifier), "family": (True, identifier),
        "vendor": (True, identifier), "source_directory": (True, root),
        "facts": (True, {"type": "object", "additionalProperties": False,
            "required": ["soc", "routes"], "properties": fact_fields}),
        "dependencies": (True, {"type": "object", "propertyNames": identifier,
                               "additionalProperties": identifier}),
        "cmake": (True, {"type": "object", "additionalProperties": False,
            "required": ["entry", "target"], "properties": {
                "entry": {"allOf": [relative, {"pattern": r"(^|/)CMakeLists\.txt$"}]},
                "target": {"type": "string", "pattern": "^" + _C_IDENTIFIER.pattern + "$"}}}),
        "exports": (True, {"type": "object", "additionalProperties": False,
            "required": ["roots", "exclude"], "properties": {
                "roots": {"type": "array", "minItems": 1, "uniqueItems": True,
                          "items": root},
                "exclude": {"type": "array", "uniqueItems": True, "items": relative}}}),
        "scope": (False, {"type": "string", "enum": ["production", "reference"]}),
        "binding_files": (False, {"type": "array", "uniqueItems": True,
                                  "items": relative}),
    }


_CONTRACT = _manifest_contract()
_FIELDS = tuple(name for name, (required, _) in _CONTRACT.items() if required)
_OPTIONAL_FIELDS = tuple(name for name, (required, _) in _CONTRACT.items() if not required)


def manifest_schema():
    """Project editor hints from the same named fields accepted by the loader."""
    value = {"$schema": "https://json-schema.org/draft/2020-12/schema",
        "title": "Nexus declarative SoC package manifest",
        "description": "Static package data. Filesystem containment, source identities "
                       "and selected software contracts require configuration validation.",
        "type": "object", "additionalProperties": False,
        "required": list(_FIELDS),
        "properties": {name: schema for name, (_, schema) in _CONTRACT.items()}}
    return json.loads(json.dumps(value))


def _record(value, required, optional=()):
    if (not isinstance(value, Mapping) or not set(required).issubset(value) or
            set(value) - set(required) - set(optional)):
        fail("Declarative package has missing or unknown fields")
    return value


def _identifier(value):
    if not isinstance(value, str) or _IDENTIFIER.fullmatch(value) is None:
        fail("Declarative package requires an ordinary identifier")
    return value


def _relative(value, *, allow_root=False):
    if allow_root and value == ".":
        return Path()
    if (not isinstance(value, str) or not value or "\\" in value or
            any(char in value for char in (';', '"', '$', '\n', '\r'))):
        fail("Declarative package requires an ordinary relative path")
    path = PurePosixPath(value)
    if (path.is_absolute() or str(path) != value or
            any(part in {"", ".", ".."} for part in value.split("/"))):
        fail("Declarative package path escapes its owner")
    return Path(*path.parts)


def _contained(root, relative, *, directory=False, exists=True):
    path = root / relative
    for item in (path, *path.parents):
        if item.is_symlink():
            fail("Declarative package path follows a symlink")
    if not path.resolve().is_relative_to(root.resolve()):
        fail("Declarative package path escapes its owner")
    if exists and not (path.is_dir() if directory else path.is_file()):
        fail("Missing declarative package input: " + str(path))
    return path


def _pairs(items):
    result = {}
    for key, value in items:
        if key in result:
            fail("Duplicate declarative package field: " + key)
        result[key] = value
    return result


class FactReader:
    """Seal standalone package reads without importing the configure command."""
    def __init__(self):
        self.snapshots = {}

    def __call__(self, path):
        raw = path.read_bytes()
        if path in self.snapshots and self.snapshots[path] != raw:
            fail("Package input changed after its first read: " + str(path))
        self.snapshots[path] = raw
        return json.loads(raw, object_pairs_hook=_pairs)

    def snapshot_bytes(self, path):
        if path not in self.snapshots:
            self.snapshots[path] = path.read_bytes()
        return self.snapshots[path]


@dataclass(frozen=True)
class Package:
    """One immutable package; paths resolve only inside its declared owner."""

    manifest_path: Path
    id: str
    family: str
    vendor: str
    source_directory: Path
    facts: Mapping
    dependencies: tuple[tuple[str, str], ...]
    cmake_entry: Path
    target: str
    export_roots: tuple[Path, ...]
    export_excludes: tuple[Path, ...]
    manifest_sha256: str
    import_lock: str | None = None
    source_fact_sha256: str = ""
    scope: str = "production"
    binding_files: tuple[Path, ...] = ()

    def fact(self, name):
        """Return an already validated fact file, without importing its data."""
        return self.source_directory / self.facts[name]

    @property
    def inputs(self):
        return (self.manifest_path, self.cmake_entry,
                *(self.fact(name) for name in self.facts), *self.binding_files)

    def exports(self, path):
        """Check an owned path; no development subtree is implicitly exported."""
        path = Path(path).absolute()
        return (any(path.is_relative_to(root) for root in self.export_roots) and
                not any(path.is_relative_to(root)
                        for root in self.export_excludes) and
                not any(part in {".git", "__pycache__"} for part in path.parts))


def load(path, *, read=None):
    """Load a strict manifest; an optional fact reader seals the same bytes."""
    path = Path(path).absolute()
    _contained(path.parent, Path(path.name))
    raw = path.read_bytes()
    try:
        value = (read(path) if read is not None else
                 json.loads(raw.decode("utf-8"), object_pairs_hook=_pairs))
    except (ValueError, UnicodeDecodeError) as error:
        fail("Invalid declarative package: " + str(error))
    _record(value, _FIELDS, _OPTIONAL_FIELDS)
    if (type(value["schema_version"]) is not int or
            value["schema_version"] != 1 or
            type(value["interface_version"]) is not int or
            value["interface_version"] != INTERFACE_VERSION):
        fail("Unsupported declarative package schema or interface version")
    identifiers = tuple(_identifier(value[name])
                        for name in ("id", "family", "vendor"))
    source = _contained(path.parent, _relative(value["source_directory"],
                                             allow_root=True), directory=True)
    facts = _record(value["facts"], {"soc", "routes"},
                    {"device_catalog", "references", "bindings", "review_profiles"})
    for name in facts.values():
        _contained(source, _relative(name))
    source_fact = source / facts["soc"]
    source_raw = source_fact.read_bytes()
    try:
        source_data = (read(source_fact) if read is not None else
                       json.loads(source_raw.decode("utf-8"), object_pairs_hook=_pairs))
    except (ValueError, UnicodeDecodeError) as error:
        fail("Invalid declarative source fact: " + str(error))
    if not isinstance(source_data, Mapping):
        fail("Declarative source facts require an object")
    source_lock = source_data.get("source_lock")
    import_lock = source_lock if isinstance(source_lock, str) else None
    if import_lock is not None:
        _relative(import_lock)
    if source_fact.read_bytes() != source_raw:
        fail("Declarative source fact changed during loading")
    dependencies = value["dependencies"]
    if not isinstance(dependencies, Mapping):
        fail("Declarative package dependencies require an identifier mapping")
    selected = tuple((_identifier(alias), _identifier(identifier))
                     for alias, identifier in dependencies.items())
    cmake = _record(value["cmake"], {"entry", "target"})
    entry = _relative(cmake["entry"])
    if entry.name != "CMakeLists.txt":
        fail("Declarative package CMake entry must be CMakeLists.txt")
    target = cmake["target"]
    if not isinstance(target, str) or _C_IDENTIFIER.fullmatch(target) is None:
        fail("Declarative package CMake target must be an ordinary identifier")
    cmake_entry = _contained(source, entry)
    exports = _record(value["exports"], {"roots", "exclude"})
    if (not isinstance(exports["roots"], list) or not exports["roots"] or
            not isinstance(exports["exclude"], list)):
        fail("Declarative package exports require roots and explicit exclusions")
    roots = tuple(_contained(source, _relative(name, allow_root=True),
                             directory=True) for name in exports["roots"])
    excludes = tuple(_contained(source, _relative(name), exists=False)
                     for name in exports["exclude"])
    if len(set(roots)) != len(roots) or len(set(excludes)) != len(excludes):
        fail("Duplicate declarative package export scope")
    if path.read_bytes() != raw:
        fail("Declarative package changed during metadata loading")
    scope = value.get("scope", "production")
    if scope not in {"production", "reference"}:
        fail("Declarative package scope must be production or reference")
    fragments = value.get("binding_files", [])
    if not isinstance(fragments, list):
        fail("Package binding files require an explicit ordered list")
    binding_files = tuple(_contained(source, _relative(name)) for name in fragments)
    if (len(set(binding_files)) != len(binding_files) or
            set(binding_files).intersection({path, cmake_entry,
                *(source / name for name in facts.values())})):
        fail("Duplicate declarative package binding input")
    return Package(path, *identifiers, source, MappingProxyType(dict(facts)),
                   selected, cmake_entry, target, roots, excludes,
                   hashlib.sha256(raw).hexdigest(), import_lock,
                   hashlib.sha256(source_raw).hexdigest(), scope, binding_files)


def discover(root):
    """Read local vendor/family manifests; duplicate IDs never use precedence."""
    result = tuple(load(path) for path in
                   sorted((Path(root) / "soc").glob("*/*/" + MANIFEST)))
    for key in ("id", "family"):
        values = [getattr(item, key) for item in result]
        if len(set(values)) != len(values):
            fail("Duplicate declarative package " + key)
    return result


def find(root, family, *, read=None):
    """Select one declared family without opening unselected hardware facts."""
    matches = []
    for path in sorted((Path(root) / "soc").glob("*/*/" + MANIFEST)):
        _contained(path.parent, Path(path.name))
        try:
            value = json.loads(path.read_bytes(), object_pairs_hook=_pairs)
        except (ValueError, UnicodeDecodeError) as error:
            fail("Invalid package discovery metadata: " + str(error))
        if isinstance(value, Mapping) and value.get("family") == family:
            matches.append(load(path, read=read))
    if len(matches) > 1:
        fail("Duplicate declarative package family: " + family)
    return matches[0] if matches else None


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--schema", action="store_true", required=True,
                        help="Print the authoritative package.json editor schema")
    parser.parse_args()
    print(json.dumps(manifest_schema(), indent=2))
