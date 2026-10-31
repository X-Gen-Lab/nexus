#!/usr/bin/env python3
"""Explicit scoped source preparation and fully hashed offline wheel closure."""

from __future__ import annotations

import argparse
import email.parser
import hashlib
import os
from pathlib import Path
import re
import subprocess
import shutil
import tarfile
import tempfile
import tomllib
import sys
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.evidence.common import (EvidenceError, atomic_json, digest, fields,
                                   file_identity, load_json, regular_file, separate_outputs)

ROOT = Path(__file__).resolve().parents[2]
from tools.maintenance.source_catalogue import (
    ALL_SOURCE_DEPENDENCIES, SOURCE_DEPENDENCIES,
)
from tools.configure.providers import descriptor
from tools.maintenance import source_catalogue

MODULES = tuple(dependency.path for dependency in ALL_SOURCE_DEPENDENCIES)
RUNTIME_MODULES = tuple(dependency.path for dependency in SOURCE_DEPENDENCIES)
FAMILY_MODULE = {item.family: tuple(path for _, path in item.git_dependencies)
                 for item in descriptor.families() if item.family != "native"}


def required_submodules(scope: str, family: str | None = None,
                        backend: str = "baremetal") -> tuple[str, ...]:
    if scope in ("native", "sdk", "candidate"):
        if scope == "native" and family not in (None, "native"):
            raise EvidenceError("Native dependencies cannot select a hardware family")
        # Native models compile all maintained vendor headers and host tests.
        # Firmware/source-SDK exports do not acquire the host test framework.
        return MODULES if scope == "native" else RUNTIME_MODULES
    if scope != "arm" or family not in FAMILY_MODULE or backend not in ("baremetal", "freertos", "threadx"):
        raise EvidenceError("unsupported dependency scope/family/backend")
    # Family descriptors own every approved compiler dependency, including the
    # shared Core used by imported GD32 device definitions.
    selected = list(FAMILY_MODULE[family])
    if backend != "baremetal":
        selected.append("ext/" + backend)
    return tuple(dict.fromkeys(selected))


def _git(root: Path, *arguments: str, check: bool = True):
    """Keep untrusted Git diagnostics and credential-bearing remotes private."""
    environment = {key: value for key, value in os.environ.items()
                   if not key.startswith("GIT_")}
    environment.update({"GIT_CONFIG_NOSYSTEM": "1",
                        "GIT_CONFIG_GLOBAL": os.devnull,
                        "GIT_OPTIONAL_LOCKS": "0",
                        "GIT_NO_REPLACE_OBJECTS": "1"})
    try:
        result = subprocess.run(["git", "-c", "core.fsmonitor=false", "-C",
                                 str(root), *arguments], env=environment,
                                capture_output=True, text=True, timeout=300,
                                check=False)
    except (OSError, subprocess.TimeoutExpired) as error:
        raise EvidenceError("dependency Git operation could not complete") from error
    if check and result.returncode:
        raise EvidenceError("dependency Git operation failed: " + arguments[0])
    return result


def _safe_directory(path: Path):
    if (not path.is_absolute() or ".." in path.parts or
            any(part.is_symlink() for part in (path, *path.parents))):
        raise EvidenceError("dependency directory must be canonical and not symlinked")
    if path.exists() and not path.is_dir():
        raise EvidenceError("dependency directory collides with an existing file")


def _clean_repository(path: Path):
    if (path / ".git").is_symlink():
        raise EvidenceError("dependency Git marker must not be symlinked")
    actual = _git(path, "rev-parse", "--show-toplevel").stdout.strip()
    if actual != str(path):
        raise EvidenceError("dependency directory is not its own Git root")
    if _git(path, "diff", "--cached", "--quiet", "HEAD", check=False).returncode:
        raise EvidenceError("dependency index contains uncommitted changes")
    dirty = _git(path, "status", "--porcelain", "--untracked-files=all",
                 "--ignore-submodules=all").stdout
    if dirty:
        raise EvidenceError("dependency source contains uncommitted files")


def _gitlinks(path: Path, parent: str | None, *, upstream: bool = False):
    declaration = _git(path, "show", "HEAD:.gitmodules", check=False)
    expected = source_catalogue.children(parent)
    if declaration.returncode and expected:
        raise EvidenceError("dependency module declaration is missing")
    try:
        source_catalogue.declared_modules(declaration.stdout.encode(), parent,
                                         upstream=upstream)
    except ValueError as error:
        raise EvidenceError("dependency module declaration differs from catalogue") from error
    links = {}
    for entry in _git(path, "ls-tree", "-rz", "HEAD").stdout.split("\0"):
        if entry:
            metadata, relative = entry.split("\t", 1)
            mode, kind, commit = metadata.split()
            if mode == "160000":
                if kind != "commit":
                    raise EvidenceError("dependency Gitlink has an invalid kind")
                links[relative] = commit
    if set(links) != {source_catalogue.relative_path(item) for item in expected}:
        raise EvidenceError("dependency Gitlinks differ from declared graph")
    return links


