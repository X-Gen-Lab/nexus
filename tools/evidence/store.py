#!/usr/bin/env python3
"""Portable immutable evidence archives with complete byte verification."""

from __future__ import annotations

import argparse
import gzip
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import sys
import tarfile
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.evidence.common import (EvidenceError, atomic_json, digest, fields,
    file_identity, load_json, regular_file, separate_outputs, stream_digest, verify_file_identity)


def _safe_path(name: str) -> str:
    if (not isinstance(name, str) or not name or "\\" in name or
            PurePosixPath(name).is_absolute() or
            any(part in ("", ".", "..") for part in name.split("/"))):
        raise EvidenceError("archive member path is unsafe")
    return name


def _safe_directory(path: Path) -> Path:
    path = path.absolute()
    if any(part.is_symlink() for part in [path, *path.parents]):
        raise EvidenceError("evidence directory cannot contain symlinks")
    return path


def inventory(root: Path) -> list[dict]:
    root = _safe_directory(root)
    if not root.is_dir():
        raise EvidenceError("evidence source directory is missing")
    records = []
    for path in sorted(root.rglob("*")):
        if path.is_symlink() or not (path.is_file() or path.is_dir()):
            raise EvidenceError("evidence contains symlink or special entry")
        record = {"path": _safe_path(path.relative_to(root).as_posix()),
                  "kind": "file" if path.is_file() else "directory",
                  "mode": path.stat().st_mode & 0o777}
        if path.is_file():
            record.update(size=path.stat().st_size)
            with path.open("rb") as stream:
                record["sha256"] = stream_digest(stream)
        records.append(record)
    if not any(record["kind"] == "file" for record in records):
        raise EvidenceError("empty evidence tree cannot be sealed")
    return records


def export_tree(source: Path, storage: Path) -> dict:
    """Seal once by content; do not mutate source or claim release approval."""
    source, storage = _safe_directory(source), _safe_directory(storage)
    if storage == source or source in storage.parents or storage in source.parents:
        raise EvidenceError("evidence source and store must not overlap")
    records = inventory(source)
    manifest = {"schema_version": 1, "kind": "evidence_tree",
                "entries": records}
    encoded = (json.dumps(manifest, sort_keys=True, separators=(",", ":")) + "\n").encode()
    storage.mkdir(parents=True, exist_ok=True)
    descriptor, name = tempfile.mkstemp(prefix=".incoming-", suffix=".tar.gz", dir=storage)
    temporary = Path(name)
    try:
        with os.fdopen(descriptor, "wb") as raw, gzip.GzipFile(fileobj=raw, mode="wb", mtime=0, filename="") as compressed:
            with tarfile.open(fileobj=compressed, mode="w|") as archive:
                for record in records:
                    info = tarfile.TarInfo("payload/" + record["path"])
                    info.mode = record["mode"]
                    if record["kind"] == "directory":
                        info.type = tarfile.DIRTYPE
                        archive.addfile(info)
                    else:
                        info.size = record["size"]
                        with (source / record["path"]).open("rb") as stream:
                            archive.addfile(info, stream)
                import io
                info = tarfile.TarInfo("delivery-manifest.json")
                info.mode, info.size = 0o644, len(encoded)
                archive.addfile(info, io.BytesIO(encoded))
        if inventory(source) != records:
            raise EvidenceError("evidence source changed while sealing")
        sha = digest(temporary)
        archive_path = storage / (sha + ".tar.gz")
        if archive_path.exists():
            if digest(archive_path) != sha:
                raise EvidenceError("existing evidence archive is corrupt")
            temporary.unlink()
        else:
            temporary.replace(archive_path)
        package = {"schema_version": 1, "kind": "portable_evidence",
                   "archive": file_identity(archive_path),
                   "manifest_sha256": hashlib.sha256(encoded).hexdigest(),
                   "files": sum(record["kind"] == "file" for record in records),
                   "bytes": sum(record.get("size", 0) for record in records),
                   "signature_status": "unsigned", "product_promotion": False}
        verify_package(package)
        atomic_json(storage / (sha + ".json"), package)
        return package
    finally:
        temporary.unlink(missing_ok=True)


