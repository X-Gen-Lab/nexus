"""Explicit, immutable SoC package selection and verified host-code inputs.

An external package is trusted configuration and CMake code selected by the
authored assembly. Its manifest binds inputs; it is not a source build graph,
a sandbox, a discovery service or a firmware runtime registry.
"""
import argparse
from dataclasses import dataclass
import hashlib
import importlib.abc
import importlib.util
from importlib import import_module
import json
import os
from pathlib import Path, PurePosixPath
import re
import sys
import tempfile
from types import MappingProxyType, ModuleType

if __name__ == "__main__":
    sys.dont_write_bytecode = True

# Direct execution uses only the explicitly selected checkout or installed SDK.
if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.configure.providers import descriptor, maintained
from tools.configure.providers.common import fail


MANIFEST = "nexus-soc.json"
INTERFACE_VERSION = 1
_IDENTIFIER = re.compile(r"[A-Za-z][A-Za-z0-9_-]{0,63}")
_VERSION = re.compile(r"[0-9]+\.[0-9]+\.[0-9]+(?:-[A-Za-z0-9.-]+)?")
_SHA256 = re.compile(r"[0-9a-f]{64}")
_PATH_PATTERN = (r"^(?!/)(?!.*(?:^|/)\.{1,2}(?:/|$))"
                 r'[^\\;"$\r\n/]+(?:/[^\\;"$\r\n/]+)*$')
# Both the runtime scalar checks and editor projection consume this contract.
# Containment, required entry files and actual hashes also require filesystem
# validation; an editor schema never authorizes execution of package code.
_MANIFEST_FIELDS = {
    "schema_version": {"type": "integer", "const": 1},
    "interface_version": {"type": "integer", "const": INTERFACE_VERSION},
    "id": {"type": "string", "pattern": "^" + _IDENTIFIER.pattern + "$"},
    "version": {"type": "string", "pattern": "^" + _VERSION.pattern + "$"},
    "family": {"type": "string", "pattern": "^" + _IDENTIFIER.pattern + "$"},
    "vendor": {"type": "string", "pattern": "^" + _IDENTIFIER.pattern + "$"},
    "source_directory": {"type": "string", "pattern": _PATH_PATTERN},
    "configuration_directory": {"type": "string", "pattern": _PATH_PATTERN},
    "inputs": {"type": "object", "minProperties": 1,
               "propertyNames": {"pattern": _PATH_PATTERN},
               "additionalProperties": {"type": "string",
                                        "pattern": "^" + _SHA256.pattern + "$"}},
}


def manifest_schema():
    """Project editing hints from the runtime manifest field contract."""
    return {"$schema": "https://json-schema.org/draft/2020-12/schema",
            "title": "Explicit Nexus SoC/provider package manifest",
            "description": "Editing hints only; static file and input identity "
                           "validation is required before trusted code loading.",
            "type": "object", "additionalProperties": False,
            "required": list(_MANIFEST_FIELDS),
            "properties": json.loads(json.dumps(_MANIFEST_FIELDS))}


@dataclass(frozen=True)
class PackageManifest:
    """Validated package metadata and bytes, without importing provider code."""

    directory: Path
    descriptor: descriptor.FamilyDescriptor
    id: str
    version: str
    schema_version: int
    interface_version: int
    manifest_sha256: str
    input_digests: tuple[tuple[Path, str], ...]

    @property
    def family(self):
        return self.descriptor.family

    @property
    def vendor(self):
        return self.descriptor.vendor

    @property
    def manifest_path(self):
        return self.directory / MANIFEST

    @property
    def inputs(self):
        return tuple(path for path, _ in self.input_digests)

    @property
    def source_directory(self):
        return self.directory / self.descriptor.source_directory

    @property
    def configuration_directory(self):
        return self.directory / self.descriptor.configuration_directory

    @property
    def identity(self):
        return MappingProxyType({"id": self.id, "version": self.version,
                                 "family": self.family, "vendor": self.vendor,
                                 "manifest_sha256": self.manifest_sha256,
                                 "interface_version": self.interface_version})