def _source_origin(path: Path, item, *, migrating: bool):
    result = _git(path, "config", "--get-all", "remote.origin.url", check=False)
    accepted = {item.fetch_url}
    if migrating:
        accepted.add(item.upstream_url)
    urls = result.stdout.splitlines()
    if result.returncode or len(urls) != 1 or urls[0] not in accepted:
        raise EvidenceError("dependency origin is not approved: " + item.path)
    _exact_transport(path, item)


def _configured_source(owner: Path, item, *, migrating: bool):
    key = "submodule." + source_catalogue.module_name(item) + ".url"
    result = _git(owner, "config", "--get-all", key, check=False)
    if result.returncode == 1 and not result.stdout:
        return
    accepted = {item.fetch_url}
    if migrating:
        accepted.add(item.upstream_url)
    urls = result.stdout.splitlines()
    if result.returncode or len(urls) != 1 or urls[0] not in accepted:
        raise EvidenceError("cached dependency URL is not approved: " + item.path)


def _revisions(root: Path, item, expected: str, actual: str, *, migrating: bool):
    if item.identifier != "freertos":
        return frozenset({expected})
    from tools.maintenance import freertos_transport
    checkout = root / item.path
    try:
        provisional = freertos_transport.provisional_migration_revisions(
            root, expected)
    except ValueError as error:
        raise EvidenceError("FreeRTOS transport review material is invalid") from error
    if actual != expected and (not migrating or actual not in provisional):
        raise EvidenceError("dependency revision is not approved: " + item.path)
    present = _git(checkout, "cat-file", "-e", expected + "^{commit}",
                   check=False).returncode == 0
    if not present and migrating:
        # Fetch objects from the reviewed URL without changing origin, HEAD or
        # the index. The target tree must pass review before checkout can move.
        _advertised(checkout, item, expected)
        _git(checkout, "fetch", "--no-tags", "--no-write-fetch-head",
             "--recurse-submodules=no", item.fetch_url, expected)
    try:
        revisions = freertos_transport.migration_revisions(root, expected)
    except ValueError as error:
        raise EvidenceError("FreeRTOS transport migration proof failed") from error
    if migrating:
        # Keep reviewed objects reachable for Git's normal submodule checkout.
        _git(checkout, "update-ref", "--no-deref",
             "refs/nexus/reviewed/" + expected, expected)
    return revisions


def _inspect_sources(root: Path, selected, *, migrating: bool):
    _safe_directory(root)
    _clean_repository(root)
    top = _gitlinks(root, None)
    records = []

    def visit(item, expected):
        path = root / item.path
        owner = root if item.parent is None else root / item.parent
        _configured_source(owner, item, migrating=migrating)
        _safe_directory(path)
        initialized = (path / ".git").exists()
        if not initialized:
            if path.exists() and any(path.iterdir()):
                raise EvidenceError("uninitialized dependency directory is not empty: " + item.path)
            if not migrating:
                raise EvidenceError("dependency is missing: " + item.path)
            records.append({"path": item.path, "commit": expected})
            return
        _clean_repository(path)
        _source_origin(path, item, migrating=migrating)
        actual = _git(path, "rev-parse", "HEAD").stdout.strip()
        allowed = _revisions(root, item, expected, actual, migrating=migrating)
        if actual != expected and (not migrating or actual not in allowed):
            raise EvidenceError("dependency revision is not approved: " + item.path)
        links = _gitlinks(path, item.path, upstream=actual != expected)
        records.append({"path": item.path, "commit": expected})
        for child in source_catalogue.children(item.path):
            visit(child, links[source_catalogue.relative_path(child)])

    for name in selected:
        item = source_catalogue.dependency(name)
        if item.parent is not None:
            raise EvidenceError("dependency selection must use top-level modules")
        visit(item, top[item.path])
    return records


def _exact_transport(root: Path, item):
    effective = _git(root, "ls-remote", "--get-url", item.fetch_url).stdout.strip()
    if effective != item.fetch_url:
        raise EvidenceError("dependency fetch URL must not be rewritten: " + item.path)


def _advertised(root: Path, item, commit: str):
    _exact_transport(root, item)
    reference = "refs/tags/nexus/pins/" + commit
    arguments = ("ls-remote", "--exit-code", item.fetch_url)
    if item.fetch_url != item.upstream_url:
        arguments += (reference, reference + "^{}")
    result = _git(root, *arguments)
    if not result.stdout.strip():
        raise EvidenceError("dependency fetch URL has no advertised references: " + item.path)
    if item.fetch_url != item.upstream_url:
        refs = dict(line.split("\t", 1)[::-1]
                    for line in result.stdout.splitlines())
        if refs.get(reference + "^{}", refs.get(reference)) != commit:
            raise EvidenceError("dependency retained pin is unavailable: " + item.path)


