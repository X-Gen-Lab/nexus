#!/usr/bin/env python3
"""Verify the curated TLS source graph and its finite static SDK closure."""
from __future__ import annotations

import argparse
import configparser
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
if __name__ == "__main__":
    sys.dont_write_bytecode = True
from tools.configure.metadata import fields, parse_json, snapshot  # noqa: E402
from tools.maintenance import source_catalogue as catalogue  # noqa: E402
from tools.maintenance.lwip_lock import git  # noqa: E402

LOCK_NAME = "dependencies/mbedtls.lock.json"
LOCK = ROOT / LOCK_NAME
COMMIT = "d840a13e087a8742d5a060a4a4a42a33ac41e3ab"
TREE = "e172b547acb578fd0c1f937786d94a07a3b15111"
REVIEWED_LOCK_SHA256 = (
    "9cb0d21de1a525e569f7fe8a629572349ebd15710845f0999b2258c326b1e713")


def mounts():
    """The existing catalogue, including aliases, owns the source graph."""
    return tuple(item for item in catalogue.ALL_DEPENDENCIES
                 if item.source_lock == LOCK_NAME)


def record(lock, identifier_or_path):
    """Return one canonical lock record; aliases share its source identity."""
    item = catalogue.dependency(identifier_or_path)
    if item.source_lock != LOCK_NAME:
        raise ValueError("TLS record requires a selected TLS dependency")
    return lock["repositories"][item.alias_of or item.identifier]


def contract(lock):
    """Require the reviewed pins, per-file notices and generated materials."""
    fields(lock, {"schema", "name", "version", "admission", "generation",
                  "generated_archive", "repositories"}, "TLS source lock")
    if (type(lock["schema"]) is not int or lock["schema"] != 1 or
            lock["name"] != "mbedtls" or lock["version"] != "4.1.1" or
            lock["admission"] != "organization_gitlink" or
            lock["generation"] != "GEN_FILES=OFF"):
        raise ValueError("Exact reviewed TLS source lock required")
    expected = {"mbedtls", "tf_psa_crypto", "mbedtls_framework"}
    repositories = lock["repositories"]
    if not isinstance(repositories, dict) or set(repositories) != expected:
        raise ValueError("TLS requires its exact canonical repository records")
    for value in repositories.values():
        fields(value, {"version", "commit", "tree", "upstream_commit",
                       "license", "generated_files", "files"},
               "TLS repository lock")
        if (not isinstance(value["files"], dict) or
                "LICENSE" not in value["files"] or
                not isinstance(value["generated_files"], list) or
                not set(value["generated_files"]).issubset(value["files"])):
            raise ValueError("TLS source and generated/license closure required")
        for name, digest in value["files"].items():
            if (not isinstance(name, str) or not name or "\\" in name or
                    PurePosixPath(name).is_absolute() or
                    any(part in ("", ".", "..") for part in name.split("/")) or
                    not isinstance(digest, str) or
                    re.fullmatch(r"[0-9a-f]{64}", digest) is None):
                raise ValueError("Invalid TLS source file identity")
    encoded = json.dumps(lock, sort_keys=True, separators=(",", ":")).encode()
    if hashlib.sha256(encoded).hexdigest() != REVIEWED_LOCK_SHA256:
        raise ValueError("TLS reviewed source lock and byte closure differ")
    return lock


def load_lock(lock_path=LOCK):
    return contract(parse_json(snapshot(lock_path)[0]))


def _directory(path):
    if (not path.is_dir() or path.resolve() != path or
            any(item.is_symlink() for item in (path, *path.parents))):
        raise ValueError("TLS source requires canonical regular directories")


def _top_declaration(root, item):
    raw, identity = snapshot(root / ".gitmodules")
    parser = configparser.ConfigParser(interpolation=None)
    try:
        parser.read_string(raw.decode())
        section = 'submodule "' + catalogue.module_name(item) + '"'
        if (parser.defaults() or dict(parser[section]) != {
                "path": item.path, "url": item.fetch_url}):
            raise ValueError("TLS organization declaration differs")
    except (configparser.Error, UnicodeError, KeyError) as error:
        raise ValueError("Invalid TLS organization declaration") from error
    return identity


