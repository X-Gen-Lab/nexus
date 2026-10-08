"""Identity for reviewed, tracked SDK imports; never invent a vendor Git repo."""
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import subprocess
from urllib.parse import urlparse

try:
    from .verify_vendor_source import verify
except ImportError:
    from verify_vendor_source import verify


class ImportIdentityError(ValueError):
    """A source import differs from its lock or reviewed repository tree."""


def _git(root, *args):
    try:
        result = subprocess.run(["git", "-C", str(root), *args], check=False,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=20)
    except (OSError, subprocess.TimeoutExpired) as error:
        raise ImportIdentityError("Source import Git identity unavailable") from error
    if result.returncode:
        raise ImportIdentityError("Source import Git identity unavailable")
    return result.stdout


def _object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ImportIdentityError("Duplicate source lock metadata")
        result[key] = value
    return result


def lock_metadata(lock):
    if not isinstance(lock, dict):
        raise ImportIdentityError("Source import lock must be an object")
    metadata = {}
    for key in ("vendor", "package", "version", "source_url", "download_sha256", "nested_archive_sha256"):
        value = lock.get(key)
        if not isinstance(value, str) or not value or len(value) > 2048:
            raise ImportIdentityError("Source import lacks its upstream identity")
        metadata[key] = value
    for key in ("download_sha256", "nested_archive_sha256"):
        if not re.fullmatch(r"[0-9a-f]{64}", metadata[key]):
            raise ImportIdentityError("Source import archive digest is invalid")
    parsed = urlparse(metadata["source_url"])
    if parsed.scheme != "https" or not parsed.hostname or parsed.username or parsed.password:
        raise ImportIdentityError("Source import upstream URL must be an HTTPS identity")
    return metadata


def _relative_path(value, prefix=None):
    if not isinstance(value, str) or not value or "\\" in value:
        raise ImportIdentityError("Invalid source import relative path")
    path = PurePosixPath(value)
    if (not path.parts or path.is_absolute() or ".." in path.parts or "." in path.parts
            or path.as_posix() != value or (prefix and path.parts[0] != prefix)):
        raise ImportIdentityError("Unsafe source import relative path")
    return value


def locked_files(lock):
    entries = lock.get("files")
    if not isinstance(entries, list) or not entries:
        raise ImportIdentityError("Source import lock requires file identities")
    names = set()
    for entry in entries:
        if not isinstance(entry, dict) or set(entry) != {"path", "sha256", "bytes", "license"}:
            raise ImportIdentityError("Invalid source import file identity fields")
        name = _relative_path(entry["path"], "Firmware")
        if "/" not in name or name in names:
            raise ImportIdentityError("Duplicate or invalid source import file")
        names.add(name)
        if (not isinstance(entry["sha256"], str) or
                not re.fullmatch(r"[0-9a-f]{64}", entry["sha256"]) or
                type(entry["bytes"]) is not int or entry["bytes"] < 0 or
                not isinstance(entry["license"], str) or
                not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_.:-]{0,127}", entry["license"])):
            raise ImportIdentityError("Invalid source import digest, size or license identity")
    return entries


