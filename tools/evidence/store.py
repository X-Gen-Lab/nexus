#!/usr/bin/env python3
"""Portable immutable evidence archives with complete byte verification."""

from __future__ import annotations

import argparse
from contextlib import contextmanager
import gzip
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import stat
import sys
import tarfile
import tempfile
import uuid

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.evidence.common import (EvidenceError, atomic_json, digest, fields,
    file_identity, load_json, regular_file, separate_outputs, sha256_value,
    stream_digest, verify_file_identity)


EXTERNAL_SCOPE = (
    "archive bytes and safe member closure; inner reports retain their own "
    "qualification")


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


def _restore_capacity(package, members, block_bytes, reserve_bytes):
    if (type(block_bytes) is not int or block_bytes < 512 or
            type(reserve_bytes) is not int or reserve_bytes < 0):
        raise EvidenceError("invalid restore capacity budget")
    files = [entry for entry in members.values() if entry["kind"] == "file"]
    directories = len(members) - len(files)
    expanded = sum(entry["size"] for entry in files)
    if (type(package["files"]) is not int or package["files"] != len(files) or
            type(package["bytes"]) is not int or package["bytes"] != expanded):
        raise EvidenceError("restore capacity envelope differs from manifest")
    allocated = sum((entry["size"] + block_bytes - 1) // block_bytes * block_bytes
                    for entry in files) + directories * block_bytes
    metadata = 2 * block_bytes * (len(members) + 2)
    return {"required_bytes": allocated + metadata + reserve_bytes,
            "expanded_bytes": expanded, "allocated_payload_bytes": allocated,
            "metadata_reserve_bytes": metadata, "files": len(files),
            "directories": directories}


def restore_capacity(package: dict, *, archive_override: Path | None = None,
                     block_bytes: int = 4096, reserve_bytes: int = 0) -> dict:
    """Estimate one restore's allocation from the original complete manifest.

    The caller selects the target filesystem block size and additional peak
    reserve, including any nested validator restoration. This is an observation
    for admission, not disk reservation or tmpfs memory qualification.
    """
    _, members = _members(package, archive_override)
    return _restore_capacity(package, members, block_bytes, reserve_bytes)


def _recovery_targets(destination, names):
    payload = destination / "payload"
    if payload.exists() and (payload.is_symlink() or not payload.is_dir() or
            any(path.name not in names for path in payload.iterdir())):
        raise EvidenceError("unselected or unsafe recovery payload targets")


def _recovery_previous_active(previous, destination):
    active = previous.get("active")
    if active is None:
        return
    fields(active, {"name", "argv", "command_view"})
    view = Path(active["command_view"])
    if (view.parent != destination / "attempts" / previous.get("attempt", "") or
            view.name != active["name"] + ".command.json" or not view.is_file()):
        raise EvidenceError("unfinished original command requires withdrawal review")
    receipt = load_json(view)
    original = Path(receipt.get("attempt_receipt", ""))
    if (receipt.get("outcome") == "running" or
            type(receipt.get("exit_code")) is not int or
            receipt.get("withdrawal_complete") is not True or
            receipt.get("withdrawal_stable") is not True):
        raise EvidenceError("unknown original command withdrawal; resume rejected")
    if (receipt.get("command") != active["argv"] or
            original.name != "receipt.json" or original.parent.parent !=
            view.parent / (active["name"] + ".attempts") or
            load_json(original) != {key: value for key, value in receipt.items()
                                    if key != "attempt_receipt"}):
        raise EvidenceError("original recovery command identity changed")
    from tools.dev import timings
    timings.summarize([original])


