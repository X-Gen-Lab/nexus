"""Source and declared-input identity used by all next-generation evidence."""

from __future__ import annotations

import hashlib
from functools import lru_cache
import importlib.util
import json
import os
from pathlib import Path
import stat
import subprocess

from tools.evidence.common import (EvidenceError, digest, file_identity, load_json,
                                   stream_digest)


def canonical_digest(value: dict) -> str:
    return hashlib.sha256(json.dumps(value, sort_keys=True,
        separators=(",", ":"), allow_nan=False).encode()).hexdigest()


def _git_environment() -> dict:
    return {key: value for key, value in os.environ.items()
            if not key.startswith("GIT_")}


def _git_output(root: Path, *args) -> bytes:
    result = subprocess.run(["git", "-C", str(root), *args],
                            capture_output=True, check=False,
                            env=_git_environment())
    if result.returncode:
        raise EvidenceError("source identity requires a complete Git checkout")
    return result.stdout


def _stat_signature(value) -> tuple:
    return (value.st_dev, value.st_ino, value.st_mode, value.st_size,
            value.st_mtime_ns, value.st_ctime_ns)


def _untracked_identity(root: Path, name: bytes) -> dict:
    relative = Path(os.fsdecode(name))
    if relative.is_absolute() or ".." in relative.parts:
        raise EvidenceError("invalid untracked source path")
    path = root / relative
    if any(parent.is_symlink() for parent in path.parents
           if parent != root and parent.is_relative_to(root)):
        raise EvidenceError("untracked source has a symlink parent")
    try:
        before = path.lstat()
        record = {"path_hex": name.hex(), "mode": stat.S_IMODE(before.st_mode)}
        if stat.S_ISLNK(before.st_mode):
            record.update(kind="symlink", target_hex=os.fsencode(os.readlink(path)).hex())
        elif stat.S_ISREG(before.st_mode):
            flags = os.O_RDONLY | os.O_NONBLOCK | getattr(os, "O_NOFOLLOW", 0)
            with os.fdopen(os.open(path, flags), "rb") as stream:
                if _stat_signature(os.fstat(stream.fileno())) != _stat_signature(before):
                    raise EvidenceError("source changed during observation")
                record.update(kind="file", bytes=before.st_size,
                              sha256=stream_digest(stream))
                if _stat_signature(os.fstat(stream.fileno())) != _stat_signature(before):
                    raise EvidenceError("source changed during observation")
        else:
            raise EvidenceError("unsupported untracked source file type")
        if _stat_signature(path.lstat()) != _stat_signature(before):
            raise EvidenceError("source changed during observation")
        return record
    except OSError as error:
        raise EvidenceError("untracked source could not be observed safely") from error


def _dirty_worktree_digest(root: Path) -> str:
    # Stream Git's actual binary worktree patch; do not buffer large patches or
    # invoke configured external diff/textconv commands. The index records bind
    # staged modes/blob identities, including an index-only dirty transition.
    command = ["git", "-C", str(root), "diff", "--binary", "--full-index",
               "--no-color", "--no-ext-diff", "--no-textconv", "HEAD", "--"]
    with subprocess.Popen(command, stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT, env=_git_environment()) as process:
        patch_digest = stream_digest(process.stdout)
        if process.wait():
            raise EvidenceError("dirty source patch could not be observed")
    index = _git_output(root, "ls-files", "--stage", "-z")
    untracked = _git_output(root, "ls-files", "--others", "--exclude-standard", "-z")
    children = []
    for entry in index.split(b"\0"):
        if not entry.startswith(b"160000 "):
            continue
        _, name = entry.split(b"\t", 1)
        child = root / os.fsdecode(name)
        observation = None
        if child.is_dir() and (child / ".git").exists():
            if child.is_symlink() or not child.resolve().is_relative_to(root):
                raise EvidenceError("ambiguous source submodule path")
            actual = Path(os.fsdecode(_git_output(child, "rev-parse", "--show-toplevel").strip()))
            if actual.resolve() != child.resolve():
                raise EvidenceError("submodule is not an independent Git checkout")
            observation = observe_git_source(child)
        children.append({"path_hex": name.hex(), "source": observation})
    return canonical_digest({
        "version": 1, "tracked_patch_sha256": patch_digest,
        "index_sha256": hashlib.sha256(index).hexdigest(),
        "untracked": [_untracked_identity(root, name)
                      for name in sorted(filter(None, untracked.split(b"\0")))],
        "submodules": children})