def _available_sources(root: Path, records):
    for record in records:
        _advertised(root, source_catalogue.dependency(record["path"]),
                    record["commit"])


def prepare(root: Path, scope: str, *, family: str | None = None,
            backend: str = "baremetal", allow_network: bool = False) -> dict:
    root = Path(root).absolute()
    selected = required_submodules(scope, family, backend)
    closure = _inspect_sources(root, selected, migrating=allow_network)
    if allow_network:
        _available_sources(root, closure)
        advertised = {record["path"] for record in closure}
        _git(root, "submodule", "sync", "--", *selected)
        _git(root, "-c", "submodule.recurse=false", "submodule", "update",
             "--init", "--checkout", "--jobs", "2", "--", *selected)
        # The parent's new .gitmodules must be present before recursive sync;
        # otherwise its old URLs would overwrite the reviewed child transport.
        closure = _inspect_sources(root, selected, migrating=True)
        _available_sources(root, [record for record in closure
                                  if record["path"] not in advertised])
        _git(root, "submodule", "sync", "--recursive", "--", *selected)
        _git(root, "submodule", "update", "--init", "--recursive",
             "--checkout", "--jobs", "2", "--", *selected)
        closure = _inspect_sources(root, selected, migrating=False)
    if "ext/freertos" in selected:
        from tools.maintenance import freertos_transport
        freertos_transport.verify(root, parent_index=False)
    records = [record for record in closure if record["path"] in selected]
    return {"schema_version": 1, "kind": "dependency_preparation",
            "scope": scope, "family": family, "backend": backend,
            "network_authorized": allow_network, "submodules": records,
            "recursive_submodules": closure,
            "status": "verified"}


def preset_selection(root: Path, preset: str) -> dict:
    """Use the maintained preset resolver; source selection remains TOML-owned."""
    from tools.dev.presets import selected_configure
    from tools.evidence.common import identifier
    selected = selected_configure(root, preset)
    values = selected.get("cacheVariables", {})
    assembly = values.get("NEXUS_ASSEMBLY_FILE")
    if isinstance(assembly, dict):
        assembly = assembly.get("value")
    if not isinstance(assembly, str):
        raise EvidenceError("dependency preset must select its authored assembly")
    path = Path(assembly)
    if not path.is_absolute():
        path = root / path
    authored = tomllib.loads(regular_file(path).read_text())
    if "board" in authored and "board_package" in authored:
        raise EvidenceError("assembly must choose Board identity or package, not both")
    if "board_package" in authored:
        package = authored["board_package"]
        if not isinstance(package, str) or not package:
            raise EvidenceError("assembly Board package path is invalid")
        directory = Path(package)
        if not directory.is_absolute():
            directory = path.parent / directory
        board_path = directory.resolve() / "board.json"
    else:
        board = identifier(authored.get("board"), "assembly Board")
        board_path = root / "boards" / board / "board.json"
    facts = load_json(board_path)
    family = facts.get("soc_family")
    backend = authored.get("backend")
    if family == "native":
        scope = "native"
    else:
        scope = "arm"
    required_submodules(scope, family, backend)
    return {"scope": scope, "family": family, "backend": backend,
            "preset": preset, "assembly": file_identity(path),
            "board": file_identity(board_path)}


def prepare_preset(root: Path, preset: str, *, allow_network: bool = False) -> dict:
    selection = preset_selection(root, preset)
    return {**prepare(root, selection["scope"], family=selection["family"],
                     backend=selection["backend"], allow_network=allow_network),
            **selection}


def _requirements(text: str) -> dict[str, str]:
    result = {}
    for line in text.splitlines():
        line = line.split("#", 1)[0].strip()
        if not line:
            continue
        match = re.fullmatch(r"([A-Za-z0-9_.-]+)==([A-Za-z0-9_.+!-]+)", line)
        if match is None:
            raise EvidenceError("wheel lock input must contain exact package==version pins")
        name = re.sub(r"[-_.]+", "-", match[1]).lower()
        if name in result:
            raise EvidenceError("duplicate package requirement")
        result[name] = match[2]
    if not result:
        raise EvidenceError("nonempty pinned wheel requirements required")
    return result


