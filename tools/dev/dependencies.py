#!/usr/bin/env python3
"""Explicit scoped source preparation and fully hashed offline wheel closure."""

from __future__ import annotations

import argparse
import email.parser
import hashlib
import json
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
from tools.maintenance.source_catalogue import SOURCE_DEPENDENCIES

MODULES = tuple(dependency.path for dependency in SOURCE_DEPENDENCIES)
FAMILY_MODULE = {"stm32f407": "vendors/st/cmsis_device_f4",
                 "stm32g474": "vendors/st/cmsis_device_g4",
                 "stm32h743": "vendors/st/cmsis_device_h7",
                 "stm32u575": "vendors/st/cmsis_device_u5", "gd32f470": None}


def required_submodules(scope: str, family: str | None = None,
                        backend: str = "baremetal") -> tuple[str, ...]:
    if scope in ("native", "sdk", "candidate"):
        if scope == "native" and family not in (None, "native"):
            raise EvidenceError("Native dependencies cannot select a hardware family")
        # Native model tests deliberately compile all maintained vendor headers.
        return MODULES
    if scope != "arm" or family not in FAMILY_MODULE or backend not in ("baremetal", "freertos", "threadx"):
        raise EvidenceError("unsupported dependency scope/family/backend")
    selected = ["vendors/arm/CMSIS_5"]
    if FAMILY_MODULE[family]:
        selected.append(FAMILY_MODULE[family])
    if backend != "baremetal":
        selected.append("ext/" + backend)
    return tuple(selected)


def prepare(root: Path, scope: str, *, family: str | None = None,
            backend: str = "baremetal", allow_network: bool = False) -> dict:
    selected = required_submodules(scope, family, backend)
    if allow_network:
        subprocess.run(["git", "-C", str(root), "submodule", "update", "--init",
                        "--recursive", "--", *selected], check=True, timeout=300)
    records = []
    for module in selected:
        result = subprocess.run(["git", "-C", str(root), "submodule", "status",
                                 "--", module], capture_output=True, text=True,
                                timeout=30, check=False)
        if result.returncode or not result.stdout or result.stdout[0] != " ":
            raise EvidenceError("dependency missing or not at locked Gitlink: " + module)
        status = subprocess.run(["git", "-C", str(root / module), "status",
                                 "--porcelain", "--untracked-files=all"],
                                capture_output=True, text=True, timeout=30,
                                check=False)
        if status.returncode or status.stdout:
            raise EvidenceError("dependency source is dirty: " + module)
        records.append({"path": module, "commit": result.stdout.split()[0]})
    recursive = subprocess.run(
        ["git", "-C", str(root), "submodule", "status", "--recursive", "--",
         *selected], capture_output=True, text=True, timeout=30, check=True)
    closure = []
    for line in recursive.stdout.splitlines():
        match = re.fullmatch(r" ([0-9a-f]{40}|[0-9a-f]{64}) ([^\r\n]+?)(?: \(.+\))?", line)
        if match is None:
            raise EvidenceError("selected recursive dependency is missing or changed")
        commit, path = match.groups()
        if not any(path == module or path.startswith(module + "/") for module in selected):
            raise EvidenceError("recursive dependency escaped selected source closure")
        status = subprocess.run(["git", "-C", str(root / path), "status",
                                 "--porcelain", "--untracked-files=all"],
                                capture_output=True, text=True, timeout=30,
                                check=True)
        if status.stdout:
            raise EvidenceError("recursive dependency source is dirty: " + path)
        closure.append({"path": path, "commit": commit})
    if not set(selected).issubset({item["path"] for item in closure}):
        raise EvidenceError("selected recursive source closure is incomplete")
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