def observe_git_source(root: Path) -> dict:
    """Observe Git source; only dirty development records gain byte identity.

    Clean records retain their five-field candidate/consumer schema. A dirty
    record binds current tracked/untracked bytes, modes, symlink text and
    initialized submodules, without qualifying ignored or hidden worktree data.
    Equality observes boundaries, not a lock against concurrent/ABA writers.
    """
    root = root.resolve()
    def git(*args):
        return _git_output(root, *args).decode().strip()
    commit = git("rev-parse", "HEAD")
    tree = git("rev-parse", "HEAD^{tree}")
    status = git("status", "--porcelain", "--untracked-files=all")
    submodules = git("submodule", "status", "--recursive")
    source = {"root": str(root), "commit": commit, "tree": tree,
              "dirty": bool(status), "submodules": submodules.splitlines()}
    if status:
        source["working_tree_sha256"] = _dirty_worktree_digest(root)
        if (git("rev-parse", "HEAD") != commit or
                git("status", "--porcelain", "--untracked-files=all") != status or
                git("submodule", "status", "--recursive") != submodules):
            raise EvidenceError("source changed during observation")
    return source


def git_source(root: Path) -> dict:
    source = observe_git_source(root)
    if any(line.startswith(("-", "+", "U"))
           for line in source['submodules']):
        raise EvidenceError(
            "source submodules are missing or differ from locked commit")
    return source


def verify_source(source: dict, *, clean: bool = False) -> None:
    if git_source(Path(source["root"])) != source:
        raise EvidenceError("source identity is stale")
    if clean and source["dirty"]:
        raise EvidenceError("dirty source cannot qualify a software candidate")


def resolved_identity(path: Path) -> tuple[dict, dict]:
    """One resolved file is the configuration authority, not a second config."""
    resolved = load_json(path)
    if type(resolved.get("schema_version")) is not int or resolved["schema_version"] != 1:
        raise EvidenceError("unsupported resolved schema")
    claimed = resolved.get("configuration_sha256")
    canonical = {key: value for key, value in resolved.items()
                 if key != "configuration_sha256"}
    if claimed != canonical_digest(canonical):
        raise EvidenceError("resolved canonical configuration digest mismatch")
    return resolved, file_identity(path)


def check_identity(identity: dict, path: Path) -> None:
    if identity.get("sha256") != digest(path):
        raise EvidenceError("evidence refers to a different artifact: " + path.name)


def resolved_input_files(path: Path, resolved: dict) -> list[dict]:
    """Navigation locates bytes; only the sole IR's digest authorizes them."""
    navigation = load_json(path.parent / "input_paths.json")
    records = resolved.get("inputs")
    if not isinstance(records, list) or not records:
        raise EvidenceError("resolved declared-input closure missing")
    labels = [record["path"] for record in records]
    if len(labels) != len(set(labels)) or set(navigation) != set(labels):
        raise EvidenceError("configuration input navigation differs from declared closure")
    files = []
    for record in records:
        file = Path(navigation[record["path"]])
        identity = file_identity(file)
        if identity["sha256"] != record["sha256"]:
            raise EvidenceError("declared configuration input changed: " + record["path"])
        files.append({"label": record["path"], "file": identity})
    return files


@lru_cache(maxsize=1)
def _source_sdk_validator():
    """Load the owned script explicitly; pip's cmake package shadows namespaces."""
    script = Path(__file__).resolve().parents[2] / "cmake/package/package_source_sdk.py"
    spec = importlib.util.spec_from_file_location("nexus_source_sdk_validator", script)
    if spec is None or spec.loader is None:
        raise EvidenceError("owned source SDK validator unavailable")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def verify_sdk_package(root: Path) -> dict:
    """Reuse the single package verifier without a competing SDK engine."""
    return _source_sdk_validator().verify(root)