@dataclass(frozen=True)
class ProviderContext:
    """One selected implementation and its exact integration input owner."""

    directory: Path
    descriptor: descriptor.FamilyDescriptor
    provider: ModuleType
    inputs: tuple[Path, ...] = ()
    identity: object = None
    input_digests: tuple[tuple[Path, str], ...] = ()

    @property
    def external(self):
        return self.identity is not None

    @property
    def source_directory(self):
        return self.directory / self.descriptor.source_directory


def directory(path):
    """Reject every original symlink component before normalizing dot paths."""
    absolute = Path(path).absolute()
    current = Path(absolute.anchor)
    for component in absolute.parts[1:]:
        current /= component
        if current.is_symlink():
            fail("SoC package path contains a symlink: " + str(current))
    result = absolute.resolve()
    if any(character in str(result) for character in (';', '"', '$', '\n', '\r')):
        fail("SoC package path contains unsupported CMake path characters")
    if not result.is_dir():
        fail("SoC package must be an existing directory: " + str(result))
    return result


def _relative(value):
    if (not isinstance(value, str) or not value or "\\" in value or
            any(character in value for character in (';', '"', '$', '\n', '\r'))):
        fail("SoC package requires an ordinary relative path")
    path = PurePosixPath(value)
    if (path.is_absolute() or any(part in {"", ".", ".."}
                                  for part in value.split("/"))):
        fail("SoC package path must remain relative and cannot escape")
    return path.as_posix()


def _file(root, name):
    path = root / _relative(name)
    directory(path.parent)
    if path.is_symlink() or not path.is_file():
        fail("SoC package input must be a regular file, never a symlink: " +
             str(path))
    if not path.resolve().is_relative_to(root):
        fail("SoC package input escapes its selected owner")
    return path


def _pairs(values):
    result = {}
    for name, value in values:
        if name in result:
            fail("Duplicate SoC package manifest field: " + name)
        result[name] = value
    return result


class _VerifiedSource(importlib.abc.Loader):
    """Compile the already verified source bytes, never timestamp bytecode."""

    def __init__(self, path, digest):
        self.path = path
        self.digest = digest

    def create_module(self, specification):
        return None

    def exec_module(self, module):
        if self.path.is_symlink() or not self.path.is_file():
            fail("External provider source is no longer a regular file")
        source = self.path.read_bytes()
        if hashlib.sha256(source).hexdigest() != self.digest:
            fail("External provider source changed before execution")
        module.__file__ = str(self.path)
        exec(compile(source, str(self.path), "exec"),
             module.__dict__)


class _PackageImports(importlib.abc.MetaPathFinder):
    """Admit only this selected package's declared relative helper modules."""

    def __init__(self, namespace, configuration, declared):
        self.namespace = namespace
        self.configuration = configuration
        self.declared = declared

    def find_spec(self, fullname, path=None, target=None):
        del path, target
        if not fullname.startswith(self.namespace + "."):
            return None
        relative = fullname[len(self.namespace) + 1:].replace(".", "/")
        source = self.configuration / (relative + ".py")
        package = self.configuration / relative / "__init__.py"
        if str(source) in self.declared:
            return importlib.util.spec_from_file_location(
                fullname, source,
                loader=_VerifiedSource(source, self.declared[str(source)]))
        if str(package) in self.declared:
            return importlib.util.spec_from_file_location(
                fullname, package,
                loader=_VerifiedSource(package, self.declared[str(package)]),
                submodule_search_locations=[str(package.parent)])
        fail("External provider imported an undeclared package input: " + relative)


