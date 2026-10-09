#!/usr/bin/env python3
"""Prepare/verify a relocatable source SDK with fixed dependency provenance.

No compiler or effective build configuration is embedded as a reusable binary
ABI. This package is an unsigned source snapshot, not a promoted release.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True

ROOT_FILES = {"CMakeLists.txt", "LICENSE", "README.md", "AGENTS.md",
              ".clang-format", ".editorconfig", ".clang-format-dirs"}
ROOT_DIRS = {"arch", "boards", "cmake", "dependencies", "core", "io", "os",
             "components", "tools", "scripts", "soc"}
REQUIRED_DEPENDENCIES = ("ext/freertos", "vendors/arm/CMSIS_5",
                         "vendors/st/cmsis_device_f4")
REQUIRED_FILES = (
    "CMakeLists.txt", "LICENSE", "dependencies/toolchains.lock.json",
    "dependencies/environment.lock.json", "core/include/nexus/core/request.h",
    "core/src/request.c", "io/include/nexus/io/gpio.h",
    "io/include/nexus/io/uart.h", "os/freertos/include/FreeRTOSConfig.h",
    "boards/stm32f407_qiming_v31/board.json",
    "boards/stm32f407ve_sky_qingchun/board.json",
    "boards/gd32f470_liangshan/board.json",
    "soc/stm32f407/soc.json", "soc/gd32f470/soc.json",
    "tools/configure/configure.py", "cmake/platform/Firmware.cmake",
    "tools/measurement/workload.c",
    "ext/freertos/tasks.c", "ext/freertos/LICENSE.md",
    "vendors/arm/CMSIS_5/CMSIS/Core/Include/core_cm4.h", "vendors/arm/CMSIS_5/LICENSE.txt",
    "vendors/st/cmsis_device_f4/Source/Templates/gcc/startup_stm32f407xx.s",
    "vendors/st/cmsis_device_f4/LICENSE.md",
    "vendors/gigadevice/gd32f4xx/Firmware/CMSIS/GD/GD32F4xx/Source/GCC/startup_gd32f450_470.S",
    "vendors/gigadevice/gd32f4xx/source.lock.json",
    "cmake/package/NexusConfig.cmake.in", "cmake/package/NexusConfigVersion.cmake.in",
    "cmake/package/package_source_sdk.py")
MANIFEST = ".nexus-source-sdk.json"
HEX40 = re.compile(r"[0-9a-f]{40}")
HEX64 = re.compile(r"[0-9a-f]{64}")


class SDKError(ValueError):
    pass


def digest(data):
    return hashlib.sha256(data).hexdigest()


def object_pairs(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise SDKError("Duplicate identity key")
        result[key] = value
    return result


def safe_relative(name):
    if not isinstance(name, str):
        raise SDKError("Source paths must be strings")
    path = PurePosixPath(name)
    if (not name or "\\" in name or path.is_absolute() or ".." in path.parts
            or path.as_posix() != name or name == "."):
        raise SDKError("Unsafe source SDK path")
    return path


def regular(root, name):
    rel = safe_relative(name)
    path = root / Path(*rel.parts)
    for index in range(1, len(rel.parts) + 1):
        if (root / Path(*rel.parts[:index])).is_symlink():
            raise SDKError(f"Source SDK cannot follow symlinks: {name}")
    if not path.is_file():
        raise SDKError(f"Missing source SDK file: {name}")
    return path


def git(root, *args):
    result = subprocess.run(["git", "-C", str(root), *args], capture_output=True)
    if result.returncode:
        raise SDKError("A complete Git checkout is required: " + result.stderr.decode(errors="replace").strip())
    return result.stdout


def tree_entries(root):
    entries = {}
    for row in git(root, "ls-tree", "-r", "-z", "HEAD").split(b"\0"):
        if not row:
            continue
        meta, name = row.split(b"\t", 1)
        mode, kind, oid = meta.decode().split()
        entries[name.decode()] = (mode, kind, oid)
    return entries


def selected(name):
    return (name in ROOT_FILES or name.split("/", 1)[0] in ROOT_DIRS
            or name.startswith("vendors/gigadevice/gd32f4xx/"))


def dependency_records(root, entries):
    records = []
    for dep in REQUIRED_DEPENDENCIES:
        row = entries.get(dep)
        if not row or row[:2] != ("160000", "commit"):
            raise SDKError(f"Source commit has no required dependency Gitlink: {dep}")
        checkout = root / dep
        actual = git(checkout, "rev-parse", "HEAD").decode().strip()
        if actual != row[2] or not (checkout / ".git").exists():
            raise SDKError(f"Initialize the pinned dependency before packaging: {dep}")
        output = git(root, "submodule", "status", "--recursive", "--", dep).decode()
        for line in output.splitlines():
            match = re.fullmatch(r" ([0-9a-f]{40}) (.+?)(?: \([^\n]+\))?", line)
            if not match:
                raise SDKError("Required recursive dependencies must be initialized at their recorded commits")
            path, commit = match[2], match[1]
            checkout = root / safe_relative(path)
            if git(checkout, "status", "--porcelain", "--untracked-files=normal").strip():
                raise SDKError(f"Dependency checkout is dirty: {path}")
            records.append({"path": path, "commit": commit,
                            "tree": git(checkout, "rev-parse", "HEAD^{tree}").decode().strip()})
    if len({record["path"] for record in records}) != len(records):
        raise SDKError("Duplicate dependency identity")
    return records


def committed_blobs(root, entries):
    """Read Git blobs without checkout EOL/export transformations."""
    names = sorted(entries)
    request = "".join(entries[name][2] + "\n" for name in names).encode()
    result = subprocess.run(["git", "-C", str(root), "cat-file", "--batch"],
                            input=request, capture_output=True)
    if result.returncode:
        raise SDKError("Cannot read the committed source objects")
    cursor = 0
    blobs = {}
    for name in names:
        end = result.stdout.find(b"\n", cursor)
        header = result.stdout[cursor:end].decode().split()
        if len(header) != 3 or header[0] != entries[name][2] or header[1] != "blob":
            raise SDKError("Source object identity is not a regular Git blob")
        size = int(header[2])
        cursor = end + 1
        data = result.stdout[cursor:cursor + size]
        cursor += size + 1
        if len(data) != size or hashlib.sha1(b"blob " + str(size).encode() + b"\0" + data).hexdigest() != entries[name][2]:
            raise SDKError("Committed source blob identity changed")
        blobs[name] = data
    if cursor != len(result.stdout):
        raise SDKError("Unexpected committed source object data")
    return blobs


def copy_file(source, output, name, data=None, executable=None):
    file = regular(source, name)
    if data is None:
        data = file.read_bytes()
    destination = output / safe_relative(name)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(data)
    destination.chmod(0o755 if (os.access(file, os.X_OK) if executable is None else executable) else 0o644)
    return digest(data)


def prepare(source, output, development_fixture=False):
    source = source.resolve()
    output = output.absolute()
    if output != output.resolve():
        raise SDKError("Source SDK output requires a canonical path without symlink or parent-directory aliases")
    if output.exists() or output.is_symlink():
        raise SDKError("Source SDK output must not already exist")
    if output.is_relative_to(source):
        raise SDKError("Prepare the relocatable source SDK outside its input checkout")
    if Path(git(source, "rev-parse", "--show-toplevel").decode().strip()).resolve() != source:
        raise SDKError("The source must be the actual Git repository root")
    revision = git(source, "rev-parse", "HEAD").decode().strip()
    source_tree = git(source, "rev-parse", "HEAD^{tree}").decode().strip()
    dirty = bool(git(source, "status", "--porcelain", "--untracked-files=normal").strip())
    if dirty and not development_fixture:
        raise SDKError("Publishable source SDK preparation rejects a dirty source checkout")
    entries = tree_entries(source)
    dependencies = dependency_records(source, entries)
    sys.path.insert(0, str(source / "scripts/ci"))
    from vendor_import_identity import reviewed_import
    imports = [reviewed_import(source / "vendors/gigadevice/gd32f4xx", source)]
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = Path(tempfile.mkdtemp(prefix=".nexus-source-sdk-", dir=output.parent))
    sdk = temporary / "share/nexus/src"
    sdk.mkdir(parents=True)
    files = {}
    try:
        names = {name for name, (_, kind, _) in entries.items() if kind == "blob" and selected(name)}
        if development_fixture:
            names.update(name.decode() for name in git(source, "ls-files", "--others", "--exclude-standard", "-z").split(b"\0")
                         if name and selected(name.decode()))
        source_blobs = {} if development_fixture else committed_blobs(source, {name: entries[name] for name in names})
        for name in sorted(names):
            entry = entries.get(name)
            if not (source / name).exists() and development_fixture:
                continue
            if entry and entry[0] not in ("100644", "100755"):
                raise SDKError("Source SDK requires regular Git blobs")
            files[name] = copy_file(source, sdk, name, source_blobs.get(name),
                                    entry[0] == "100755" if entry else None)
        for record in dependencies:
            dep = record["path"]
            dep_entries = tree_entries(source / dep)
            blobs = committed_blobs(source / dep, {name: row for name, row in dep_entries.items() if row[1] == "blob"})
            for name, (mode, kind, blob) in dep_entries.items():
                if kind == "commit":
                    continue  # Copied from its own separately verified record.
                if mode not in ("100644", "100755"):
                    raise SDKError("Dependencies require regular tracked files")
                relative = dep + "/" + name
                files[relative] = copy_file(source, sdk, relative, blobs[name], mode == "100755")
        for name in REQUIRED_FILES:
            if name not in files:
                raise SDKError(f"Source SDK is incomplete: {name}")
        if git(source, "rev-parse", "HEAD").decode().strip() != revision:
            raise SDKError("Source commit changed while preparing the package")
        manifest = {"schema": 1, "kind": "nexus-installed-source-sdk",
                    "source_revision": revision, "source_tree": source_tree,
                    "publishable": not development_fixture, "source_dirty": dirty,
                    "files_sha256": files, "dependencies": dependencies,
                    "reviewed_imports": imports,
                    "toolchains_lock_sha256": files["dependencies/toolchains.lock.json"],
                    "snapshot_sha256": digest(json.dumps(files, sort_keys=True, separators=(",", ":")).encode())}
        version_match = re.search(r"project\(nexus VERSION ([0-9]+\.[0-9]+\.[0-9]+)", (sdk / "CMakeLists.txt").read_text())
        if not version_match:
            raise SDKError("Source SDK requires a declared semantic project version")
        config = (sdk / "cmake/package/NexusConfig.cmake.in").read_text().replace("@NEXUS_SDK_VERSION@", version_match[1])
        package = temporary / "lib/cmake/Nexus"
        package.mkdir(parents=True)
        (package / "NexusConfig.cmake").write_text(config)
        version = (sdk / "cmake/package/NexusConfigVersion.cmake.in").read_text().replace("@NEXUS_SDK_VERSION@", version_match[1])
        (package / "NexusConfigVersion.cmake").write_text(version)
        manifest["package_files_sha256"] = {
            "lib/cmake/Nexus/NexusConfig.cmake": digest((package / "NexusConfig.cmake").read_bytes()),
            "lib/cmake/Nexus/NexusConfigVersion.cmake": digest((package / "NexusConfigVersion.cmake").read_bytes())}
        (sdk / MANIFEST).write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")
        verify(sdk)
        temporary.rename(output)
    except BaseException:
        shutil.rmtree(temporary, ignore_errors=True)
        raise
    return manifest


def verify(root):
    root = root.absolute()
    if root.is_symlink() or not root.is_dir() or root.resolve() != root:
        raise SDKError("Installed source SDK root must be a regular directory")
    identity = json.loads(regular(root, MANIFEST).read_text(), object_pairs_hook=object_pairs)
    fields = {"schema", "kind", "source_revision", "source_tree", "publishable", "source_dirty",
              "files_sha256", "dependencies", "reviewed_imports", "toolchains_lock_sha256", "snapshot_sha256",
              "package_files_sha256"}
    if set(identity) != fields or identity["schema"] != 1 or identity["kind"] != "nexus-installed-source-sdk":
        raise SDKError("Unsupported installed source SDK identity")
    if any(not isinstance(identity[key], str) or not HEX40.fullmatch(identity[key]) for key in ("source_revision", "source_tree")):
        raise SDKError("Invalid source Git identity")
    if any(type(identity[key]) is not bool for key in ("publishable", "source_dirty")):
        raise SDKError("Invalid source SDK provenance flags")
    if identity["publishable"] and identity["source_dirty"]:
        raise SDKError("A dirty source SDK cannot be publishable")
    files = identity["files_sha256"]
    if not isinstance(files, dict) or not files:
        raise SDKError("Source SDK requires a complete file identity")
    for name, expected in files.items():
        if not isinstance(expected, str) or not HEX64.fullmatch(expected):
            raise SDKError("Invalid source SDK file digest")
        if digest(regular(root, name).read_bytes()) != expected:
            raise SDKError(f"Source SDK file identity changed: {name}")
    actual = {path.relative_to(root).as_posix() for path in root.rglob("*") if path.is_file() or path.is_symlink()}
    if actual != set(files) | {MANIFEST}:
        raise SDKError("Installed source SDK has missing or additional files")
    if any(name not in files for name in REQUIRED_FILES):
        raise SDKError("Installed source SDK is missing required sources/licenses")
    if identity["snapshot_sha256"] != digest(json.dumps(files, sort_keys=True, separators=(",", ":")).encode()):
        raise SDKError("Source snapshot file-map identity changed")
    if identity["toolchains_lock_sha256"] != files["dependencies/toolchains.lock.json"]:
        raise SDKError("Toolchain lock identity differs from packaged source")
    if root.parts[-3:] != ("share", "nexus", "src"):
        raise SDKError("Relocate the complete installed SDK prefix, not its source subtree")
    prefix = root.parents[2]
    package_files = identity["package_files_sha256"]
    expected_exports = {"lib/cmake/Nexus/NexusConfig.cmake", "lib/cmake/Nexus/NexusConfigVersion.cmake"}
    if not isinstance(package_files, dict) or set(package_files) != expected_exports:
        raise SDKError("Source SDK requires its exported CMake package identity")
    for name, expected in package_files.items():
        if not isinstance(expected, str) or not HEX64.fullmatch(expected) or digest(regular(prefix, name).read_bytes()) != expected:
            raise SDKError("Exported CMake package identity changed")
    records = identity["dependencies"]
    if not isinstance(records, list) or not records:
        raise SDKError("Source SDK requires dependency commit identities")
    paths = set()
    for record in records:
        if not isinstance(record, dict) or set(record) != {"path", "commit", "tree"}:
            raise SDKError("Invalid source SDK dependency record")
        path = str(safe_relative(record["path"]))
        if path in paths or any(not isinstance(record[key], str) or not HEX40.fullmatch(record[key]) for key in ("commit", "tree")):
            raise SDKError("Invalid or duplicate source SDK dependency identity")
        if not any(name.startswith(path + "/") for name in files):
            raise SDKError("Dependency provenance has no packaged source")
        paths.add(path)
    if not set(REQUIRED_DEPENDENCIES).issubset(paths):
        raise SDKError("Source SDK dependency set is incomplete")
    sys.path.insert(0, str(root / "scripts/ci"))
    from vendor_import_identity import validate_import_record
    imports = identity["reviewed_imports"]
    if not isinstance(imports, list) or len(imports) != 1:
        raise SDKError("Source SDK requires its reviewed GD32 source import")
    record = imports[0]
    if not isinstance(record, dict) or record.get("path") != "vendors/gigadevice/gd32f4xx" or record.get("source_tree") != identity["source_tree"]:
        raise SDKError("Source import has a different owner tree")
    imported = root / record["path"]
    notices = {item["path"]: regular(imported, item["path"]).read_bytes() for item in record.get("notices", [])}
    validate_import_record(record, regular(imported, "source.lock.json").read_bytes(), notices, identity["source_revision"])
    return identity


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--development-fixture", action="store_true")
    parser.add_argument("--verify", type=Path)
    args = parser.parse_args(argv)
    try:
        if args.verify:
            if args.source or args.output or args.development_fixture:
                raise SDKError("Verification cannot also prepare a source package")
            result = verify(args.verify)
        elif args.source and args.output:
            result = prepare(args.source, args.output, args.development_fixture)
        else:
            raise SDKError("Use --source/--output to prepare, or --verify to inspect")
    except (SDKError, OSError, ValueError, TypeError, KeyError) as exc:
        print(f"Source SDK rejected: {exc}", file=sys.stderr)
        return 1
    print(json.dumps({"kind": "nexus_source_sdk_summary", "sdk_kind": result["kind"],
                      "source_revision": result["source_revision"],
                      "publishable": result["publishable"], "files": len(result["files_sha256"]),
                      "snapshot_sha256": result["snapshot_sha256"]}, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