def _members(package: dict, archive_override: Path | None = None) -> tuple[Path, dict]:
    fields(package, {"schema_version", "kind", "archive", "manifest_sha256",
                     "files", "bytes", "signature_status", "product_promotion"})
    if (package["schema_version"] != 1 or package["kind"] != "portable_evidence" or
            package["signature_status"] != "unsigned" or package["product_promotion"] is not False):
        raise EvidenceError("unsupported portable evidence envelope")
    identity = dict(package["archive"])
    if archive_override is not None:
        identity["path"] = str(archive_override.absolute())
    archive_path = verify_file_identity(identity)
    with tarfile.open(archive_path, "r:gz") as archive:
        member = archive.getmember("delivery-manifest.json")
        if not member.isfile() or member.size > 64 * 1024 * 1024:
            raise EvidenceError("missing or oversized evidence manifest")
        raw = archive.extractfile(member).read()
        if hashlib.sha256(raw).hexdigest() != package["manifest_sha256"]:
            raise EvidenceError("evidence manifest identity changed")
        from tools.evidence.common import parse_json
        manifest = parse_json(raw)
    fields(manifest, {"schema_version", "kind", "entries"})
    if manifest["schema_version"] != 1 or manifest["kind"] != "evidence_tree":
        raise EvidenceError("unsupported evidence manifest")
    if not isinstance(manifest["entries"], list) or not manifest["entries"]:
        raise EvidenceError("nonempty evidence entries required")
    expected = {}
    for record in manifest["entries"]:
        if not isinstance(record, dict) or record.get("kind") not in ("file", "directory"):
            raise EvidenceError("invalid evidence entry")
        fields(record, {"path", "kind", "mode"}, {"size", "sha256"})
        name = "payload/" + _safe_path(record["path"])
        if name in expected or type(record["mode"]) is not int or not 0 <= record["mode"] <= 0o777:
            raise EvidenceError("duplicate path or invalid evidence mode")
        if record["kind"] == "file":
            if type(record.get("size")) is not int or record["size"] < 0 or record.get("sha256") is None:
                raise EvidenceError("file identity is incomplete")
        elif set(record) != {"path", "kind", "mode"}:
            raise EvidenceError("directory has unexpected file identity")
        expected[name] = record
    return archive_path, expected


def verify_package(package: dict, *, archive_override: Path | None = None) -> dict:
    archive_path, expected = _members(package, archive_override)
    seen, total = set(), 0
    with tarfile.open(archive_path, "r|gz") as archive:
        for member in archive:
            if member.name in seen:
                raise EvidenceError("duplicate evidence archive member")
            seen.add(member.name)
            if member.name == "delivery-manifest.json":
                continue
            record = expected.get(member.name)
            if (record is None or (member.mode & 0o777) != record["mode"] or
                    member.isdir() != (record["kind"] == "directory") or
                    not (member.isfile() or member.isdir())):
                raise EvidenceError("archive entry differs from evidence manifest")
            if member.isfile():
                if (member.size != record["size"] or
                        stream_digest(archive.extractfile(member)) != record["sha256"]):
                    raise EvidenceError("archived evidence bytes changed")
                total += member.size
    files = sum(record["kind"] == "file" for record in expected.values())
    if (seen != {*expected, "delivery-manifest.json"} or
            files != package["files"] or total != package["bytes"]):
        raise EvidenceError("evidence archive closure is incomplete")
    return {"status": "verified", "files": files, "bytes": total,
            "archive_sha256": package["archive"]["sha256"]}


def restore_tree(package: dict, destination: Path, *,
                 archive_override: Path | None = None) -> dict:
    verify_package(package, archive_override=archive_override)
    archive_path, expected = _members(package, archive_override)
    destination = _safe_directory(destination)
    if destination.exists():
        raise EvidenceError("evidence restore destination must be new")
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = Path(tempfile.mkdtemp(prefix=".evidence-restore-", dir=destination.parent))
    try:
        with tarfile.open(archive_path, "r|gz") as archive:
            for member in archive:
                if member.name == "delivery-manifest.json":
                    continue
                record = expected[member.name]
                target = temporary / record["path"]
                if member.isdir():
                    target.mkdir(parents=True, exist_ok=True)
                else:
                    target.parent.mkdir(parents=True, exist_ok=True)
                    with target.open("xb") as stream:
                        shutil.copyfileobj(archive.extractfile(member), stream, 1024 * 1024)
                if member.isfile():
                    target.chmod(record["mode"])
        for record in reversed(list(expected.values())):
            if record["kind"] == "directory":
                (temporary / record["path"]).chmod(record["mode"])
        if inventory(temporary) != list(expected.values()):
            raise EvidenceError("restored evidence differs from complete manifest")
        verify_file_identity({**package["archive"], "path": str(archive_path)})
        temporary.replace(destination)
        return {"status": "verified", "destination": str(destination),
                "files": package["files"], "bytes": package["bytes"],
                "archive_sha256": package["archive"]["sha256"]}
    finally:
        if temporary.exists():
            shutil.rmtree(temporary)


