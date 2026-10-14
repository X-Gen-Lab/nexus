#!/usr/bin/env python3
"""Verify a prepared Pages tree against its trusted, external byte inventory.

The delivery branch supplies this verifier and publication.json. Their trust is
repository access and review; hashes do not authenticate an author or sign a
release. The documentation source commit is independent of the delivery branch.
"""
from __future__ import annotations

import argparse
from collections.abc import Mapping
import hashlib
from html.parser import HTMLParser
import json
from pathlib import Path, PurePosixPath
import re
import stat
import sys
from urllib.parse import unquote, urlsplit

SCRIPT = Path(__file__).resolve()
ROOT = (SCRIPT.parent.parent if SCRIPT.parent.name == "verifier"
        else SCRIPT.parents[2])
sys.path.insert(0, str(ROOT))
from tools.configure.metadata import parse_json
from tools.documentation import delivery

MAX_FILES = 200000
MAX_BYTES = 4 * 1024 * 1024 * 1024
MAX_RECORD_BYTES = 64 * 1024 * 1024
FIELDS = {"schema_version", "kind", "release_id", "documentation_commit",
          "manifest_sha256", "archive_sha256", "archive_path", "files"}


def _hash(contents):
    return hashlib.sha256(contents).hexdigest()


def _sha(value):
    if not isinstance(value, str) or not re.fullmatch(r"[0-9a-f]{64}", value):
        raise ValueError("Publication requires a lowercase SHA-256")
    return value


def _relative(value):
    if (not isinstance(value, str) or not value or "\\" in value or
            ":" in value or any(ord(c) < 32 for c in value)):
        raise ValueError("Invalid publication inventory path")
    path = PurePosixPath(value)
    if (path.is_absolute() or path.as_posix() != value or
            any(p in {"", ".", ".."} for p in value.split("/"))):
        raise ValueError("Publication path escape rejected")
    return value


def _plain(path, *, directory=False):
    path = Path(path).absolute()
    if ".." in path.parts:
        raise ValueError("Publication path escape rejected")
    if any(p.is_symlink() for p in (path, *path.parents)):
        raise ValueError("Publication path traverses a symlink")
    if not (path.is_dir() if directory else path.is_file()):
        raise ValueError("Missing publication input: " + str(path))
    return path


def file_inventory(public_root):
    """Observe every regular file, including old versions and download bytes."""
    root = _plain(public_root, directory=True)
    records = []
    total = 0
    for path in sorted(root.rglob("*")):
        if path.is_symlink():
            raise ValueError("Publication contains a symlink")
        mode = path.stat().st_mode
        if stat.S_ISDIR(mode):
            continue
        if not stat.S_ISREG(mode):
            raise ValueError("Publication contains a nonregular file")
        relative = _relative(path.relative_to(root).as_posix())
        size = path.stat().st_size
        total += size
        if total > MAX_BYTES or len(records) >= MAX_FILES:
            raise ValueError("Publication exceeds its byte/file bound")
        with path.open("rb") as stream:
            digest = hashlib.file_digest(stream, "sha256").hexdigest()
        records.append({"path": relative, "size": size, "sha256": digest})
    if not records:
        raise ValueError("Publication inventory is empty")
    return records


def _validate_record(record):
    if not isinstance(record, dict) or set(record) != FIELDS:
        raise ValueError("Invalid publication record fields")
    if (type(record["schema_version"]) is not int or
            record["schema_version"] != 1 or
            record["kind"] != "nexus_documentation_publication"):
        raise ValueError("Invalid publication record schema")
    release_id = record["release_id"]
    if (not isinstance(release_id, str) or not re.fullmatch(
            r"[A-Za-z0-9][A-Za-z0-9._-]{0,127}", release_id)):
        raise ValueError("Invalid active publication release ID")
    if (not isinstance(record["documentation_commit"], str) or
            not re.fullmatch(r"[0-9a-f]{40}", record["documentation_commit"])):
        raise ValueError("Invalid documentation source commit")
    _sha(record["manifest_sha256"])
    _sha(record["archive_sha256"])
    if _relative(record["archive_path"]) != f"downloads/{release_id}.tar.gz":
        raise ValueError("Publication archive must match the active release")
    entries = record["files"]
    if not isinstance(entries, list) or not 0 < len(entries) <= MAX_FILES:
        raise ValueError("Invalid publication file inventory")
    paths = set()
    total = 0
    for entry in entries:
        if (not isinstance(entry, dict) or
                set(entry) != {"path", "size", "sha256"}):
            raise ValueError("Invalid publication inventory entry")
        path = _relative(entry["path"])
        if path in paths:
            raise ValueError("Duplicate publication inventory path")
        paths.add(path)
        if type(entry["size"]) is not int or entry["size"] < 0:
            raise ValueError("Invalid publication inventory size")
        total += entry["size"]
        _sha(entry["sha256"])
    if total > MAX_BYTES:
        raise ValueError("Publication inventory exceeds its byte bound")
    return {entry["path"]: entry for entry in entries}