def _load_provider(root, configuration, inputs, identity, digests):
    """Import only the explicit entry under its path-and-byte-specific name."""
    entry = root / configuration / "__init__.py"
    digest = hashlib.sha256((str(root) + "\0" + identity).encode()).hexdigest()
    name = "_nexus_external_soc_" + digest
    # A namespace is unique to both the selected path and complete input bytes.
    # It is Python's import cache, not an integration registration authority.
    module = sys.modules.get(name)
    if module is None:
        specification = importlib.util.spec_from_file_location(
            name, entry, loader=_VerifiedSource(
                entry, digests[entry.relative_to(root).as_posix()]),
            submodule_search_locations=[str(entry.parent)])
        if specification is None or specification.loader is None:
            fail("External provider entry cannot be loaded")
        module = importlib.util.module_from_spec(specification)
        sys.modules[name] = module
        finder = _PackageImports(name, entry.parent,
            {str(path): digests[path.relative_to(root).as_posix()]
             for path in inputs})
        sys.meta_path.insert(0, finder)
        try:
            specification.loader.exec_module(module)
            # The explicit manifest is the complete local-code closure. Load
            # its declared modules before removing the verified finder; later
            # relative imports may reuse those modules, never filesystem code.
            for path in inputs:
                if path.suffix != ".py" or not path.is_relative_to(entry.parent):
                    continue
                relative = path.relative_to(entry.parent).with_suffix("")
                parts = list(relative.parts)
                if parts[-1] == "__init__":
                    parts.pop()
                if any(not part.isidentifier() for part in parts):
                    fail("External provider module path is not a Python identifier")
                selected_name = name + ("." + ".".join(parts) if parts else "")
                import_module(selected_name)
            for loaded, value in tuple(sys.modules.items()):
                if (loaded == name or loaded.startswith(name + ".")) and hasattr(
                        value, "__path__"):
                    value.__path__ = ()
                    value.__spec__.submodule_search_locations = ()
        except BaseException:
            for loaded in tuple(sys.modules):
                if loaded == name or loaded.startswith(name + "."):
                    del sys.modules[loaded]
            raise
        finally:
            sys.meta_path.remove(finder)
    declared = {str(path.resolve()) for path in inputs}
    for loaded, value in tuple(sys.modules.items()):
        if loaded == name or loaded.startswith(name + "."):
            origin = getattr(value, "__file__", None)
            if origin is None or str(Path(origin).resolve()) not in declared:
                fail("External provider imported an undeclared package input: " +
                     str(origin))
    return module


def _manifest_fields(manifest):
    if not isinstance(manifest, dict) or set(manifest) != set(_MANIFEST_FIELDS):
        fail("SoC package manifest has missing or unknown fields")
    for name, rule in _MANIFEST_FIELDS.items():
        value = manifest[name]
        expected = {"integer": int, "string": str, "object": dict}[rule["type"]]
        if type(value) is not expected:
            fail("Invalid SoC package " + name)
        if "const" in rule and value != rule["const"]:
            fail("Unsupported SoC package " + name.replace("_", " "))
        if "pattern" in rule and re.fullmatch(rule["pattern"], value) is None:
            fail("Invalid SoC package " + name)
    if not manifest["inputs"]:
        fail("SoC package requires explicit input SHA256 identities")


def _read_manifest(package_root, *, verify_digests):
    package = directory(package_root)
    manifest_path = _file(package, MANIFEST)
    raw = manifest_path.read_bytes()
    try:
        manifest = json.loads(raw, object_pairs_hook=_pairs)
    except (ValueError, UnicodeError) as error:
        fail("Invalid SoC package manifest: " + str(error))
    _manifest_fields(manifest)
    if manifest["family"] in descriptor.family_names():
        fail("External SoC package cannot shadow a built-in family")
    source = _relative(manifest["source_directory"])
    configuration = _relative(manifest["configuration_directory"])
    paths = manifest["inputs"]
    required = {source + "/" + name for name in
                ("soc.json", "routes.json", "CMakeLists.txt")}
    required.add(configuration + "/__init__.py")
    if not required.issubset(paths):
        fail("SoC package omits required facts, CMakeLists or configuration input")
    input_digests = []
    for name, expected in sorted(paths.items()):
        path = _file(package, name)
        if path == manifest_path:
            fail("SoC package manifest cannot declare its own input digest")
        if not isinstance(expected, str) or _SHA256.fullmatch(expected) is None:
            fail("Invalid SoC package input SHA256 identity: " + name)
        actual = hashlib.sha256(path.read_bytes()).hexdigest()
        if verify_digests and actual != expected:
            fail("SoC package input SHA256 identity differs: " + name)
        input_digests.append((path, actual))
    manifest_digest = hashlib.sha256(raw).hexdigest()
    selected = descriptor.FamilyDescriptor(
        family=manifest["family"], vendor=manifest["vendor"],
        source_directory=source, configuration_directory=configuration)
    record = PackageManifest(package, selected, manifest["id"],
                             manifest["version"], manifest["schema_version"],
                             manifest["interface_version"], manifest_digest,
                             ((manifest_path, manifest_digest), *input_digests))
    return record, manifest, raw