def recover_packages(plan: dict, destination: Path, *, resume: bool = False) -> dict:
    """Restore a fixed local package plan, or reverify its existing byte closure.

    Recovery continues artifact work only. It never reuses test success, runs an
    inner report as code, or promotes an unknown original command exit. Every
    invocation executes a fresh owned store command and rechecks all members.
    Original unique command attempts and the previous journal remain retained.
    Capacity admission is a conservative observation, not a reservation.
    """
    from tools.evidence.identity import canonical_digest, observe_git_source

    fields(plan, {"schema_version", "kind", "reserve_bytes", "packages"})
    if (type(plan["schema_version"]) is not int or plan["schema_version"] != 1 or
            plan["kind"] != "evidence_recovery_plan" or
            type(plan["reserve_bytes"]) is not int or plan["reserve_bytes"] < 0 or
            not isinstance(plan["packages"], list) or
            not 1 <= len(plan["packages"]) <= 128):
        raise EvidenceError("invalid evidence recovery plan")
    destination = _safe_directory(destination)
    prepared, names = [], set()
    for entry in plan["packages"]:
        fields(entry, {"name", "package", "archive"})
        from tools.evidence.common import identifier
        name = identifier(entry["name"], "recovery package name")
        if name in names or ":" in name or name in (".", ".."):
            raise EvidenceError("duplicate or unsafe recovery package name")
        names.add(name)
        envelope = verify_file_identity(entry["package"])
        archive = verify_file_identity(entry["archive"])
        if any(path == destination or destination in path.parents
               for path in (envelope, archive)):
            raise EvidenceError("recovery inputs cannot belong to its output tree")
        package = load_json(envelope)
        _, members = _members(package, archive)
        if any(package["archive"][key] != entry["archive"][key]
               for key in ("sha256", "size")):
            raise EvidenceError("recovery archive differs from package identity")
        prepared.append((entry, envelope, archive, package, members))
    tools_root = Path(__file__).resolve().parents[2]
    source = observe_git_source(tools_root)
    fingerprint = canonical_digest(plan)
    journal = destination / "recovery.json"
    previous = None
    if destination.exists():
        if not resume:
            raise EvidenceError("existing recovery output requires explicit resume")
        previous = load_json(journal)
        if (previous.get("kind") != "evidence_recovery" or
                previous.get("source") != source or
                previous.get("plan_sha256") != fingerprint or
                previous.get("destination") != str(destination)):
            raise EvidenceError("recovery source or plan fingerprint changed")
    elif resume:
        raise EvidenceError("resume requires the original recovery output")
    _recovery_targets(destination, names)
    if previous is not None:
        _recovery_previous_active(previous, destination)

    needed = plan["reserve_bytes"] + 1024 * 1024 * len(prepared)
    for entry, _, _, package, members in prepared:
        target = destination / "payload" / entry["name"]
        if target.exists():
            if inventory(target) != list(members.values()):
                raise EvidenceError("existing recovery members differ from manifest")
        else:
            needed += _restore_capacity(package, members, 4096, 0)["required_bytes"]
    ancestor = destination
    while not ancestor.exists():
        ancestor = ancestor.parent
    free = shutil.disk_usage(ancestor).free
    if free < needed:
        raise EvidenceError(f"recovery capacity insufficient: need {needed}, free {free}")
    with _recovery_lock(destination):
        # A different coordinator may have completed between observation and
        # lock acquisition. Re-read its journal; never overwrite another plan.
        if journal.exists():
            if not resume:
                raise EvidenceError("existing recovery output requires explicit resume")
            previous = load_json(journal)
            if (previous.get("source") != source or
                    previous.get("plan_sha256") != fingerprint or
                    previous.get("kind") != "evidence_recovery" or
                    previous.get("destination") != str(destination)):
                raise EvidenceError("recovery source or plan fingerprint changed")
            _recovery_previous_active(previous, destination)
        _recovery_targets(destination, names)
        return _recover_admitted(prepared, destination, source, fingerprint,
                                 needed, free, plan["reserve_bytes"], previous)