def wheel_lock(directory: Path, requirements: str, *, target: str) -> dict:
    pinned = _requirements(requirements)
    wheels = []
    for path in sorted(directory.iterdir()):
        if path.suffix != ".whl" or path.is_symlink() or not path.is_file():
            raise EvidenceError("wheel bundle may contain only regular wheel files")
        pieces = path.name.split("-")
        if len(pieces) < 5:
            raise EvidenceError("invalid wheel filename")
        name = re.sub(r"[-_.]+", "-", pieces[0]).lower()
        try:
            with zipfile.ZipFile(path) as archive:
                metadata = [entry for entry in archive.namelist()
                            if len(entry.split("/")) == 2 and
                            entry.endswith(".dist-info/METADATA")]
                if len(metadata) != 1:
                    raise EvidenceError("wheel must contain one metadata record")
                info_directory = metadata[0].split("/")[0].removesuffix(".dist-info")
                info_name, separator, info_version = info_directory.rpartition("-")
                if (not separator or
                        re.sub(r"[-_.]+", "-", info_name).lower() != name or
                        info_version != pieces[1]):
                    raise EvidenceError("wheel top-level metadata directory differs from identity")
                record = email.parser.BytesParser().parsebytes(archive.read(metadata[0]))
                normalized = re.sub(r"[-_.]+", "-", record.get("Name", "")).lower()
                if normalized != name or record.get("Version") != pieces[1]:
                    raise EvidenceError("wheel metadata differs from filename identity")
        except (zipfile.BadZipFile, KeyError) as error:
            raise EvidenceError("invalid wheel archive") from error
        wheel = {"filename": path.name, "name": name, "version": pieces[1],
                 "sha256": digest(path), "size": path.stat().st_size}
        wheels.append(wheel)
    if not wheels or len({wheel["name"] for wheel in wheels}) != len(wheels):
        raise EvidenceError("wheel bundle must contain one selected wheel per package")
    observed = {wheel["name"]: wheel["version"] for wheel in wheels}
    if any(observed.get(name) != version for name, version in pinned.items()):
        raise EvidenceError("bundle does not satisfy exact direct requirements")
    # pip download resolves every dependency; pip --require-hashes installation
    # verifies closure again, including extras/markers of the actual environment.
    return {"schema_version": 1, "kind": "offline_python_wheels",
            "target": target, "direct_requirements": pinned, "wheels": wheels}


def verify_wheels(directory: Path, lock: dict) -> dict:
    fields(lock, {"schema_version", "kind", "target", "direct_requirements", "wheels"})
    if lock["schema_version"] != 1 or lock["kind"] != "offline_python_wheels":
        raise EvidenceError("unsupported wheel lock")
    if not isinstance(lock["target"], str) or not lock["target"] or len(lock["target"]) > 128:
        raise EvidenceError("wheel target identity is invalid")
    if not isinstance(lock["direct_requirements"], dict) or not lock["direct_requirements"]:
        raise EvidenceError("wheel direct requirements are missing")
    _requirements("\n".join(name + "==" + version
                            for name, version in lock["direct_requirements"].items()))
    if not isinstance(lock["wheels"], list):
        raise EvidenceError("wheel identities must be a list")
    names = set()
    observed = {}
    for wheel in lock["wheels"]:
        fields(wheel, {"filename", "name", "version", "sha256", "size"})
        name = wheel["filename"]
        if Path(name).name != name or not name.endswith(".whl") or name in names:
            raise EvidenceError("noncanonical or duplicate wheel name")
        names.add(name)
        _requirements(wheel["name"] + "==" + wheel["version"])
        pieces = name.split("-")
        normalized = re.sub(r"[-_.]+", "-", pieces[0]).lower()
        if len(pieces) < 5 or normalized != wheel["name"] or pieces[1] != wheel["version"]:
            raise EvidenceError("wheel lock identity differs from filename")
        if wheel["name"] in observed:
            raise EvidenceError("duplicate package in wheel closure")
        observed[wheel["name"]] = wheel["version"]
        path = regular_file(directory / name)
        if digest(path) != wheel["sha256"] or path.stat().st_size != wheel["size"]:
            raise EvidenceError("offline wheel bytes changed")
    if set(path.name for path in directory.iterdir()) != names:
        raise EvidenceError("offline wheel bundle has missing or extra files")
    if not names or any(observed.get(name) != version
                        for name, version in lock["direct_requirements"].items()):
        raise EvidenceError("empty or incomplete direct requirement closure")
    return {"status": "verified", "wheels": len(names), "target": lock["target"]}


def hashed_requirements(lock: dict) -> str:
    return "".join(f"{wheel['name']}=={wheel['version']} --hash=sha256:{wheel['sha256']}\n"
                   for wheel in lock["wheels"])


