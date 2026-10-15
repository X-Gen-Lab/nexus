#!/usr/bin/env python3
"""Content-addressed downloads and explicit compiler-cache namespaces."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import sys
import tempfile
from urllib.parse import urlsplit
from urllib.request import urlopen

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.evidence.common import (EvidenceError, atomic_json, digest,
                                   file_identity, regular_file, separate_outputs, sha256_value)


def _directory(path: Path) -> Path:
    path = path.absolute()
    if any(part.is_symlink() for part in [path, *path.parents]):
        raise EvidenceError("cache directory must not contain symlinks")
    path.mkdir(parents=True, exist_ok=True)
    return path


def lookup(directory: Path, sha256: str) -> Path | None:
    """Every cache hit checks its actual bytes; names never establish identity."""
    path = _directory(directory) / sha256_value(sha256)
    if not path.exists() and not path.is_symlink():
        return None
    if digest(path) != sha256:
        raise EvidenceError("content-addressed cache entry is corrupt")
    return path


def put(source: Path, directory: Path, sha256: str) -> Path:
    source = regular_file(source)
    sha256_value(sha256)
    if digest(source) != sha256:
        raise EvidenceError("download differs from locked digest")
    destination = lookup(directory, sha256)
    if destination is not None:
        return destination
    root = _directory(directory)
    descriptor, name = tempfile.mkstemp(prefix=".incoming-", dir=root)
    temporary = Path(name)
    try:
        with os.fdopen(descriptor, "wb") as stream, source.open("rb") as incoming:
            shutil.copyfileobj(incoming, stream, 1024 * 1024)
            stream.flush()
            os.fsync(stream.fileno())
        if digest(temporary) != sha256:
            raise EvidenceError("source changed while entering download cache")
        destination = root / sha256
        # Another writer can only publish the same content-addressed bytes.
        os.replace(temporary, destination)
        return lookup(root, sha256)
    finally:
        temporary.unlink(missing_ok=True)


def fetch(url: str, directory: Path, sha256: str, *, maximum_bytes: int = 2**31,
          timeout_s: int = 60) -> Path:
    cached = lookup(directory, sha256)
    if cached is not None:
        return cached
    parsed = urlsplit(url)
    if (parsed.scheme != "https" or not parsed.hostname or parsed.username or
            parsed.password or parsed.fragment):
        raise EvidenceError("download requires credential-free HTTPS URL")
    if type(maximum_bytes) is not int or not 0 < maximum_bytes <= 2**34:
        raise EvidenceError("download size bound is invalid")
    with tempfile.TemporaryDirectory(prefix=".download-", dir=_directory(directory)) as name:
        incoming = Path(name) / "payload"
        with urlopen(url, timeout=timeout_s) as response, incoming.open("wb") as output:
            final = urlsplit(response.geturl())
            if final.scheme != "https" or final.username or final.password:
                raise EvidenceError("download redirect violates HTTPS policy")
            size = 0
            for block in iter(lambda: response.read(1024 * 1024), b""):
                size += len(block)
                if size > maximum_bytes:
                    raise EvidenceError("download exceeds locked size bound")
                output.write(block)
        return put(incoming, directory, sha256)


def compiler_namespace(compiler: Path, flags: list[str],
                       dependencies: list[Path], *, environment: dict | None = None) -> str:
    """ccache still hashes arguments/source/includes; namespace binds policies."""
    if not isinstance(flags, list) or any(not isinstance(flag, str) for flag in flags):
        raise EvidenceError("compiler flags must be an explicit string list")
    inputs = [file_identity(path) for path in dependencies]
    identity = {"compiler_sha256": digest(compiler), "flags": flags,
                "dependencies": [{"sha256": item["sha256"], "size": item["size"]}
                                 for item in inputs],
                "environment": environment or {}}
    encoded = json.dumps(identity, sort_keys=True, separators=(",", ":"),
                         allow_nan=False).encode()
    return hashlib.sha256(encoded).hexdigest()


def compiler_environment(directory: Path, namespace: str, *, formal: bool = False) -> dict:
    if formal:
        raise EvidenceError("formal independent rebuilds cannot use compiler cache")
    return {"CCACHE_DIR": str(_directory(directory)),
            "CCACHE_NAMESPACE": sha256_value(namespace),
            "CCACHE_COMPILERCHECK": "content", "CCACHE_SLOPPINESS": "",
            "CCACHE_HASHDIR": "true"}


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    actions = parser.add_subparsers(dest="operation", required=True)
    for operation in ("put", "fetch"):
        child = actions.add_parser(operation)
        child.add_argument("--cache", type=Path, required=True)
        child.add_argument("--sha256", required=True)
        child.add_argument("--file" if operation == "put" else "--url", required=True)
    child = actions.add_parser("namespace")
    child.add_argument("--compiler", type=Path, required=True)
    child.add_argument("--flag", action="append", default=[])
    child.add_argument("--dependency", type=Path, action="append", default=[])
    child.add_argument("--output", type=Path, required=True)
    child = actions.add_parser("environment")
    child.add_argument("--cache", type=Path, required=True)
    child.add_argument("--namespace", required=True)
    child.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        if args.operation == "namespace":
            separate_outputs([args.output], [args.compiler, *args.dependency])
            value = compiler_namespace(args.compiler, args.flag, args.dependency)
            atomic_json(args.output, {"schema_version": 1, "namespace": value})
        elif args.operation == "environment":
            atomic_json(args.output, compiler_environment(args.cache, args.namespace))
        elif args.operation == "put":
            print(put(Path(args.file), args.cache, args.sha256))
        else:
            print(fetch(args.url, args.cache, args.sha256))
        return 0
    except (EvidenceError, OSError, ValueError) as error:
        print("Build cache rejected: " + str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