def verify(source, lock_path=LOCK, *, development=False, source_sdk=False,
           dependency_records=None):
    """Prove selected sources; complete SDK authentication stays with its owner.

    Development permits an explicitly located exact curated checkout. It never
    receives organization-gitlink admission. SDK scope requires the owning
    verifier's exact dependency records, rather than inventing Git metadata.
    """
    if development and source_sdk:
        raise ValueError("TLS development and SDK scopes are exclusive")
    source, lock_path = Path(source).absolute(), Path(lock_path).absolute()
    root = lock_path.parents[1]
    raw, lock_identity = snapshot(lock_path)
    lock = contract(parse_json(raw))
    main = catalogue.dependency("mbedtls")
    if not development and (source != root / main.path or
                            lock_path != root / LOCK_NAME):
        raise ValueError("TLS production source requires canonical placement")
    selected = mounts()
    directories = {item.path: source / PurePosixPath(item.path).relative_to(
        main.path) for item in selected}
    for directory in directories.values():
        _directory(directory)
    metadata = {}
    if not development and not source_sdk:
        metadata[root / ".gitmodules"] = _top_declaration(root, main)
    if source_sdk:
        if not isinstance(dependency_records, list):
            raise ValueError("TLS SDK requires observed dependency records")
        for item in selected:
            matches = [row for row in dependency_records
                       if isinstance(row, dict) and row.get("path") == item.path]
            expected = record(lock, item.identifier)
            if (len(matches) != 1 or matches[0].get("commit") != expected["commit"]
                    or matches[0].get("tree") != expected["tree"]):
                raise ValueError("TLS SDK dependency identity differs")
        catalogue.verify_alias_identities(dependency_records)

    def observe():
        observed = []
        for item in selected:
            directory = directories[item.path]
            expected = record(lock, item.identifier)
            if not source_sdk:
                marker = directory / ".git"
                if (not marker.exists() or marker.is_symlink() or
                        Path(git(directory, "rev-parse", "--show-toplevel")) != directory
                        or git(directory, "rev-parse", "HEAD") != expected["commit"]
                        or git(directory, "rev-parse", "HEAD^{tree}") != expected["tree"]):
                    raise ValueError("TLS checkout commit/tree differs")
                allowed = ((item.fetch_url, item.upstream_url) if development
                           else (item.fetch_url,))
                if git(directory, "config", "--get-all", "remote.origin.url") not in allowed:
                    raise ValueError("TLS organization origin differs")
                if git(directory, "status", "--porcelain", "--untracked-files=all"):
                    raise ValueError("TLS checkout is dirty")
                if not development:
                    owner = root if item.parent is None else directories[item.parent]
                    relative = catalogue.relative_path(item)
                    wanted = "160000 " + expected["commit"] + " 0\t" + relative
                    if git(owner, "ls-files", "--stage", "--", relative,
                           parent_index=owner == root) != wanted:
                        raise ValueError("TLS owning parent Gitlink differs")
            descendants = catalogue.children(item.path)
            if descendants:
                module_file = directory / ".gitmodules"
                declaration, identity = snapshot(module_file)
                catalogue.declared_modules(declaration, item.path)
                previous = metadata.setdefault(module_file, identity)
                if previous != identity:
                    raise ValueError("TLS source declaration changed")
                if not source_sdk:
                    links = {}
                    for row in git(directory, "ls-tree", "-r", "HEAD").splitlines():
                        header, name = row.split("\t")
                        if header.startswith("160000 commit "):
                            links[name] = header.split()[2]
                    catalogue.verify_gitlinks(links, item.path)
                    for child in catalogue.children(item.path):
                        if links[catalogue.relative_path(child)] != record(
                                lock, child.identifier)["commit"]:
                            raise ValueError("TLS selected child Gitlink differs")
            observed.append({"path": item.path, "commit": expected["commit"],
                             "tree": expected["tree"]})
        catalogue.verify_alias_identities(observed)

    observe()
    identities = {}
    for item in selected:
        directory = directories[item.path]
        files = record(lock, item.identifier)["files"]
        for name, digest in files.items():
            path = directory / name
            identity = snapshot(path)[1]
            if identity["sha256"] != digest:
                raise ValueError("TLS source bytes differ: " + item.path + "/" + name)
            identities[path] = identity
        if source_sdk:
            descendants = [directory / catalogue.relative_path(child)
                           for child in catalogue.children(item.path)]
            actual = {path.relative_to(directory).as_posix()
                      for path in directory.rglob("*") if path.is_file()
                      and not any(path.is_relative_to(child) for child in descendants)}
            if actual != set(files):
                raise ValueError("TLS SDK payload is not the reviewed file closure")
    for path, identity in {**metadata, **identities, lock_path: lock_identity}.items():
        if snapshot(path)[1] != identity:
            raise ValueError("TLS inputs changed during observation")
    observe()
    return {"name": "mbedtls", "version": "4.1.1", "commit": COMMIT,
            "tree": TREE, "verified_mounts": len(selected),
            "verified_files": len(identities),
            "generation": "GEN_FILES=OFF", "scope": (
                "explicit_development_only" if development else
                "source_sdk_payload" if source_sdk else "organization_gitlink"),
            "production_admitted": not development,
            "software_qualified": False, "hardware_status": "not_executed",
            "configure_inputs": [str(lock_path), *map(str, metadata),
                                 *map(str, identities)]}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--lock", type=Path, default=LOCK)
    scope = parser.add_mutually_exclusive_group()
    scope.add_argument("--development", action="store_true")
    scope.add_argument("--source-sdk", action="store_true")
    args = parser.parse_args(argv)
    try:
        records = None
        before = None
        manifest = args.lock.absolute().parents[1] / ".nexus-source-sdk.json"
        if args.source_sdk:
            before = snapshot(manifest)[1]
            sys.path.insert(0, str(manifest.parent / "cmake/package"))
            import package_source_sdk
            records = package_source_sdk.verify(manifest.parent)["dependencies"]
        result = verify(args.source, args.lock, development=args.development,
                        source_sdk=args.source_sdk, dependency_records=records)
        if before is not None and snapshot(manifest)[1] != before:
            raise ValueError("TLS SDK manifest changed during proof")
    except (OSError, ValueError, TypeError, KeyError, ImportError,
            subprocess.SubprocessError) as error:
        print("TLS source rejected: " + str(error), file=sys.stderr)
        return 1
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