@contextmanager
def _recovery_lock(destination: Path):
    """Hold one POSIX coordinator lock; process death releases the kernel lock."""
    try:
        import fcntl
    except ImportError as error:
        raise EvidenceError("recovery coordinator requires POSIX file locking") from error
    destination.mkdir(parents=True, exist_ok=True)
    descriptor = os.open(destination / ".recovery.lock",
                         os.O_RDWR | os.O_CREAT | os.O_NONBLOCK |
                         getattr(os, "O_NOFOLLOW", 0), 0o600)
    try:
        if not stat.S_ISREG(os.fstat(descriptor).st_mode):
            raise EvidenceError("recovery lock must be a regular file")
        try:
            fcntl.flock(descriptor, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError as error:
            raise EvidenceError("another recovery coordinator is active") from error
        yield
    finally:
        os.close(descriptor)


def _recover_admitted(prepared, destination, source, fingerprint, needed, free,
                      reserve, previous):
    from tools.dev import process
    from tools.evidence.identity import observe_git_source

    tools_root = Path(__file__).resolve().parents[2]
    journal = destination / "recovery.json"
    attempt = uuid.uuid4().hex
    control = destination / "attempts" / attempt
    control.mkdir(parents=True)
    previous_identity = None
    if previous is not None:
        shutil.copy2(journal, control / "previous.json")
        previous_identity = file_identity(control / "previous.json")
    state = {"schema_version": 1, "kind": "evidence_recovery",
             "source": source, "plan_sha256": fingerprint,
             "destination": str(destination), "attempt": attempt,
             "previous": previous_identity, "status": "running", "packages": [],
             "active": None,
             "capacity": {"required_bytes": needed, "observed_free_bytes": free,
                          "reserve_bytes": reserve},
             "software_qualified": False, "physical_status": "not_executed"}
    atomic_json(journal, state)
    try:
        for entry, envelope, archive, package, members in prepared:
            # Reobserve immediately before each side effect. No checkpoint can
            # authorize changed inputs, even when earlier packages completed.
            verify_file_identity(entry["package"])
            verify_file_identity(entry["archive"])
            if observe_git_source(tools_root) != source:
                raise EvidenceError("recovery verifier source changed")
            target = destination / "payload" / entry["name"]
            existing = target.exists()
            remaining = reserve + 1024 * 1024
            if not existing:
                remaining += _restore_capacity(package, members, 4096, 0)["required_bytes"]
            if shutil.disk_usage(destination).free < remaining:
                raise EvidenceError("recovery capacity changed before restore")
            operation = "verify" if existing else "restore"
            report = control / (entry["name"] + ".json")
            argv = [sys.executable, "-B", str(Path(__file__).resolve()), operation,
                    "--package", str(envelope), "--archive", str(archive),
                    "--report", str(report)]
            if not existing:
                argv += ["--destination", str(target)]
            log = control / (entry["name"] + ".log")
            state["active"] = {"name": entry["name"], "argv": argv,
                               "command_view": str(log.with_suffix(".command.json"))}
            atomic_json(journal, state)
            result = process.run(argv, cwd=tools_root, env=os.environ.copy(),
                                 log=log,
                                 timeout=1800, stage="evidence_recovery")
            if result.returncode:
                raise EvidenceError("owned recovery command returned nonzero")
            if inventory(target) != list(members.values()):
                raise EvidenceError("recovered members differ from manifest")
            verified = load_json(report)
            if (verified.get("status") != "verified" or
                    verified.get("archive_sha256") != package["archive"]["sha256"] or
                    verified.get("files") != package["files"] or
                    verified.get("bytes") != package["bytes"]):
                raise EvidenceError("owned recovery report differs from package")
            state["packages"].append({"name": entry["name"],
                "operation": "existing_members_reverified" if existing else "restored",
                "package": entry["package"], "archive": entry["archive"],
                "report": file_identity(report),
                "receipt": file_identity(result.receipt_path),
                "manifest_sha256": package["manifest_sha256"]})
            state["active"] = None
            atomic_json(journal, state)
        for entry, _, _, _, _ in prepared:
            verify_file_identity(entry["package"])
            verify_file_identity(entry["archive"])
        if observe_git_source(tools_root) != source:
            raise EvidenceError("recovery verifier source changed")
        state["status"] = "recovered_verified"
        atomic_json(journal, state)
        return state
    except BaseException:
        state["status"] = "failed"
        atomic_json(journal, state)
        raise


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
                    "member_inventory_sha256", "scope"},
           {"persistence_status"})
    if (type(record["schema_version"]) is not int or
            record["schema_version"] != 1 or record["kind"] not in
            ("preserved_external_archive", "durable_external_archive")):
        raise EvidenceError("unsupported external archive receipt")
    if (record["signature_status"] != "unsigned" or
            record["status"] != "all_members_read" or
            record["scope"] != EXTERNAL_SCOPE or
            record["storage"] not in ("independent_hardlink", "independent_copy") or
            not isinstance(record["original"], str) or not record["original"] or
            any(type(record[key]) is not int or record[key] <= 0
                for key in ("members", "files", "bytes")) or
            record["files"] > record["members"]):
        raise EvidenceError("invalid external archive preservation claims")
    if (record.get("persistence_status", "not_established") !=
            "not_established" or
            (record["kind"] == "preserved_external_archive" and
             "persistence_status" not in record)):
        raise EvidenceError("local archive verification cannot establish persistence")
    sha256_value(record["member_inventory_sha256"])
    identity = dict(record["archive"])
    if archive_override is not None:
        identity["path"] = str(archive_override.absolute())
    archive = verify_file_identity(identity)
    measured = _external_metrics(archive)
    if any(measured[key] != record[key] for key in measured):
        raise EvidenceError("durable archive member inventory changed")
    verify_file_identity(identity)
    return {"status": "verified", **measured,
            "archive_sha256": record["archive"]["sha256"],
            "persistence_status": "not_established"}


