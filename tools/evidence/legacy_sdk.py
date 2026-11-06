"""Check only a frozen SDK's declared bytes; do not reestablish qualification."""

from pathlib import Path, PurePosixPath

from tools.evidence.common import (
    EvidenceError, fields, load_json, regular_file, sha256_value, stream_digest,
)


def _member_name(value):
    """Keep original member spelling unique without navigating the old tree."""
    if not isinstance(value, str):
        raise EvidenceError("legacy SDK member name must be a string")
    path = PurePosixPath(value)
    if (not value or not path.parts or path.is_absolute() or
            path.as_posix() != value or
            ".." in path.parts or "\\" in value or
            any(ord(character) < 32 or ord(character) == 127
                for character in value)):
        raise EvidenceError("noncanonical legacy SDK member name")
    return path


def _checked_bytes(identity, resolver, *, source_file=False):
    """The resolver locates packed bytes; their digest remains authoritative."""
    fields(identity, {"path", "sha256", "size", "role"} if source_file else
           {"path", "sha256", "size"})
    if source_file and identity["role"] != "sdk_file":
        raise EvidenceError("legacy SDK member requires its source-file role")
    size = identity["size"]
    if type(size) is not int or size < (0 if source_file else 1):
        raise EvidenceError("invalid legacy SDK identity size")
    sha256_value(identity["sha256"])
    path = regular_file(resolver(identity), nonempty=not source_file)
    before = path.stat()
    with path.open("rb") as stream:
        actual = stream_digest(stream)
    after = path.stat()
    stable = ("st_dev", "st_ino", "st_mode", "st_size", "st_mtime_ns",
              "st_ctime_ns")
    if (any(getattr(before, name) != getattr(after, name) for name in stable) or
            after.st_size != size or
            actual != identity["sha256"]):
        raise EvidenceError("legacy SDK payload identity changed")
    return path


def validate_declared_sdk_closure(identity, sdk, closure, *, resolver):
    """Replay exact original member declarations without executing SDK code.

    This checks no current required-source list, import policy, build result or
    qualification scope. The caller must explicitly select legacy replay and
    retain that distinction when displaying the original seal's declarations.
    """
    fields(closure, {"manifest_sha256", "files", "package_files"})
    sha256_value(closure["manifest_sha256"])
    if closure["manifest_sha256"] != identity.get("sha256"):
        raise EvidenceError("legacy SDK manifest identity differs from closure")
    original = identity.get("path")
    if not isinstance(original, str):
        raise EvidenceError("invalid legacy SDK manifest path")
    manifest = Path(original)
    root = manifest.parent
    if (not manifest.is_absolute() or str(manifest) != original or
            ".." in manifest.parts or manifest.name != ".nexus-source-sdk.json"
            or root.parts[-3:] != ("share", "nexus", "src")):
        raise EvidenceError("noncanonical legacy SDK manifest path")
    manifest_path = _checked_bytes(identity, resolver)
    if load_json(manifest_path) != sdk:
        raise EvidenceError("legacy SDK manifest bytes differ from parsed data")
    if (type(sdk.get("schema")) is not int or sdk["schema"] != 1 or
            sdk.get("kind") != "nexus-installed-source-sdk"):
        raise EvidenceError("unsupported legacy SDK manifest format")
    seen = {original}
    for key, declared_key, original_root in (
            ("files", "files_sha256", root),
            ("package_files", "package_files_sha256", root.parents[2])):
        declared, members = sdk.get(declared_key), closure[key]
        if (not isinstance(declared, dict) or not declared or
                not isinstance(members, dict) or set(members) != set(declared)):
            raise EvidenceError("legacy SDK member map differs from declaration")
        for name, record in members.items():
            relative = _member_name(name)
            sha256_value(declared[name])
            if not isinstance(record, dict):
                raise EvidenceError("invalid legacy SDK member identity")
            if record.get("sha256") != declared[name]:
                raise EvidenceError("legacy SDK member differs from declared digest")
            original_path = str(original_root / relative)
            if record.get("path") != original_path:
                raise EvidenceError("legacy SDK member leaves its original SDK path")
            if original_path in seen:
                raise EvidenceError("duplicate legacy SDK member path")
            seen.add(original_path)
            _checked_bytes(record, resolver, source_file=True)
    _checked_bytes(identity, resolver)
    return closure