def export_wheels(requirements: list[Path], output: Path, *, python: str = sys.executable,
                  python_version: str | None = None) -> dict:
    if output.exists():
        raise EvidenceError("offline wheel export destination must be new")
    output.mkdir(parents=True)
    wheels = output / "wheels"
    wheels.mkdir()
    text = "\n".join(regular_file(path).read_text() for path in requirements)
    _requirements(text)
    argv = [python, "-m", "pip", "download", "--only-binary=:all:", "--dest", str(wheels)]
    if python_version:
        if not re.fullmatch(r"3\.(11|12|13)", python_version):
            raise EvidenceError("wheel export supports explicit CPython 3.11/3.12/3.13")
        argv.extend(["--python-version", python_version, "--implementation", "cp",
                     "--abi", "cp" + python_version.replace(".", ""),
                     "--platform", "manylinux2014_x86_64",
                     "--platform", "manylinux2010_x86_64",
                     "--platform", "manylinux_2_17_x86_64",
                     "--platform", "manylinux_2_12_x86_64"])
    for path in requirements:
        argv.extend(["-r", str(path)])
    raw = output / "download.raw"
    with raw.open("wb") as stream:
        result = subprocess.run(argv, stdout=stream, stderr=subprocess.STDOUT,
                                timeout=300, check=False)
    if result.returncode:
        raise EvidenceError("actual pip wheel resolution/download failed; raw retained")
    import platform
    lock = wheel_lock(wheels, text, target=("Linux-x86_64-cp" + python_version.replace(".", "") if python_version else
                          f"{platform.system()}-{platform.machine()}-cp{sys.version_info.major}{sys.version_info.minor}"))
    lock["requirements_sha256"] = hashlib.sha256(text.encode()).hexdigest()
    # Keep the reusable lock schema separate from command metadata.
    sha = lock.pop("requirements_sha256")
    atomic_json(output / "lock.json", lock)
    (output / "requirements-hashed.txt").write_text(hashed_requirements(lock))
    verify_wheels(wheels, lock)
    receipt = {"status": "downloaded_and_verified", "argv": argv,
               "exit_code": result.returncode, "raw": file_identity(raw),
               "requirements_sha256": sha, "lock": file_identity(output / "lock.json")}
    atomic_json(output / "receipt.json", receipt)
    return receipt


def install_wheels(bundle: Path, python: str, *, report: Path) -> dict:
    separate_outputs([report, report.with_suffix(".raw")], input_trees=[bundle])
    lock = load_json(bundle / "lock.json")
    verify_wheels(bundle / "wheels", lock)
    hashed = regular_file(bundle / "requirements-hashed.txt")
    if hashed.read_text() != hashed_requirements(lock):
        raise EvidenceError("hash requirements differ from wheel lock")
    report.parent.mkdir(parents=True, exist_ok=True)
    raw = report.with_suffix(".raw")
    argv = [python, "-m", "pip", "install", "--no-index", "--find-links", str(bundle / "wheels"),
            "--require-hashes", "-r", str(hashed)]
    with raw.open("xb") as stream:
        result = subprocess.run(argv, stdout=stream, stderr=subprocess.STDOUT,
                                timeout=300, check=False)
    receipt = {"schema_version": 1, "status": "passed" if result.returncode == 0 else "failed",
               "exit_code": result.returncode, "argv": argv, "raw": file_identity(raw),
               "lock": file_identity(bundle / "lock.json")}
    atomic_json(report, receipt)
    if result.returncode:
        raise EvidenceError("actual offline hash-required installation failed")
    return receipt


