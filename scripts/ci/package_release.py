#!/usr/bin/env python3
"""Validate release tags and package checked build outputs using only stdlib.

The recorded build provenance and SHA-256 checksums are unsigned. They do not
constitute a reproducible-build claim, firmware signature, or hardware test.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import stat
import subprocess
import sys
import zipfile


VERSION_RE = re.compile(
    r"v(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)"
    r"(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?\Z"
)
NAME_RE = re.compile(r"[A-Za-z0-9][A-Za-z0-9_.-]*\Z")
CACHE_KEYS = (
    "CMAKE_BUILD_TYPE", "CMAKE_GENERATOR", "CMAKE_C_COMPILER",
    "CMAKE_CXX_COMPILER", "CMAKE_TOOLCHAIN_FILE", "CMAKE_C_FLAGS",
    "CMAKE_C_FLAGS_RELEASE", "CMAKE_CXX_FLAGS_RELEASE", "NEXUS_PLATFORM",
    "NEXUS_TOOLCHAIN_NAME",
    "NEXUS_OSAL_BACKEND", "NEXUS_BUILD_TESTS", "NEXUS_BUILD_EXAMPLES",
    "NEXUS_CPU_ARCH", "NEXUS_FPU_TYPE", "NEXUS_FLOAT_ABI",
)
# Deliberately explicit: extending the release matrix requires a reviewed target
# profile rather than treating a filename/preset label as evidence of its target.
RELEASE_PROFILES = {
    "windows-msvc-release": {
        "artifact": "nexus-windows-msvc", "platform": "native", "compiler": "cl",
    },
    "linux-gcc-release": {
        "artifact": "nexus-linux-gcc", "platform": "native", "compiler": "gcc",
    },
    "macos-clang-release": {
        "artifact": "nexus-macos-clang", "platform": "native", "compiler": "clang",
    },
    "linux-stm32-armgcc-release": {
        "artifact": "nexus-arm-cortex-m4", "platform": "stm32",
        "compiler": "arm-none-eabi-gcc", "cpu": "cortex-m4",
        "fpu": "fpv4-sp-d16", "float_abi": "hard", "toolchain": "arm-gcc",
        "toolchain_file": "cmake/toolchains/arm-gcc.cmake",
    },
}


class ReleaseError(ValueError):
    """A release prerequisite is missing or unsafe."""


def git(source, *args):
    result = subprocess.run(
        ["git", "-C", str(source), *args], check=False,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
    )
    if result.returncode:
        raise ReleaseError(f"Git validation failed: {args[0]}")
    return result.stdout.strip()


def validate_name(value, label):
    if not NAME_RE.fullmatch(value) or value in (".", "..") or value.endswith("."):
        raise ReleaseError(f"Invalid {label}: {value!r}")
    return value


def validate_archive_path(value):
    path = PurePosixPath(value)
    if (path.is_absolute() or ".." in path.parts or path.as_posix() != value
            or any(character in value for character in ("\\", ":"))
            or any(ord(character) < 32 or ord(character) == 127 for character in value)
            or any(part.endswith((".", " ")) for part in path.parts)):
        raise ReleaseError("Archive contains an unsafe entry")
    return path


def validate_version(value):
    match = VERSION_RE.fullmatch(value)
    if not match:
        raise ReleaseError("Version must be vMAJOR.MINOR.PATCH with optional prerelease")
    if match[4] and any(p.isdigit() and len(p) > 1 and p[0] == "0"
                        for p in match[4].split(".")):
        raise ReleaseError("Numeric prerelease identifiers cannot have leading zeros")
    return value


def checked_path(path, source, *, directory=False, required=True):
    """Reject traversal, symlinks, and paths outside the declared source root."""
    path = Path(path)
    if not path.is_absolute():
        path = source / path
    if ".." in path.parts:
        raise ReleaseError(f"Path traversal is not allowed: {path}")
    try:
        relative = path.relative_to(source)
    except ValueError as exc:
        raise ReleaseError(f"Path is outside the source tree: {path}") from exc
    current = source
    for part in relative.parts:
        current = current / part
        if current.is_symlink():
            raise ReleaseError(f"Symlinks are not allowed: {current}")
    if path.resolve() != path:
        raise ReleaseError(f"Path resolves outside its declared location: {path}")
    if required and not (path.is_dir() if directory else path.is_file()):
        raise ReleaseError(f"Required {'directory' if directory else 'file'} missing: {path}")
    if path.exists() and not (path.is_dir() if directory else path.is_file()):
        raise ReleaseError(f"Unexpected file type: {path}")
    return path


def source_root(value):
    source = Path(value).absolute()
    if source.is_symlink() or not source.is_dir():
        raise ReleaseError("Source must be an existing directory, not a symlink")
    return source.resolve()


def metadata(source, version, expected_commit=None):
    validate_version(version)
    commit = git(source, "rev-parse", "--verify", "HEAD^{commit}")
    tag_commit = git(source, "rev-parse", "--verify", f"refs/tags/{version}^{{commit}}")
    if commit != tag_commit:
        raise ReleaseError("Version tag does not point to the checked-out source commit")
    if expected_commit is not None and commit != expected_commit:
        raise ReleaseError("Checked-out source differs from the validated release commit")
    return {"version": version, "commit": commit, "prerelease": "-" in version}


def sha256_file(path):
    with path.open("rb") as handle:
        return hashlib.file_digest(handle, "sha256").hexdigest()


def cmake_cache(path):
    values = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if line.startswith(("#", "//")) or ":" not in line or "=" not in line:
            continue
        key_type, value = line.split("=", 1)
        key, _ = key_type.split(":", 1)
        values[key] = value
    return values


def release_profile(preset, artifact):
    if preset not in RELEASE_PROFILES:
        raise ReleaseError(f"Unsupported release preset: {preset}")
    profile = RELEASE_PROFILES[preset]
    if profile["artifact"] != artifact:
        raise ReleaseError("Release artifact does not match its expected preset")
    return profile


def validate_effective_build(cache, profile, configuration):
    if configuration != "Release" or cache.get("CMAKE_BUILD_TYPE") != "Release":
        raise ReleaseError("Requested configuration does not match an effective Release build")
    if cache.get("NEXUS_PLATFORM") != profile["platform"]:
        raise ReleaseError("Effective platform does not match the release target")
    compiler_path = cache.get("CMAKE_C_COMPILER", "").replace("\\", "/")
    compiler_name = PurePosixPath(compiler_path).name.lower()
    if compiler_name.endswith(".exe"):
        compiler_name = compiler_name[:-4]
    if compiler_name != profile["compiler"]:
        raise ReleaseError("Effective compiler does not match the release target")
    if profile["platform"] == "native":
        if cache.get("CMAKE_TOOLCHAIN_FILE"):
            raise ReleaseError("Native release target unexpectedly uses a cross-toolchain file")
        return
    for field, expected in (
        ("NEXUS_CPU_ARCH", profile["cpu"]), ("NEXUS_FPU_TYPE", profile["fpu"]),
        ("NEXUS_FLOAT_ABI", profile["float_abi"]), ("NEXUS_TOOLCHAIN_NAME", profile["toolchain"]),
    ):
        if cache.get(field) != expected:
            raise ReleaseError(f"Effective {field} does not match the release target")
    toolchain_file = cache.get("CMAKE_TOOLCHAIN_FILE", "").replace("\\", "/")
    if not (toolchain_file == profile["toolchain_file"]
            or toolchain_file.endswith("/" + profile["toolchain_file"])):
        raise ReleaseError("Effective toolchain file does not match the release target")


def compiled_header(header, size):
    """Recognize executable/object-library formats; raw .bin/.hex cannot qualify."""
    if size >= 64 and header.startswith(b"\x7fELF"):
        return True
    if size > 8 and header.startswith(b"!<arch>\n"):
        return True
    if size >= 32 and header[:4] in (
        b"\xfe\xed\xfa\xce", b"\xce\xfa\xed\xfe", b"\xfe\xed\xfa\xcf",
        b"\xcf\xfa\xed\xfe", b"\xca\xfe\xba\xbe", b"\xbe\xba\xfe\xca",
        b"\xca\xfe\xba\xbf", b"\xbf\xba\xfe\xca",
    ):
        return True
    if size >= 64 and header.startswith(b"MZ"):
        offset = int.from_bytes(header[60:64], "little")
        return offset >= 64 and header[offset:offset + 4] == b"PE\x00\x00"
    return False


def compiled_file(path):
    with path.open("rb") as handle:
        header = handle.read(4096)
    return compiled_header(header, path.stat().st_size)


def build_outputs(build, source, configuration):
    files = []
    for name in ("bin", "lib"):
        root = checked_path(build / name, source, directory=True, required=False)
        if not root.exists():
            continue
        for current, directories, names in os.walk(root, followlinks=False):
            directories.sort()
            for directory in directories:
                checked_path(Path(current) / directory, source, directory=True)
            for filename in sorted(names):
                path = checked_path(Path(current) / filename, source)
                relative = path.relative_to(build)
                # A multi-config generator may leave other configurations behind.
                if len(relative.parts) > 2 and relative.parts[1] in (
                    "Debug", "Release", "RelWithDebInfo", "MinSizeRel"
                ) and relative.parts[1] != configuration:
                    continue
                validate_archive_path(relative.as_posix())
                files.append((relative.as_posix(), path))
    if not any(compiled_file(path) for _, path in files):
        raise ReleaseError("No compiled executable or library found under build bin/lib")
    return files


def submodules(source):
    modules = []
    output = git(source, "submodule", "status", "--recursive")
    # git() strips the first leading status space, so restore that marker only.
    if output and re.match(r"[0-9a-f]{40,64} ", output):
        output = " " + output
    for line in output.splitlines():
        match = re.fullmatch(r" ([0-9a-f]{40,64}) (.+?)(?: \([^\n]+\))?", line)
        if not match:
            raise ReleaseError("Submodules must be initialized at their recorded commits")
        modules.append({"path": match[2], "commit": match[1]})
    return modules


def package(source, version, commit, preset, artifact, build, output, configuration):
    info = metadata(source, version, commit)
    validate_name(preset, "preset")
    validate_name(artifact, "artifact")
    validate_name(configuration, "configuration")
    profile = release_profile(preset, artifact)
    build = checked_path(build, source, directory=True)
    output = checked_path(output, source, directory=True, required=False)
    cache = cmake_cache(checked_path(build / "CMakeCache.txt", source))
    validate_effective_build(cache, profile, configuration)
    entries = build_outputs(build, source, configuration)
    compiled = [name for name, path in entries if compiled_file(path)]
    config_paths = {}
    for key, target in (
        ("NEXUS_CONFIG_FILE", "configuration/.config"),
        ("NEXUS_CONFIG_HEADER", "configuration/nexus_config.h"),
    ):
        if not cache.get(key):
            raise ReleaseError(f"CMake cache does not record {key}")
        path = checked_path(cache[key], source)
        entries.append((target, path))
        config_paths[key] = path.relative_to(source).as_posix()
    for filename in ("CMakePresets.json", "README.md", "LICENSE"):
        entries.append((filename, checked_path(source / filename, source)))
    gitmodules = checked_path(source / ".gitmodules", source, required=False)
    if gitmodules.exists():
        entries.append(("configuration/gitmodules", gitmodules))
    toolchain = cache.get("CMAKE_TOOLCHAIN_FILE")
    if toolchain:
        entries.append(("configuration/toolchain.cmake", checked_path(toolchain, source)))
    dirty = git(source, "status", "--porcelain", "--untracked-files=no")
    if dirty:
        raise ReleaseError("Tracked source files changed after checkout; provenance would be ambiguous")
    provenance = {
        "schema_version": 1, **info, "artifact": artifact, "preset": preset,
        "configuration": configuration, "source_worktree_dirty": False,
        "configuration_paths": config_paths,
        "cmake": {key: cache[key] for key in CACHE_KEYS if key in cache},
        "submodules": submodules(source), "compiled_outputs": compiled,
        "files": {name: {"sha256": sha256_file(path), "size": path.stat().st_size}
                  for name, path in entries},
        "limitations": ["unsigned provenance", "not a reproducible-build attestation",
                        "not firmware signing or hardware qualification"],
    }
    provenance_bytes = (json.dumps(provenance, indent=2, sort_keys=True) + "\n").encode()
    hashes = {name: data["sha256"] for name, data in provenance["files"].items()}
    hashes["provenance.json"] = hashlib.sha256(provenance_bytes).hexdigest()
    checksum_bytes = "".join(f"{digest}  {name}\n" for name, digest in sorted(hashes.items())).encode()
    output.mkdir(parents=True, exist_ok=True)
    archive = output / f"{artifact}-{version}.zip"
    checksum = output / f"{archive.name}.sha256"
    if archive.exists() or checksum.exists() or archive.is_symlink() or checksum.is_symlink():
        raise ReleaseError("Refusing to replace existing release assets")
    created = []
    try:
        with zipfile.ZipFile(archive, "x", compression=zipfile.ZIP_DEFLATED) as bundle:
            created.append(archive)
            for name, path in entries:
                bundle.write(path, f"{artifact}/{name}")
            bundle.writestr(f"{artifact}/provenance.json", provenance_bytes)
            bundle.writestr(f"{artifact}/SHA256SUMS", checksum_bytes)
        with checksum.open("x", encoding="utf-8", newline="\n") as handle:
            created.append(checksum)
            handle.write(f"{sha256_file(archive)}  {archive.name}\n")
    except Exception:
        for path in created:
            path.unlink(missing_ok=True)
        raise
    return archive


def parse_checksums(value):
    checksums = {}
    for line in value.splitlines():
        match = re.fullmatch(r"([0-9a-f]{64})  (.+)", line)
        if not match or match[2] in checksums:
            raise ReleaseError("Invalid or duplicate SHA256SUMS entry")
        checksums[match[2]] = match[1]
    return checksums


def verify_bundle(archive, artifact, preset, version, commit):
    profile = release_profile(preset, artifact)
    with zipfile.ZipFile(archive) as bundle:
        names = bundle.namelist()
        if len(names) != len(set(names)):
            raise ReleaseError("Archive contains duplicate entries")
        for entry in bundle.infolist():
            path = validate_archive_path(entry.filename)
            if (len(path.parts) < 2 or path.parts[0] != artifact
                    or stat.S_ISLNK(entry.external_attr >> 16) or entry.is_dir()):
                raise ReleaseError("Archive contains an unsafe entry")
        provenance = json.loads(bundle.read(f"{artifact}/provenance.json"))
        for key, value in (("artifact", artifact), ("preset", preset),
                           ("version", version), ("commit", commit)):
            if provenance.get(key) != value:
                raise ReleaseError(f"Archive provenance does not match {key}")
        if not provenance.get("compiled_outputs") or provenance.get("source_worktree_dirty") is not False:
            raise ReleaseError("Archive lacks clean compiled-output provenance")
        validate_effective_build(provenance.get("cmake", {}), profile,
                                 provenance.get("configuration"))
        checksums = parse_checksums(bundle.read(f"{artifact}/SHA256SUMS").decode())
        expected_names = {f"{artifact}/{name}" for name in checksums}
        if expected_names != set(names) - {f"{artifact}/SHA256SUMS"}:
            raise ReleaseError("Archive checksum manifest does not cover its contents")
        for name, digest in checksums.items():
            with bundle.open(f"{artifact}/{name}") as handle:
                if hashlib.file_digest(handle, "sha256").hexdigest() != digest:
                    raise ReleaseError("Archive member checksum mismatch")
        for name in provenance["compiled_outputs"]:
            member = bundle.getinfo(f"{artifact}/{name}")
            with bundle.open(member) as handle:
                if not compiled_header(handle.read(4096), member.file_size):
                    raise ReleaseError("Archive compiled output has an unrecognized format")


def verify_assets(source, version, commit, assets, expected, notes):
    metadata(source, version, commit)
    assets = checked_path(assets, source, directory=True)
    notes = checked_path(notes, source, required=False)
    expected_map = {}
    for value in expected:
        if "=" not in value:
            raise ReleaseError("Expected artifact must have ARTIFACT=PRESET form")
        artifact, preset = value.split("=", 1)
        validate_name(artifact, "artifact")
        validate_name(preset, "preset")
        release_profile(preset, artifact)
        if artifact in expected_map:
            raise ReleaseError("Duplicate expected artifact")
        expected_map[artifact] = preset
    required = {f"{artifact}-{version}.zip{suffix}" for artifact in expected_map
                for suffix in ("", ".sha256")}
    if {path.name for path in assets.iterdir()} != required:
        raise ReleaseError("Downloaded release assets do not match the complete build matrix")
    lines = []
    for artifact, preset in sorted(expected_map.items()):
        archive = checked_path(assets / f"{artifact}-{version}.zip", source)
        sidecar = checked_path(assets / f"{archive.name}.sha256", source)
        actual = sha256_file(archive)
        if parse_checksums(sidecar.read_text(encoding="utf-8")) != {archive.name: actual}:
            raise ReleaseError("Downloaded archive checksum mismatch")
        verify_bundle(archive, artifact, preset, version, commit)
        lines.append(f"{actual}  {archive.name}\n")
    sums = checked_path(assets / "SHA256SUMS", source, required=False)
    with sums.open("x", encoding="utf-8", newline="\n") as handle:
        handle.writelines(lines)
    with notes.open("x", encoding="utf-8", newline="\n") as handle:
        handle.write(
            f"# Nexus {version} release candidate\n\n"
            "This draft stages outputs from the complete release build matrix. "
            "A maintainer must review and publish it explicitly.\n\n"
            f"Source commit: `{commit}`.\n\n"
            "Native host tests passed before packaging; the ARM target was cross-compiled. "
            "Hardware qualification and firmware signing are not provided by this workflow.\n\n"
            "Archives include build configuration, submodule commits, and unsigned provenance. "
            "SHA256SUMS checks transfer integrity; it is not an authenticity signature or "
            "a reproducible-build attestation.\n\n"
            + "".join(f"- `{artifact}`: `{preset}`\n" for artifact, preset in sorted(expected_map.items()))
        )


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    for command in ("metadata", "package", "verify-assets"):
        sub = subparsers.add_parser(command)
        sub.add_argument("--source", default=".")
        sub.add_argument("--version", required=True)
        if command != "metadata":
            sub.add_argument("--source-commit", required=True)
        if command == "package":
            sub.add_argument("--preset", required=True)
            sub.add_argument("--artifact", required=True)
            sub.add_argument("--build-dir", required=True)
            sub.add_argument("--output-dir", default="release")
            sub.add_argument("--configuration", default="Release")
        elif command == "verify-assets":
            sub.add_argument("--assets-dir", default="release")
            sub.add_argument("--expected", action="append", required=True)
            sub.add_argument("--notes", default="release_notes.md")
    args = parser.parse_args(argv)
    try:
        source = source_root(args.source)
        if args.command == "metadata":
            result = metadata(source, args.version)
            output = os.environ.get("GITHUB_OUTPUT")
            if output:
                with open(output, "a", encoding="utf-8") as handle:
                    handle.write(
                        f"version={result['version']}\ncommit={result['commit']}\n"
                        f"prerelease={str(result['prerelease']).lower()}\n"
                    )
            print(json.dumps(result, sort_keys=True))
        elif args.command == "package":
            print(package(source, args.version, args.source_commit, args.preset, args.artifact,
                          args.build_dir, args.output_dir, args.configuration))
        else:
            verify_assets(source, args.version, args.source_commit, args.assets_dir,
                          args.expected, args.notes)
            print("Complete candidate artifact set and checksums verified")
    except (ReleaseError, OSError, zipfile.BadZipFile, KeyError, json.JSONDecodeError) as exc:
        print(f"Release validation failed: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