def _external_metrics(archive: Path) -> dict:
    members, files, byte_count = set(), 0, 0
    inventory_hash = hashlib.sha256()
    with tarfile.open(archive, "r|*") as content:
        for member in content:
            _safe_path(member.name.rstrip("/"))
            if member.name in members or not (member.isfile() or member.isdir()):
                raise EvidenceError("external archive has unsafe or duplicate members")
            members.add(member.name)
            entry = {"path": member.name, "size": member.size,
                     "mode": member.mode,
                     "kind": "file" if member.isfile() else "directory"}
            if member.isfile():
                entry["sha256"] = stream_digest(content.extractfile(member))
                files += 1
                byte_count += member.size
            inventory_hash.update((json.dumps(entry, sort_keys=True,
                separators=(",", ":")) + "\n").encode())
            # Stream consumers never look up an earlier TarInfo by name.
            content.members.clear()
    if not files:
        raise EvidenceError("external archive is empty")
    return {"members": len(members), "files": files, "bytes": byte_count,
            "member_inventory_sha256": inventory_hash.hexdigest()}


def verify_external(record: dict, *, archive_override: Path | None = None) -> dict:
    fields(record, {"schema_version", "kind", "archive", "original", "storage",
                    "signature_status", "status", "members", "files", "bytes",
                    "member_inventory_sha256", "scope"})
    if record["schema_version"] != 1 or record["kind"] != "durable_external_archive":
        raise EvidenceError("unsupported external archive receipt")
    identity = dict(record["archive"])
    if archive_override is not None:
        identity["path"] = str(archive_override.absolute())
    archive = verify_file_identity(identity)
    measured = _external_metrics(archive)
    if any(measured[key] != record[key] for key in measured):
        raise EvidenceError("durable archive member inventory changed")
    verify_file_identity(identity)
    return {"status": "verified", **measured,
            "archive_sha256": record["archive"]["sha256"]}


def adopt(archive: Path, storage: Path) -> dict:
    """Hard-link existing evidence into its durable identity and read all bytes."""
    archive = regular_file(archive)
    storage = _safe_directory(storage)
    storage.mkdir(parents=True, exist_ok=True)
    sha = digest(archive)
    destination = storage / (sha + ".tar.gz")
    created = not destination.exists()
    if destination.exists():
        if digest(destination) != sha:
            raise EvidenceError("existing durable evidence identity changed")
    else:
        try:
            os.link(archive, destination)
        except OSError as error:
            raise EvidenceError("in-place adoption requires same-filesystem hardlink") from error
    try:
        measured = _external_metrics(destination)
        if digest(destination) != sha:
            raise EvidenceError("archive changed during adoption")
    except BaseException:
        if created:
            destination.unlink(missing_ok=True)
        raise
    record = {"schema_version": 1, "kind": "durable_external_archive",
              "archive": file_identity(destination), "original": str(archive),
              "storage": "independent_hardlink", "signature_status": "unsigned",
              "status": "all_members_read", **measured,
              "scope": "archive bytes and safe member closure; inner reports retain their own qualification"}
    atomic_json(storage / (sha + ".external.json"), record)
    return record


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("operation", choices=("export", "verify", "restore", "adopt"))
    parser.add_argument("--source", type=Path)
    parser.add_argument("--store", type=Path)
    parser.add_argument("--package", type=Path)
    parser.add_argument("--destination", type=Path)
    parser.add_argument("--archive", type=Path,
                        help="Explicit archive navigation after transport; expected identities stay unchanged")
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        separate_outputs([args.report],
                         [path for path in (args.source, args.package) if path is not None],
                         [path for path in (args.source, args.destination)
                          if path is not None and (path.is_dir() or path == args.destination)])
        if args.archive and args.operation in ("export", "adopt"):
            parser.error("--archive is only for verify/restore")
        if args.operation == "export":
            if args.source is None or args.store is None:
                parser.error("export requires --source and --store")
            result = export_tree(args.source, args.store)
        elif args.operation == "adopt":
            if args.source is None or args.store is None:
                parser.error("adopt requires --source and --store")
            result = adopt(args.source, args.store)
        else:
            if args.package is None:
                parser.error("verify/restore requires --package")
            package = load_json(args.package)
            separate_outputs([args.report],
                             [args.archive or Path(package["archive"]["path"])])
            if args.operation == "restore":
                if args.destination is None:
                    parser.error("restore requires --destination")
                result = restore_tree(package, args.destination, archive_override=args.archive)
            else:
                result = (verify_external(package, archive_override=args.archive) if
                          package.get("kind") == "durable_external_archive" else
                          verify_package(package, archive_override=args.archive))
        if "archive" in result:
            separate_outputs([args.report], [Path(result["archive"]["path"])])
        atomic_json(args.report, result)
        return 0
    except (EvidenceError, OSError, ValueError, tarfile.TarError) as error:
        print("Evidence storage rejected: " + str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