def install_ccache(directory: Path, cache_directory: Path, *, root: Path = ROOT) -> dict:
    """Prepare the reviewed optional host cache from exact distribution bytes."""
    from tools.dev.cache import fetch
    lock = load_json(root / "dependencies/ccache.lock.json")
    fields(lock, {"schema_version", "kind", "version", "platform", "url", "sha256",
                  "bytes", "identity_origin", "signature_verified", "license"})
    if (lock["schema_version"] != 1 or lock["kind"] != "compiler_cache_distribution" or
            lock["platform"] != "linux-x86_64-musl-static"):
        raise EvidenceError("unsupported ccache distribution")
    import platform
    if platform.system() != "Linux" or platform.machine() != "x86_64":
        raise EvidenceError("locked optional ccache currently supports Linux x86_64")
    if directory.exists() or any(path.is_symlink() for path in [directory, *directory.parents]):
        raise EvidenceError("ccache preparation destination must be new")
    archive = fetch(lock["url"], cache_directory, lock["sha256"], maximum_bytes=lock["bytes"])
    if archive.stat().st_size != lock["bytes"]:
        raise EvidenceError("ccache archive size differs from lock")
    directory.parent.mkdir(parents=True, exist_ok=True)
    temporary = Path(tempfile.mkdtemp(prefix=".ccache-", dir=directory.parent))
    try:
        prefix = "ccache-" + lock["version"] + "-" + lock["platform"]
        seen, expanded = set(), 0
        with tarfile.open(archive, "r:xz") as content:
            for member in content:
                parts = member.name.split("/")
                if (parts[0] != prefix or member.name in seen or
                        any(part in ("", ".", "..") for part in parts) or
                        not (member.isfile() or member.isdir())):
                    raise EvidenceError("ccache distribution member is unsafe")
                seen.add(member.name)
                expanded += member.size
                if expanded > 100 * 1024 * 1024 or len(seen) > 1000:
                    raise EvidenceError("ccache distribution exceeds extraction bound")
                if len(parts) == 1:
                    continue
                target = temporary.joinpath(*parts[1:])
                if member.isdir():
                    target.mkdir(parents=True, exist_ok=True)
                else:
                    target.parent.mkdir(parents=True, exist_ok=True)
                    with target.open("xb") as output:
                        shutil.copyfileobj(content.extractfile(member), output, 1024 * 1024)
                    target.chmod(member.mode & 0o777)
        for name in ("ccache", "LICENSE.md", "GPL-3.0.txt"):
            regular_file(temporary / name)
        result = subprocess.run([str(temporary / "ccache"), "--version"],
                                capture_output=True, timeout=30, check=False)
        if result.returncode or not result.stdout.startswith(("ccache version " + lock["version"]).encode()):
            raise EvidenceError("actual ccache executable version differs from lock")
        (temporary / "version.raw").write_bytes(result.stdout + result.stderr)
        temporary.replace(directory)
        receipt = {"schema_version": 1, "status": "verified", "version": lock["version"],
                   "archive": file_identity(archive), "binary": file_identity(directory / "ccache"),
                   "notices": [file_identity(directory / name) for name in ("LICENSE.md", "GPL-3.0.txt")],
                   "version_raw": file_identity(directory / "version.raw"),
                   "signature_verified": False, "formal_rebuild_cache_enabled": False}
        atomic_json(directory / "receipt.json", receipt)
        return receipt
    finally:
        if temporary.exists():
            shutil.rmtree(temporary)


ACTIONLINT_LOCK = Path("dependencies/actionlint.lock.json")


def actionlint_asset(lock_path: Path) -> tuple[dict, dict]:
    """Read the single official distribution authority for this host."""
    import platform
    lock = load_json(lock_path)
    fields(lock, {"schema_version", "kind", "version", "tag", "tag_commit",
                  "upstream", "license", "signature_verified", "assets"})
    if (lock["schema_version"] != 1 or
            lock["kind"] != "actionlint_official_distribution" or
            not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", lock["version"]) or
            lock["tag"] != "v" + lock["version"] or
            lock["upstream"] != "https://github.com/rhysd/actionlint" or
            lock["license"] != "MIT" or lock["signature_verified"] is not False):
        raise EvidenceError("invalid actionlint lock identity")
    platforms = {"linux_amd64", "darwin_amd64", "darwin_arm64",
                 "windows_amd64"}
    if not isinstance(lock["assets"], dict) or set(lock["assets"]) != platforms:
        raise EvidenceError("actionlint lock requires the four reviewed hosts")
    for name, asset in lock["assets"].items():
        fields(asset, {"url", "bytes", "sha256", "format", "executable",
                       "notice"})
        extension = "zip" if name.startswith("windows_") else "tar.gz"
        expected = (lock["upstream"] + "/releases/download/" + lock["tag"] +
                    "/actionlint_" + lock["version"] + "_" + name +
                    "." + extension)
        if asset["url"] != expected or asset["format"] != extension:
            raise EvidenceError("actionlint lock requires exact official URL")
        for record in (asset, asset["executable"], asset["notice"]):
            if (type(record["bytes"]) is not int or
                    not 0 < record["bytes"] <= 8 * 1024 * 1024 or
                    not re.fullmatch(r"[0-9a-f]{64}", record["sha256"])):
                raise EvidenceError("invalid actionlint size or digest")
        for record, member, mode in (
                (asset["executable"], "actionlint.exe" if
                 name.startswith("windows_") else "actionlint", 0o755),
                (asset["notice"], "LICENSE.txt", 0o644)):
            fields(record, {"member", "bytes", "sha256", "mode"})
            if record["member"] != member or record["mode"] != mode:
                raise EvidenceError("invalid actionlint member or mode")
    system = platform.system().lower()
    machine = {"x86_64": "amd64", "amd64": "amd64",
               "arm64": "arm64", "aarch64": "arm64"}.get(
                   platform.machine().lower(), "unsupported")
    host = system + "_" + machine
    if host not in platforms:
        raise EvidenceError("unsupported actionlint host: " + host)
    return lock, {"platform": host, **lock["assets"][host]}