def reviewed_import(root, repository):
    root = Path(root).absolute()
    repository = Path(repository).resolve()
    if root.is_symlink() or not root.is_dir() or root.resolve() != root:
        raise ImportIdentityError("Source import root must be a regular directory")
    try:
        relative = root.relative_to(repository).as_posix()
    except ValueError as exc:
        raise ImportIdentityError("Source import is outside the build repository") from exc
    if relative == ".":
        raise ImportIdentityError("Source import must be a typed repository subtree")
    parent = root
    while parent != repository:
        if parent.is_symlink():
            raise ImportIdentityError("Source import cannot follow directory symlinks")
        parent = parent.parent
    if Path(_git(repository, "rev-parse", "--show-toplevel").decode().strip()).resolve() != repository:
        raise ImportIdentityError("Source import owner must be a Git repository root")
    result = verify(root)
    lock_path = root / "source.lock.json"
    lock = json.loads(lock_path.read_text(encoding="utf-8"), object_pairs_hook=_object)
    upstream = lock_metadata(lock)
    locked_files(lock)
    notices = [root / "README.md"] + sorted((root / "LICENSES").rglob("*"))
    notices = [path for path in notices if path.is_file() or path.is_symlink()]
    if len(notices) < 2:
        raise ImportIdentityError("Source import requires origin and license notices")
    entries = [lock_path, *notices, *(root / entry["path"] for entry in lock["files"])]
    expected = {path.relative_to(root).as_posix() for path in entries}
    actual = {path.relative_to(root).as_posix() for path in root.rglob("*")
              if path.is_file() or path.is_symlink()}
    if actual != expected:
        raise ImportIdentityError("Source import contains unreviewed auxiliary files")
    tree = {}
    for raw in _git(repository, "ls-tree", "-r", "-z", "HEAD", "--", relative).split(b"\0"):
        if not raw:
            continue
        meta, name = raw.split(b"\t", 1)
        mode, kind, oid = meta.decode().split()
        if kind != "blob" or mode not in ("100644", "100755"):
            raise ImportIdentityError("Source import requires tracked regular blobs, not Git links")
        tree[name.decode("utf-8")] = oid
    if set(tree) != {relative + "/" + name for name in expected}:
        raise ImportIdentityError("Reviewed source import tree differs from its locked file/notice set")
    notice_records = []
    for path in entries:
        if path.is_symlink() or not path.is_file() or path.resolve() != path:
            raise ImportIdentityError("Source import files must be regular")
        data = path.read_bytes()
        name = path.relative_to(repository).as_posix()
        oid = tree.get(name)
        if not oid:
            raise ImportIdentityError("Source import file is not in the reviewed source commit")
        algorithm = "sha1" if len(oid) == 40 else "sha256"
        observed = hashlib.new(algorithm, b"blob " + str(len(data)).encode() + b"\0" + data).hexdigest()
        if observed != oid:
            raise ImportIdentityError("Source import differs from its reviewed source commit")
        if path in notices:
            if not data:
                raise ImportIdentityError("Source import origin and license notices must be nonempty")
            notice_records.append({"path": path.relative_to(root).as_posix(),
                                   "sha256": hashlib.sha256(data).hexdigest(), "bytes": len(data)})
    return {"kind": "vendor-source-import", "path": relative,
            "source_commit": _git(repository, "rev-parse", "HEAD").decode().strip(),
            "source_tree": _git(repository, "rev-parse", "HEAD^{tree}").decode().strip(),
            "lock_sha256": hashlib.sha256(lock_path.read_bytes()).hexdigest(),
            "upstream": upstream, "files": result["files"], "bytes": result["bytes"],
            "notices": notice_records}


def validate_import_record(record, lock_contents, notice_contents, source_commit):
    """Validate archived lock/notices without treating them as upstream signing."""
    required = {"kind", "path", "source_commit", "source_tree", "lock_sha256", "upstream", "files", "bytes", "notices"}
    if (not isinstance(record, dict) or set(record) != required or
            record.get("kind") != "vendor-source-import"):
        raise ImportIdentityError("Invalid typed source import identity")
    _relative_path(record["path"])
    if any(not isinstance(record[key], str) or not re.fullmatch(r"[0-9a-f]{40}|[0-9a-f]{64}", record[key])
           for key in ("source_commit", "source_tree")):
        raise ImportIdentityError("Invalid source import owning repository identity")
    if record.get("source_commit") != source_commit:
        raise ImportIdentityError("Source import belongs to another source commit")
    if hashlib.sha256(lock_contents).hexdigest() != record.get("lock_sha256"):
        raise ImportIdentityError("Source import lock digest mismatch")
    lock = json.loads(lock_contents, object_pairs_hook=_object)
    if record.get("upstream") != lock_metadata(lock):
        raise ImportIdentityError("Source import upstream identity differs from its lock")
    entries = locked_files(lock)
    if type(record.get("files")) is not int or record["files"] != len(entries):
        raise ImportIdentityError("Source import file count differs from its lock")
    if type(record.get("bytes")) is not int or record["bytes"] != sum(entry["bytes"] for entry in entries):
        raise ImportIdentityError("Source import byte count differs from its lock")
    notices = record.get("notices")
    if not isinstance(notices, list) or len(notices) < 2:
        raise ImportIdentityError("Source import origin and license notices are missing")
    for item in notices:
        if not isinstance(item, dict) or set(item) != {"path", "sha256", "bytes"}:
            raise ImportIdentityError("Invalid source import notice identity")
        name = _relative_path(item["path"])
        if name != "README.md" and not name.startswith("LICENSES/"):
            raise ImportIdentityError("Source import notice path is not origin or license evidence")
        if type(item["bytes"]) is not int or item["bytes"] <= 0:
            raise ImportIdentityError("Source import notice is empty or has invalid size")
    if (len({item["path"] for item in notices}) != len(notices) or
            "README.md" not in notice_contents or
            {item["path"] for item in notices} != set(notice_contents)):
        raise ImportIdentityError("Source import notice set differs from its identity")
    for item in notices:
        content = notice_contents[item["path"]]
        if len(content) != item.get("bytes") or hashlib.sha256(content).hexdigest() != item.get("sha256"):
            raise ImportIdentityError("Source import notice content differs from its identity")