def _checked_bytes(root, relative, entries):
    path = _plain(root / _relative(relative))
    if relative not in entries:
        raise ValueError("Publication inventory lacks: " + relative)
    content = path.read_bytes()
    expected = entries[relative]
    if len(content) != expected["size"] or _hash(content) != expected["sha256"]:
        raise ValueError("Publication bytes changed: " + relative)
    return content


def _checked_digest(root, relative, entries):
    """Stream archives instead of retaining another compressed payload copy."""
    path = _plain(root / _relative(relative))
    if relative not in entries:
        raise ValueError("Publication inventory lacks: " + relative)
    with path.open("rb") as stream:
        digest = hashlib.file_digest(stream, "sha256").hexdigest()
    expected = entries[relative]
    if path.stat().st_size != expected["size"] or digest != expected["sha256"]:
        raise ValueError("Publication bytes changed: " + relative)
    return digest


class _MarkupPayload(Mapping):
    """Reuse the shared offline validator without loading binary downloads."""
    def __init__(self, root, entries):
        self.root = root
        self.entries = entries

    def __iter__(self):
        return iter(self.entries)

    def __len__(self):
        return len(self.entries)

    def __contains__(self, name):
        return name in self.entries

    def __getitem__(self, name):
        if name not in self.entries:
            raise KeyError(name)
        if name.endswith((".html", ".css", ".svg")):
            return _checked_bytes(self.root, name, self.entries)
        # The shared validator needs nonmarkup membership, not its contents.
        # Inventory and archive validation independently bind those bytes.
        return b""