def load_manifest(package_root, *, root=None):
    """Validate static metadata; root anchors an explicit relative package path.

    The returned frozen record includes the manifest itself in inputs and
    input_digests. This operation never imports or executes package code.
    """
    path = Path(package_root) if root is None else Path(root) / package_root
    record, _, _ = _read_manifest(path, verify_digests=True)
    return record


def select(value, *, family, root, assembly_directory):
    """Choose one explicit package; an external identity never shadows builtins."""
    if value is None:
        selected = descriptor.maintained(family)
        return ProviderContext(root, selected, maintained(family))
    if not isinstance(value, str) or not value:
        fail("soc_package requires an explicit directory path")
    manifest = load_manifest(value, root=assembly_directory)
    if manifest.family != family:
        fail("External SoC package differs from Board family")
    digests = {path.relative_to(manifest.directory).as_posix(): digest
               for path, digest in manifest.input_digests}
    provider = _load_provider(manifest.directory,
                              manifest.descriptor.configuration_directory,
                              manifest.inputs, manifest.manifest_sha256, digests)
    # Trusted code may import declared helpers but cannot change selected bytes.
    for path, expected in manifest.input_digests:
        _file(manifest.directory, path.relative_to(manifest.directory).as_posix())
        if hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            fail("SoC package input changed during provider loading")
    return ProviderContext(manifest.directory, manifest.descriptor, provider,
                           manifest.inputs, manifest.identity,
                           manifest.input_digests)


def refresh_inputs(package_root):
    """Atomically refresh only existing declared paths; preserve package version."""
    record, manifest, raw = _read_manifest(package_root, verify_digests=False)
    manifest["inputs"] = {path.relative_to(record.directory).as_posix(): digest
                          for path, digest in record.input_digests
                          if path != record.manifest_path}
    refreshed = (json.dumps(manifest, indent=2, sort_keys=True) + "\n").encode()
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=record.directory,
                                         prefix=".nexus-soc-", delete=False) as stream:
            temporary = Path(stream.name)
            stream.write(refreshed)
            stream.flush()
            os.fsync(stream.fileno())
        for path, expected in record.input_digests:
            _file(record.directory, path.relative_to(record.directory).as_posix())
            if hashlib.sha256(path.read_bytes()).hexdigest() != expected:
                fail("SoC package input changed during digest refresh")
        if record.manifest_path.read_bytes() != raw:
            fail("SoC package manifest changed during digest refresh")
        os.chmod(temporary, record.manifest_path.stat().st_mode & 0o777)
        os.replace(temporary, record.manifest_path)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)
    return load_manifest(record.directory)


def main(argv=None):
    """Check or explicitly refresh declared identities without loading code."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("check", "refresh-inputs"))
    parser.add_argument("--package", type=Path, required=True)
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args(argv)
    try:
        manifest = (load_manifest(args.package) if args.command == "check" else
                    refresh_inputs(args.package))
        report = {**manifest.identity,
                  "declared_inputs": len(manifest.inputs) - 1,
                  "scope": "Static manifest, declared paths and SHA256 identities; "
                           "package configuration and CMake remain trusted host code."}
        if args.json:
            print(json.dumps(report, indent=2, sort_keys=True))
        else:
            action = "Checked" if args.command == "check" else "Refreshed"
            print(f"{action} SoC package {manifest.id}/{manifest.version}: "
                  f"{len(manifest.inputs) - 1} declared inputs")
        return 0
    except (OSError, ValueError, TypeError) as error:
        print(f"SoC package rejected [{args.package}]: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