def _external_receipt(archive: Path, destination: Path, storage: Path,
                      measured: dict, method: str) -> dict:
    record = {"schema_version": 1, "kind": "preserved_external_archive",
              "archive": file_identity(destination), "original": str(archive),
              "storage": method, "signature_status": "unsigned",
              "status": "all_members_read", **measured,
              "persistence_status": "not_established", "scope": EXTERNAL_SCOPE}
    atomic_json(storage / (record["archive"]["sha256"] + ".external.json"),
                record)
    return record


def adopt(archive: Path, storage: Path) -> dict:
    """Preserve a local name; a hardlink does not establish durable storage."""
    archive = regular_file(archive)
    storage = _safe_directory(storage)
    storage.mkdir(parents=True, exist_ok=True)
    sha = digest(archive)
    destination = storage / (sha + ".tar.gz")
    created = not destination.exists()
    if destination.exists():
        if digest(destination) != sha:
            raise EvidenceError("existing preserved evidence identity changed")
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
    return _external_receipt(archive, destination, storage, measured,
                             "independent_hardlink")


def copy_archive(archive: Path, storage: Path) -> dict:
    """Copy and verify independent bytes without inferring retention guarantees."""
    archive, storage = regular_file(archive), _safe_directory(storage)
    original = file_identity(archive)
    storage.mkdir(parents=True, exist_ok=True)
    destination = storage / (original["sha256"] + ".tar.gz")
    if destination.exists() and archive.samefile(destination):
        raise EvidenceError("independent copy cannot alias the original archive")
    descriptor, name = tempfile.mkstemp(prefix=".incoming-", suffix=".tar.gz",
                                       dir=storage)
    temporary = Path(name)
    try:
        with os.fdopen(descriptor, "wb") as target, archive.open("rb") as source:
            shutil.copyfileobj(source, target, 1024 * 1024)
            target.flush()
            os.fsync(target.fileno())
        verify_file_identity({**original, "path": str(temporary)})
        measured = _external_metrics(temporary)
        verify_file_identity(original)
        try:
            # A fresh link publishes complete bytes without replacing a racing
            # writer. Its inode is independent of the original archive.
            os.link(temporary, destination)
        except FileExistsError:
            if archive.samefile(destination):
                raise EvidenceError("independent copy aliases the original archive")
        verify_file_identity({**original, "path": str(destination)})
        return _external_receipt(archive, destination, storage, measured,
                                 "independent_copy")
    finally:
        temporary.unlink(missing_ok=True)


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("operation", choices=("export", "verify", "restore",
                                              "adopt", "copy", "recover"))
    parser.add_argument("--source", type=Path)
    parser.add_argument("--store", type=Path)
    parser.add_argument("--package", type=Path)
    parser.add_argument("--plan", type=Path)
    parser.add_argument("--resume", action="store_true")
    parser.add_argument("--destination", type=Path)
    parser.add_argument("--archive", type=Path,
                        help="Explicit archive navigation after transport; expected identities stay unchanged")
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        separate_outputs([args.report],
                         [path for path in (args.source, args.package, args.plan)
                          if path is not None],
                         [path for path in (args.source, args.destination)
                          if path is not None and (path.is_dir() or path == args.destination)])
        if args.archive and args.operation in ("export", "adopt", "copy", "recover"):
            parser.error("--archive is only for verify/restore")
        if (args.resume or args.plan) and args.operation != "recover":
            parser.error("--resume/--plan are only for recover")
        if args.operation == "recover":
            if args.plan is None or args.destination is None:
                parser.error("recover requires --plan and --destination")
            plan = load_json(args.plan)
            for entry in plan.get("packages", []):
                separate_outputs([args.report], [Path(entry[key]["path"])
                                 for key in ("package", "archive")])
            result = recover_packages(plan, args.destination, resume=args.resume)
        elif args.operation == "export":
            if args.source is None or args.store is None:
                parser.error("export requires --source and --store")
            result = export_tree(args.source, args.store)
        elif args.operation in ("adopt", "copy"):
            if args.source is None or args.store is None:
                parser.error(args.operation + " requires --source and --store")
            result = (adopt if args.operation == "adopt" else copy_archive)(
                args.source, args.store)
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
                          package.get("kind") in ("durable_external_archive",
                                                  "preserved_external_archive") else
                          verify_package(package, archive_override=args.archive))
        if "archive" in result:
            separate_outputs([args.report], [Path(result["archive"]["path"])])
        atomic_json(args.report, result)
        return 0
    except (EvidenceError, OSError, ValueError, KeyError, TypeError,
            tarfile.TarError) as error:
        print("Evidence storage rejected: " + str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
