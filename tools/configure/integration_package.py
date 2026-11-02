"""Explicit, immutable SoC package selection and verified host-code inputs.

An external package is trusted configuration and CMake code selected by the
authored assembly. Its manifest binds inputs; it is not a source build graph,
a sandbox, a discovery service or a firmware runtime registry.
"""
from dataclasses import dataclass
import hashlib
import importlib.abc
import importlib.util
from importlib import import_module
import json
from pathlib import Path, PurePosixPath
import re
import sys
from types import MappingProxyType, ModuleType

from tools.configure.providers import descriptor, maintained
from tools.configure.providers.common import fail


MANIFEST = "nexus-soc.json"
INTERFACE_VERSION = 1
_IDENTIFIER = re.compile(r"[A-Za-z][A-Za-z0-9_-]{0,63}")
_VERSION = re.compile(r"[0-9]+\.[0-9]+\.[0-9]+(?:-[A-Za-z0-9.-]+)?")


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


def select(value, *, family, root, assembly_directory):
    """Choose one explicit package; an external identity never shadows builtins."""
    if value is None:
        selected = descriptor.maintained(family)
        return ProviderContext(root, selected, maintained(family))
    if not isinstance(value, str) or not value:
        fail("soc_package requires an explicit directory path")
    package = directory(assembly_directory / value)
    manifest_path = _file(package, MANIFEST)
    raw = manifest_path.read_bytes()
    try:
        manifest = json.loads(raw, object_pairs_hook=_pairs)
    except (ValueError, UnicodeError) as error:
        fail("Invalid SoC package manifest: " + str(error))
    fields = {"schema_version", "interface_version", "id", "version", "family",
              "vendor", "source_directory", "configuration_directory", "inputs"}
    if not isinstance(manifest, dict) or set(manifest) != fields:
        fail("SoC package manifest has missing or unknown fields")
    if type(manifest["schema_version"]) is not int or manifest["schema_version"] != 1:
        fail("Unsupported SoC package schema version")
    if (type(manifest["interface_version"]) is not int or
            manifest["interface_version"] != INTERFACE_VERSION):
        fail("Unsupported SoC package interface version")
    for field in ("id", "family", "vendor"):
        if (not isinstance(manifest[field], str) or
                _IDENTIFIER.fullmatch(manifest[field]) is None):
            fail("Invalid SoC package " + field)
    if (not isinstance(manifest["version"], str) or
            _VERSION.fullmatch(manifest["version"]) is None):
        fail("SoC package requires an explicit semantic version")
    if manifest["family"] in {item.family for item in descriptor.families()}:
        fail("External SoC package cannot shadow a built-in family")
    if manifest["family"] != family:
        fail("External SoC package differs from Board family")
    source = _relative(manifest["source_directory"])
    configuration = _relative(manifest["configuration_directory"])
    paths = manifest["inputs"]
    if not isinstance(paths, dict) or not paths:
        fail("SoC package requires explicit input SHA256 identities")
    required = {source + "/" + name for name in
                ("soc.json", "routes.json", "CMakeLists.txt")}
    required.add(configuration + "/__init__.py")
    if not required.issubset(paths):
        fail("SoC package omits required facts, CMakeLists or configuration input")
    inputs = []
    for name, expected in sorted(paths.items()):
        path = _file(package, name)
        if (not isinstance(expected, str) or
                re.fullmatch(r"[0-9a-f]{64}", expected) is None or
                hashlib.sha256(path.read_bytes()).hexdigest() != expected):
            fail("SoC package input SHA256 identity differs: " + name)
        inputs.append(path)
    manifest_digest = hashlib.sha256(raw).hexdigest()
    selected = descriptor.FamilyDescriptor(
        family=family, vendor=manifest["vendor"], source_directory=source,
        configuration_directory=configuration)
    identity = MappingProxyType({name: manifest[name] for name in
                                ("id", "version", "family", "vendor")})
    identity = MappingProxyType({**identity,
                                 "manifest_sha256": manifest_digest,
                                 "interface_version": INTERFACE_VERSION})
    provider = _load_provider(package, configuration, inputs, manifest_digest,
                              paths)
    # Code may import helpers but it may not silently change selected inputs.
    for path in inputs:
        if hashlib.sha256(path.read_bytes()).hexdigest() != paths[
                path.relative_to(package).as_posix()]:
            fail("SoC package input changed during provider loading")
    return ProviderContext(package, selected, provider,
                           (manifest_path, *inputs), identity,
                           ((manifest_path, manifest_digest),
                            *((path, paths[path.relative_to(package).as_posix()])
                              for path in inputs)))