def verify_actionlint(directory: Path, lock_path: Path) -> Path:
    """Verify owned exact bytes offline; never discover or download a tool."""
    import os
    directory = Path(directory).absolute()
    if any(path.is_symlink() for path in (directory, *directory.parents)):
        raise EvidenceError("actionlint prefix cannot use a symlink")
    lock, asset = actionlint_asset(lock_path)
    try:
        receipt = load_json(directory / "receipt.json")
        if receipt != {"schema_version": 1, "kind": "prepared_actionlint",
                       "lock_sha256": digest(lock_path),
                       "platform": asset["platform"],
                       "version": lock["version"]}:
            raise EvidenceError("actionlint receipt differs from staged lock")
        for record in (asset["executable"], asset["notice"]):
            path = directory / record["member"]
            regular_file(path)
            if (path.stat().st_size != record["bytes"] or
                    digest(path) != record["sha256"] or
                    (os.name != "nt" and
                     path.stat().st_mode & 0o777 != record["mode"])):
                raise EvidenceError("actionlint member differs from lock")
        stdout = directory / "version.stdout"
        stderr = directory / "version.stderr"
        regular_file(stdout)
        regular_file(stderr, nonempty=False)
        if (stdout.read_bytes().splitlines()[0] != lock["version"].encode()
                or stderr.read_bytes()):
            raise EvidenceError("actionlint version evidence differs from lock")
    except (OSError, IndexError, EvidenceError) as error:
        raise EvidenceError("actionlint not prepared for this lock; run "
                            "install_dev_tools.py explicitly: " + str(error)) from error
    return directory / asset["executable"]["member"]


def install_actionlint(directory: Path, cache_directory: Path, *,
                       root: Path = ROOT) -> dict:
    """Explicitly prepare one reviewed host tool; preserve unknown prefixes."""
    from tools.dev.cache import fetch
    from tools.dev import process
    import os
    import stat
    lock_path = root / ACTIONLINT_LOCK
    lock, asset = actionlint_asset(lock_path)
    directory = directory.absolute()
    cache_directory = cache_directory.absolute()
    if (directory.is_relative_to(cache_directory) or
            cache_directory.is_relative_to(directory)):
        raise EvidenceError("actionlint cache/prefix overlap is not allowed")
    if any(path.is_symlink() for path in (directory, *directory.parents)):
        raise EvidenceError("actionlint preparation cannot use symlinks")
    if directory.exists():
        verify_actionlint(directory, lock_path)
        return load_json(directory / "receipt.json")
    archive = fetch(asset["url"], cache_directory, asset["sha256"],
                    maximum_bytes=asset["bytes"])
    if archive.stat().st_size != asset["bytes"]:
        raise EvidenceError("actionlint archive size differs from lock")
    directory.parent.mkdir(parents=True, exist_ok=True)
    temporary = Path(tempfile.mkdtemp(prefix=".actionlint-", dir=directory.parent))
    wanted = {item["member"]: item for item in
              (asset["executable"], asset["notice"])}
    seen, expanded = set(), 0
    attempted = False

    def member(name, size, mode, source):
        nonlocal expanded
        parts = name.split("/")
        if (name in seen or "\\" in name or
                any(part in ("", ".", "..") for part in parts)):
            raise EvidenceError("unsafe actionlint archive member")
        seen.add(name)
        expanded += size
        if expanded > 8 * 1024 * 1024 or len(seen) > 32:
            raise EvidenceError("actionlint archive exceeds extraction bound")
        if name in wanted:
            record = wanted[name]
            if size != record["bytes"] or mode & 0o777 != record["mode"]:
                raise EvidenceError("actionlint archive member differs from lock")
            target = temporary / name
            with target.open("xb") as output:
                shutil.copyfileobj(source, output, 1024 * 1024)
            if digest(target) != record["sha256"]:
                raise EvidenceError("actionlint member digest differs from lock")
            target.chmod(record["mode"])

    try:
        if asset["format"] == "zip":
            with zipfile.ZipFile(archive) as content:
                for item in content.infolist():
                    mode = item.external_attr >> 16
                    if (not stat.S_ISREG(mode) or item.flag_bits & 1):
                        raise EvidenceError("nonregular actionlint ZIP member")
                    with content.open(item) as source:
                        member(item.filename, item.file_size, mode, source)
        else:
            with tarfile.open(archive, "r:gz") as content:
                for item in content:
                    if not item.isfile():
                        raise EvidenceError("nonregular actionlint TAR member")
                    with content.extractfile(item) as source:
                        member(item.name, item.size, item.mode, source)
        if not set(wanted).issubset(seen):
            raise EvidenceError("actionlint archive lacks binary or notice")
        attempted = True
        result = process.run(
            [str(temporary / asset["executable"]["member"]), "-version"],
            cwd=temporary, env=dict(os.environ),
            log=temporary / "prepare.log", timeout=30,
            stage="actionlint-version")
        command = load_json(temporary / "prepare.command.json")
        for channel in ("stdout", "stderr"):
            shutil.copyfile(command[channel]["path"],
                            temporary / ("version." + channel))
        if result.returncode:
            raise EvidenceError("actual actionlint -version failed")
        receipt = {"schema_version": 1, "kind": "prepared_actionlint",
                   "lock_sha256": digest(lock_path),
                   "platform": asset["platform"], "version": lock["version"]}
        atomic_json(temporary / "receipt.json", receipt)
        verify_actionlint(temporary, lock_path)
        temporary.replace(directory)
        return receipt
    finally:
        # Keep a failed execution and its original receipts for diagnosis.
        if temporary.exists() and not attempted:
            shutil.rmtree(temporary)


