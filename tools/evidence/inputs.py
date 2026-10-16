"""Byte identities of explicitly declared, regular input directories."""

import hashlib
import json
from pathlib import Path

from tools.evidence.common import EvidenceError, fields, sha256_value, stream_digest


def records_identity(records: list[dict]) -> dict:
    """Use one canonical content digest for live and archived input snapshots."""
    if not isinstance(records, list) or not 0 < len(records) <= 1000000:
        raise EvidenceError("nonempty bounded input file enumeration required")
    names = set()
    for entry in records:
        fields(entry, {"path", "size", "sha256"})
        name = entry["path"]
        if (not isinstance(name, str) or "\\" in name or
                Path(name).is_absolute() or any(part in ("", ".", "..")
                for part in name.split("/")) or name in names or
                type(entry["size"]) is not int or not 0 <= entry["size"] < 2**64):
            raise EvidenceError("invalid or duplicate input file record")
        names.add(name)
        sha256_value(entry["sha256"])
    encoded = json.dumps(sorted(records, key=lambda entry: Path(entry["path"])),
                         sort_keys=True, separators=(",", ":")).encode()
    return {"sha256": hashlib.sha256(encoded).hexdigest(), "files": len(records),
            "bytes": sum(entry["size"] for entry in records)}


def tree_identity(root: Path) -> dict:
    if root.is_symlink() or not root.is_dir():
        raise EvidenceError("declared input directory missing or symlink")
    records = []
    for path in sorted(root.rglob("*")):
        if path.is_symlink():
            raise EvidenceError("undeclared symlink in input tree")
        if path.is_file():
            with path.open("rb") as stream:
                sha256 = stream_digest(stream)
            records.append({"path": path.relative_to(root).as_posix(),
                            "size": path.stat().st_size,
                            "sha256": sha256})
    return records_identity(records)
