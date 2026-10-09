"""Source and declared-input identity used by all next-generation evidence."""

from __future__ import annotations

import hashlib
from functools import lru_cache
import importlib.util
import json
from pathlib import Path
import subprocess

from tools.evidence.common import EvidenceError, digest, file_identity, load_json


def canonical_digest(value: dict) -> str:
    return hashlib.sha256(json.dumps(value, sort_keys=True,
        separators=(",", ":"), allow_nan=False).encode()).hexdigest()


def git_source(root: Path) -> dict:
    root = root.resolve()
    def git(*args):
        result = subprocess.run(["git", "-C", str(root), *args],
                                capture_output=True, check=False)
        if result.returncode:
            raise EvidenceError("source identity requires a complete Git checkout")
        return result.stdout.decode().strip()
    commit = git("rev-parse", "HEAD")
    tree = git("rev-parse", "HEAD^{tree}")
    status = git("status", "--porcelain", "--untracked-files=all")
    submodules = git("submodule", "status", "--recursive")
    if any(line.startswith(("-", "+", "U")) for line in submodules.splitlines()):
        raise EvidenceError("source submodules are missing or differ from locked commit")
    return {"root": str(root), "commit": commit, "tree": tree,
            "dirty": bool(status), "submodules": submodules.splitlines()}


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