def verify_upgrade(gates: dict) -> dict:
    """Reuse the reviewed dependency gate list; references never approve a merge."""
    from tools.maintenance.dependency_proposals import GATES
    if set(gates) != set(GATES):
        raise EvidenceError("dependency upgrade requires every maintained gate")
    references = {}
    from tools.evidence.common import verify_file_identity
    for name, record in gates.items():
        fields(record, {"status", "evidence"})
        if record["status"] not in ("executed_passed", "independently_reviewed"):
            raise EvidenceError("missing execution/review for dependency gate: " + name)
        if not isinstance(record["evidence"], list) or not record["evidence"]:
            raise EvidenceError("dependency gate requires actual evidence files")
        for identity in record["evidence"]:
            verify_file_identity(identity)
        references[name] = record
    return {"schema_version": 1, "kind": "dependency_upgrade_gate_references",
            "gates": references, "status": "references_verified",
            "merge_authorized": False, "qualification_claim": False}


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    actions = parser.add_subparsers(dest="operation", required=True)
    child = actions.add_parser("prepare")
    child.add_argument("--root", type=Path, default=ROOT)
    child.add_argument("--scope", choices=("native", "arm", "sdk", "candidate", "preset"), required=True)
    child.add_argument("--preset")
    child.add_argument("--family", choices=tuple(FAMILY_MODULE))
    child.add_argument("--backend", choices=("baremetal", "freertos", "threadx"))
    child.add_argument("--allow-network", action="store_true")
    child.add_argument("--report", type=Path)
    child = actions.add_parser("wheel-export")
    child.add_argument("--requirements", type=Path, action="append", required=True)
    child.add_argument("--output", type=Path, required=True)
    child.add_argument("--python-version")
    child = actions.add_parser("wheel-install")
    child.add_argument("--bundle", type=Path, required=True)
    child.add_argument("--python", required=True)
    child.add_argument("--report", type=Path, required=True)
    child = actions.add_parser("ccache-prepare")
    child.add_argument("--install-dir", type=Path, required=True)
    child.add_argument("--cache", type=Path, required=True)
    child.add_argument("--root", type=Path, default=ROOT)
    child = actions.add_parser("actionlint-prepare")
    child.add_argument("--install-dir", type=Path, required=True)
    child.add_argument("--cache", type=Path, required=True)
    child.add_argument("--root", type=Path, default=ROOT)
    child = actions.add_parser("upgrade-verify")
    child.add_argument("--gates", type=Path, required=True)
    child.add_argument("--report", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        if args.operation == "prepare":
            if args.scope == "preset":
                if args.preset is None:
                    parser.error("preset preparation requires --preset")
                if args.family is not None or args.backend is not None:
                    parser.error("preset preparation selects family/backend from its assembly")
                result = prepare_preset(args.root, args.preset, allow_network=args.allow_network)
            else:
                result = prepare(args.root, args.scope, family=args.family,
                                 backend=args.backend or "baremetal", allow_network=args.allow_network)
            if args.report:
                atomic_json(args.report, result)
        elif args.operation == "wheel-export":
            result = export_wheels(args.requirements, args.output, python_version=args.python_version)
        elif args.operation == "wheel-install":
            result = install_wheels(args.bundle, args.python, report=args.report)
        elif args.operation == "ccache-prepare":
            result = install_ccache(args.install_dir, args.cache, root=args.root)
        elif args.operation == "actionlint-prepare":
            result = install_actionlint(args.install_dir, args.cache, root=args.root)
            result = {**result, "status": "verified"}
        else:
            result = verify_upgrade(load_json(args.gates))
            atomic_json(args.report, result)
        print(result["status"])
        return 0
    except (EvidenceError, OSError, ValueError, subprocess.SubprocessError) as error:
        print("Dependency preparation rejected: " + str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