class _EntryPage(HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.links = []
        self.text = []

    def handle_starttag(self, tag, attrs):
        if tag.lower() == "a":
            self.links.extend(value for key, value in attrs
                              if key.lower() == "href" and value)

    def handle_data(self, data):
        self.text.append(data)


def _entry(root, relative, entries, required, release_id=None, *, stable=False):
    content = _checked_bytes(root, relative, entries).decode("utf-8")
    page = _EntryPage()
    page.feed(content)
    targets = set()
    for link in page.links:
        url = urlsplit(link)
        if url.scheme or url.netloc or url.path.startswith("/"):
            raise ValueError("Publication entry link must remain local")
        path = (root / relative).parent / unquote(url.path)
        resolved = path.resolve()
        if not resolved.is_relative_to(root) or not resolved.is_file():
            raise ValueError("Publication entry link escapes or lacks target")
        target = resolved.relative_to(root).as_posix()
        targets.add(target)
        if stable and target.startswith("versions/"):
            if target.split("/")[1] != release_id:
                raise ValueError("Stable selects a different active version")
        if stable and target.endswith(".tar.gz"):
            if target != f"downloads/{release_id}.tar.gz":
                raise ValueError("Stable download differs from active version")
    if not required.issubset(targets):
        raise ValueError("Publication entry lacks active targets: " + relative)
    if release_id:
        text = " ".join(" ".join(page.text).split())
        if not re.search(r"\bVersion:\s+" + re.escape(release_id) +
                         r"(?=\s|$)", text):
            raise ValueError("Entry reports a different active version")


def _layout(root, record, entries):
    release_id = record["release_id"]
    prefix = f"versions/{release_id}/"
    active_targets = {prefix + path for path in
                      ("en/index.html", "zh_CN/index.html", "api/index.html",
                       delivery.MANIFEST)}
    _entry(root, "stable/index.html", entries,
           active_targets | {record["archive_path"]}, release_id, stable=True)
    _entry(root, "index.html", entries,
           active_targets | {"stable/index.html", "downloads/index.html"},
           release_id)
    _entry(root, "downloads/index.html", entries, {record["archive_path"]})
    versions = _plain(root / "versions", directory=True)
    downloads = _plain(root / "downloads", directory=True)
    manifests = {}
    for version in sorted(versions.iterdir()):
        _plain(version, directory=True)
        name = version.relative_to(root).as_posix() + "/" + delivery.MANIFEST
        manifest_digest = _hash(_checked_bytes(root, name, entries))
        manifest = delivery.verify(version,
                                   expected_manifest_sha256=manifest_digest)
        if manifest["release_id"] != version.name:
            raise ValueError("Version directory differs from manifest")
        manifests[version.name] = (manifest, manifest_digest)
    if release_id not in manifests:
        raise ValueError("Active publication version missing")
    active, digest = manifests[release_id]
    if (digest != record["manifest_sha256"] or
            active["documentation_source"]["commit"] !=
            record["documentation_commit"]):
        raise ValueError("Active publication manifest or source anchor differs")
    archives = list(downloads.glob("*.tar.gz"))
    if not archives:
        raise ValueError("Publication archive missing")
    for archive in archives:
        _plain(archive)
        version_id = archive.name[:-len(".tar.gz")]
        if version_id not in manifests:
            raise ValueError("Publication archive has no verified version")
        relative = archive.relative_to(root).as_posix()
        archive_digest = _checked_digest(root, relative, entries)
        expected, manifest_digest = manifests[version_id]
        observed = delivery.verify(archive,
                                   expected_manifest_sha256=manifest_digest)
        if observed != expected:
            raise ValueError("Archive and version identities differ")
        if (version_id == release_id and
                archive_digest != record["archive_sha256"]):
            raise ValueError("Active archive digest differs")
    _plain(root / record["archive_path"])


def verify(public_root, record):
    """Reject unchecked bytes and stale navigation before Pages upload."""
    root = _plain(public_root, directory=True)
    entries = _validate_record(record)
    observed = {e["path"]: e for e in file_inventory(root)}
    if observed != entries:
        raise ValueError("Publication inventory differs from public bytes")
    _layout(root, record, entries)
    offline_links = delivery._offline(_MarkupPayload(root, entries))
    if {e["path"]: e for e in file_inventory(root)} != entries:
        raise ValueError("Publication inventory changed during verification")
    return {"status": "verified", "release_id": record["release_id"],
            "documentation_commit": record["documentation_commit"],
            "manifest_sha256": record["manifest_sha256"],
            "archive_sha256": record["archive_sha256"],
            "public_files": len(entries), "offline_links": offline_links,
            "physical_status": "not_executed"}


def create_record(public_root, package_result):
    """Bind a prepared publication to the independently produced package."""
    manifest_digest = _sha(package_result["manifest_sha256"])
    archive_digest = _sha(package_result["archive_sha256"])
    original = delivery.verify(package_result["directory"],
                               expected_manifest_sha256=manifest_digest)
    archive = _plain(package_result["archive"])
    with archive.open("rb") as stream:
        actual_digest = hashlib.file_digest(stream, "sha256").hexdigest()
    if actual_digest != archive_digest:
        raise ValueError("Original package archive digest differs")
    archived = delivery.verify(archive,
                               expected_manifest_sha256=manifest_digest)
    if (original != archived or
            original["release_id"] != package_result["release_id"]):
        raise ValueError("Original package identity differs from publication")
    record = {"schema_version": 1, "kind": "nexus_documentation_publication",
              "release_id": original["release_id"],
              "documentation_commit": original["documentation_source"][
                  "commit"],
              "manifest_sha256": manifest_digest,
              "archive_sha256": archive_digest,
              "archive_path": f'downloads/{original["release_id"]}.tar.gz',
              "files": file_inventory(public_root)}
    verify(public_root, record)
    return record


def main(arguments=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("public_root", type=Path)
    parser.add_argument("publication", type=Path)
    args = parser.parse_args(arguments)
    try:
        root = _plain(args.public_root, directory=True)
        source = _plain(args.publication)
        if source.is_relative_to(root):
            raise ValueError("Publication record must stay outside public")
        if source.stat().st_size > MAX_RECORD_BYTES:
            raise ValueError("Publication record exceeds its byte bound")
        record = parse_json(source.read_bytes())
        print(json.dumps(verify(root, record), indent=2))
        return 0
    except (OSError, ValueError, KeyError, TypeError) as error:
        print("Publication verification rejected: " + str(error),
              file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
