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
import shlex
import stat
import subprocess
import sys
import xml.etree.ElementTree as ET
import zipfile


VERSION_RE = re.compile(
    r"v(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)"
    r"(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?\Z"
)
NAME_RE = re.compile(r"[A-Za-z0-9][A-Za-z0-9_.-]*\Z")
CACHE_KEYS = (
    "CMAKE_BUILD_TYPE", "CMAKE_GENERATOR", "CMAKE_HOME_DIRECTORY", "CMAKE_C_COMPILER",
    "CMAKE_CXX_COMPILER", "CMAKE_TOOLCHAIN_FILE", "CMAKE_C_FLAGS",
    "CMAKE_C_FLAGS_RELEASE", "CMAKE_CXX_FLAGS_RELEASE", "NEXUS_PLATFORM",
    "NEXUS_OSAL_BACKEND", "NEXUS_BUILD_TESTS", "NEXUS_BUILD_EXAMPLES",
    "NEXUS_ENABLE_COVERAGE", "NEXUS_ENABLE_SANITIZERS",
)
# Deliberately explicit: extending the release matrix requires a reviewed target
# profile rather than treating a filename/preset label as evidence of its target.
RELEASE_PROFILES = {
    "linux-gcc-release": {
        "artifact": "nexus-linux-gcc", "platform": "native", "compiler": "gcc",
        "board": "native-reference", "chip": None, "osal": "native",
        "support_profile": "native-contracts", "architecture": "x86_64",
        "toolchain": "gcc", "fragment": "platforms/native/defconfig",
        "dependencies": ("ext/googletest", "ext/freertos"),
    },
    "stm32-armgcc-release": {
        "artifact": "nexus-stm32f407-baremetal", "platform": "stm32",
        "compiler": "arm-none-eabi-gcc", "cpu": "cortex-m4",
        "fpu": "fpv4-sp-d16", "float_abi": "hard", "toolchain": "arm-none-eabi-gcc",
        "toolchain_file": "cmake/toolchains/arm-gcc.cmake",
        "board": "stm32f4discovery-mb997", "chip": "STM32F407xx", "osal": "baremetal",
        "support_profile": "stm32f407-discovery-baremetal", "architecture": "armv7e-m",
        "fragment": "configs/stm32f407_baremetal_defconfig",
        "dependencies": ("vendors/arm/CMSIS_5", "vendors/st/cmsis_device_f4",
                         "vendors/st/stm32f4xx_hal_driver"),
    },
    "stm32-armgcc-freertos-release": {
        "artifact": "nexus-stm32f407-freertos", "platform": "stm32",
        "compiler": "arm-none-eabi-gcc", "cpu": "cortex-m4",
        "fpu": "fpv4-sp-d16", "float_abi": "hard", "toolchain": "arm-none-eabi-gcc",
        "toolchain_file": "cmake/toolchains/arm-gcc.cmake",
        "board": "stm32f4discovery-mb997", "chip": "STM32F407xx", "osal": "freertos",
        "support_profile": "stm32f407-discovery-freertos", "architecture": "armv7e-m",
        "fragment": "configs/stm32f407_freertos_defconfig",
        "dependencies": ("ext/freertos", "vendors/arm/CMSIS_5", "vendors/st/cmsis_device_f4",
                         "vendors/st/stm32f4xx_hal_driver"),
    },
}
CONFIGURATION_FILES = ("effective.config", "nexus_config.h", "config.cmake")


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
    return cmake_cache_contents(path.read_text(encoding="utf-8"))


def cmake_cache_contents(contents):
    values = {}
    for line in contents.splitlines():
        if line.startswith(("#", "//")) or ":" not in line or "=" not in line:
            continue
        key_type, value = line.split("=", 1)
        key, _ = key_type.split(":", 1)
        if key in values:
            raise ReleaseError("CMake cache contains duplicate entries")
        values[key] = value
    return values


def release_profile(preset, artifact):
    if preset not in RELEASE_PROFILES:
        raise ReleaseError(f"Unsupported release preset: {preset}")
    profile = RELEASE_PROFILES[preset]
    if profile["artifact"] != artifact:
        raise ReleaseError("Release artifact does not match its expected preset")
    return profile


def config_value(value):
    if value in ("y", "n"):
        return value == "y"
    if value.startswith('"'):
        result = json.loads(value)
        if isinstance(result, str):
            return result
    if re.fullmatch(r"-?[0-9]+|0[xX][0-9a-fA-F]+", value):
        return int(value, 16 if value.lower().startswith("0x") else 10)
    raise ReleaseError("Invalid effective configuration value")


def effective_config(value):
    values = {}
    for line in value.splitlines():
        assigned = re.fullmatch(r"(CONFIG_[A-Za-z0-9_]+)=(.+)", line)
        unset = re.fullmatch(r"# (CONFIG_[A-Za-z0-9_]+) is not set", line)
        if assigned:
            key, result = assigned[1], config_value(assigned[2])
        elif unset:
            key, result = unset[1], False
        elif not line or line.startswith("#"):
            continue
        else:
            raise ReleaseError("Malformed effective configuration")
        if key in values:
            raise ReleaseError("Duplicate effective configuration symbol")
        values[key] = result
    if not values:
        raise ReleaseError("Effective configuration is empty")
    return values


def generated_cmake(value):
    values = {}
    for line in value.splitlines():
        if not line or line.startswith("#"):
            continue
        match = re.fullmatch(r"set\((CONFIG_[A-Za-z0-9_]+) \[(=+)\[(.*?)\]\2\]\)", line)
        if not match or match[1] in values:
            raise ReleaseError("Malformed or duplicate generated CMake configuration")
        values[match[1]] = match[3]
    return values


def generated_header(value, symbols):
    values = {}
    derived = False
    for line in value.splitlines():
        # Everything below this marker is conditional derived helper macros,
        # including both branches of *_ENABLED. It is not the symbol table.
        if "Peripheral Instance Traversal Macros" in line:
            derived = True
            continue
        defined = re.fullmatch(r"#define NX_(CONFIG_[A-Za-z0-9_]+) (.+)", line)
        unset = re.fullmatch(r"/\* #undef NX_(CONFIG_[A-Za-z0-9_]+) \*/", line)
        if defined:
            key, result = defined[1], config_value(defined[2])
        elif unset:
            key, result = unset[1], False
        elif "NX_CONFIG_" in line and not re.fullmatch(r"#ifdef NX_CONFIG_[A-Za-z0-9_]+", line):
            raise ReleaseError("Malformed generated configuration header")
        else:
            continue
        if derived:
            if key in symbols:
                raise ReleaseError("Generated helpers redefine an effective configuration symbol")
            continue
        if key in values:
            raise ReleaseError("Duplicate generated header symbol")
        values[key] = result
    return values


def validate_configuration_bundle(contents):
    """Read data without executing CMake or trusting removed cache aliases."""
    values = effective_config(contents["effective.config"])
    cmake = generated_cmake(contents["config.cmake"])
    header = generated_header(contents["nexus_config.h"], cmake)
    for key, value in values.items():
        expected_cmake = ("ON" if value else "OFF") if isinstance(value, bool) else str(value)
        actual_cmake = cmake.get(key)
        if isinstance(value, int) and not isinstance(value, bool) and actual_cmake is not None:
            try:
                actual_cmake = str(config_value(actual_cmake))
            except ReleaseError:
                pass
        if actual_cmake != expected_cmake:
            raise ReleaseError(f"Effective configuration and CMake disagree on {key}")
        # The generator omits empty/inactive non-boolean symbols from the header.
        expected_header = 1 if value is True else value
        if value == "":
            valid = key not in header
        else:
            valid = (key in header and type(header[key]) is type(expected_header)
                     and header[key] == expected_header)
        if not valid:
            raise ReleaseError(f"Effective configuration and header disagree on {key}")
    for key in set(cmake) - set(values):
        if not ((cmake[key] == "OFF" and header.get(key) is False)
                or (cmake[key] == "" and key not in header)):
            raise ReleaseError(f"Generated configuration contains an unrecorded value: {key}")
    if set(header) - set(cmake):
        raise ReleaseError("Generated header contains unrecorded configuration symbols")
    return values


def validate_effective_build(cache, profile, configuration, config):
    if configuration != "Release" or cache.get("CMAKE_BUILD_TYPE") != "Release":
        raise ReleaseError("Requested configuration does not match an effective Release build")
    if config.get("CONFIG_BUILD_TYPE") != "Release" or config.get("CONFIG_BUILD_TYPE_RELEASE") is not True:
        raise ReleaseError("Effective configuration does not describe a Release build")
    if cache.get("NEXUS_PLATFORM") != profile["platform"]:
        raise ReleaseError("Effective platform does not match the release target")
    compiler_path = cache.get("CMAKE_C_COMPILER", "").replace("\\", "/")
    compiler_name = PurePosixPath(compiler_path).name.lower()
    if compiler_name.endswith(".exe"):
        compiler_name = compiler_name[:-4]
    if compiler_name != profile["compiler"]:
        raise ReleaseError("Effective compiler does not match the release target")
    for field, expected in (
        ("CONFIG_PLATFORM_NAME", profile["platform"]), ("CONFIG_BOARD_NAME", profile["board"]),
        ("CONFIG_OSAL_BACKEND_NAME", profile["osal"]), ("CONFIG_TOOLCHAIN_NAME", profile["toolchain"]),
    ):
        if config.get(field) != expected:
            raise ReleaseError(f"Effective {field} does not match the release target")
    if cache.get("NEXUS_OSAL_BACKEND") and cache["NEXUS_OSAL_BACKEND"] != profile["osal"]:
        raise ReleaseError("Effective OSAL cache constraint disagrees with the release target")
    for option, symbol in (("NEXUS_BUILD_TESTS", "CONFIG_BUILD_TESTS"),
                           ("NEXUS_BUILD_EXAMPLES", "CONFIG_BUILD_EXAMPLES"),
                           ("NEXUS_ENABLE_COVERAGE", "CONFIG_ENABLE_COVERAGE"),
                           ("NEXUS_ENABLE_SANITIZERS", "CONFIG_ENABLE_SANITIZERS")):
        expected = config.get(symbol)
        if not isinstance(expected, bool) or cache.get(option) not in ("ON", "OFF"):
            raise ReleaseError(f"Missing effective build option: {option}")
        if (cache[option] == "ON") != expected:
            raise ReleaseError(f"Effective configuration disagrees with {option}")
    if config["CONFIG_ENABLE_COVERAGE"] or config["CONFIG_ENABLE_SANITIZERS"]:
        raise ReleaseError("Instrumented output cannot form a release candidate")
    if not config["CONFIG_BUILD_EXAMPLES"]:
        raise ReleaseError("Release candidates require built reference applications")
    if profile["platform"] == "native":
        if cache.get("CMAKE_TOOLCHAIN_FILE"):
            raise ReleaseError("Native release target unexpectedly uses a cross-toolchain file")
        if not config["CONFIG_BUILD_TESTS"]:
            raise ReleaseError("Native release requires host tests")
        return
    for field, expected in (
        ("CONFIG_CPU_ARCH", profile["cpu"]), ("CONFIG_FPU_TYPE", profile["fpu"]),
        ("CONFIG_FLOAT_ABI", profile["float_abi"]), ("CONFIG_STM32_CHIP_NAME", profile["chip"]),
    ):
        if config.get(field) != expected:
            raise ReleaseError(f"Effective {field} does not match the release target")
    toolchain_file = cache.get("CMAKE_TOOLCHAIN_FILE", "").replace("\\", "/")
    if not (toolchain_file == profile["toolchain_file"]
            or toolchain_file.endswith("/" + profile["toolchain_file"])):
        raise ReleaseError("Effective toolchain file does not match the release target")


def target_identity(profile):
    return {key: profile[key] for key in ("platform", "board", "chip", "osal", "architecture",
                                         "toolchain", "support_profile")} | {
        "board_revision": None, "hardware_verified": False,
    }


def test_report(contents):
    try:
        root = ET.fromstring(contents)
    except ET.ParseError as exc:
        raise ReleaseError("Invalid host test report") from exc
    if root.tag not in ("testsuite", "testsuites"):
        raise ReleaseError("Invalid host test report root")
    cases = list(root.iter("testcase"))
    if not cases:
        raise ReleaseError("Host test report contains zero tests")
    if any(node.tag in ("failure", "error") for node in root.iter()):
        raise ReleaseError("Host test report contains failing tests")
    for suite in (node for node in root.iter() if node.tag in ("testsuite", "testsuites")):
        for key in ("failures", "errors"):
            if suite.get(key, "0") != "0":
                raise ReleaseError("Host test report contains failing tests")
        try:
            if suite.get("tests") is not None and int(suite.get("tests")) != len(list(suite.iter("testcase"))):
                raise ReleaseError("Host test report count disagrees with executed entries")
        except ValueError as exc:
            raise ReleaseError("Host test report contains an invalid test count") from exc
    skipped = sum(case.find("skipped") is not None for case in cases)
    executed = len(cases) - skipped
    if executed <= 0 or any(case.get("status", "run") != "run" and case.find("skipped") is None for case in cases):
        raise ReleaseError("Host test report has no complete successful execution")
    return {"kind": "native-host-tests", "executed": executed, "skipped": skipped,
            "failed": 0, "hardware_verified": False}


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


def validate_target_outputs(outputs, profile):
    machine = 62 if profile["platform"] == "native" else 40
    elf_class = 2 if profile["platform"] == "native" else 1
    executable = False
    for name, header in outputs:
        if not header.startswith(b"\x7fELF"):
            continue
        if (len(header) < 20 or header[4] != elf_class or header[5] != 1
                or int.from_bytes(header[18:20], "little") != machine):
            raise ReleaseError("Compiled ELF architecture does not match the release target")
        if name.startswith("bin/") and int.from_bytes(header[16:18], "little") in (2, 3):
            executable = True
    if not executable:
        raise ReleaseError("No target ELF reference application found under build bin")


def validate_compile_commands(contents, profile, generated_directory=None, source=None):
    commands = json.loads(contents)
    if not isinstance(commands, list) or not commands:
        raise ReleaseError("Compile command database is empty or invalid")
    production = []
    for entry in commands:
        if not isinstance(entry, dict) or not isinstance(entry.get("file"), str):
            raise ReleaseError("Compile command database contains an invalid entry")
        filename = entry["file"].replace("\\", "/")
        if not any(f"/{directory}/" in filename for directory in
                   ("hal", "osal", "framework", "services", "platforms", "boards", "soc", "applications")):
            continue
        if source is not None:
            checked_path(filename, source)
        arguments = entry.get("arguments")
        if arguments is None and isinstance(entry.get("command"), str):
            arguments = shlex.split(entry["command"])
        if not isinstance(arguments, list) or not all(isinstance(arg, str) for arg in arguments):
            raise ReleaseError("Compile command database contains an invalid command")
        if generated_directory is not None and f"-I{generated_directory}" not in arguments:
            raise ReleaseError("Production compile command does not consume its generated configuration")
        if profile["platform"] == "stm32":
            required = {f"-mcpu={profile['cpu']}", "-mthumb", f"-mfpu={profile['fpu']}",
                        f"-mfloat-abi={profile['float_abi']}"}
            if not required.issubset(arguments):
                raise ReleaseError("Production compile command does not match the ARM target flags")
        production.append(entry)
    if not production:
        raise ReleaseError("Compile command database contains no Nexus production source")


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


def submodules(source, paths=()):
    modules = []
    arguments = ("--", *paths) if paths else ()
    output = git(source, "submodule", "status", "--recursive", *arguments)
    # git() strips the first leading status space, so restore that marker only.
    if output and re.match(r"[0-9a-f]{40,64} ", output):
        output = " " + output
    for line in output.splitlines():
        match = re.fullmatch(r" ([0-9a-f]{40,64}) (.+?)(?: \([^\n]+\))?", line)
        if not match:
            raise ReleaseError("Submodules must be initialized at their recorded commits")
        modules.append({"path": match[2], "commit": match[1]})
    if paths and not set(paths).issubset(module["path"] for module in modules):
        raise ReleaseError("Required release dependencies are missing")
    return modules


def validate_dependency_identity(source, modules, profile):
    recorded = {module.get("path"): module.get("commit") for module in modules}
    if len(recorded) != len(modules) or not set(profile["dependencies"]).issubset(recorded):
        raise ReleaseError("Archive lacks the required dependency identity")
    for path in profile["dependencies"]:
        entry = git(source, "ls-tree", "HEAD", "--", path)
        match = re.fullmatch(r"160000 commit ([0-9a-f]{40,64})\t" + re.escape(path), entry)
        if not match or recorded[path] != match[1]:
            raise ReleaseError("Archive dependency identity does not match the source commit")


def validate_clean_source(source, build, output, version):
    if git(source, "status", "--porcelain", "--untracked-files=no"):
        raise ReleaseError("Tracked source files changed after checkout; provenance would be ambiguous")
    build_relative = build.relative_to(source)
    output_relative = output.relative_to(source)
    if build_relative == Path(".") or output_relative == Path("."):
        raise ReleaseError("Release build and output directories must be separate from the source root")
    assets = {output_relative / f"{profile['artifact']}-{version}.zip{suffix}"
              for profile in RELEASE_PROFILES.values() for suffix in ("", ".sha256")}
    for filename in git(source, "ls-files", "--others", "--exclude-standard").splitlines():
        path = Path(filename)
        if not path.is_relative_to(build_relative) and path not in assets:
            raise ReleaseError("Untracked source files could affect the build; commit them before packaging")


def package(source, version, commit, preset, artifact, build, output, configuration):
    info = metadata(source, version, commit)
    validate_name(preset, "preset")
    validate_name(artifact, "artifact")
    validate_name(configuration, "configuration")
    profile = release_profile(preset, artifact)
    build = checked_path(build, source, directory=True)
    output = checked_path(output, source, directory=True, required=False)
    cache = cmake_cache(checked_path(build / "CMakeCache.txt", source))
    if not cache.get("CMAKE_HOME_DIRECTORY") or checked_path(cache["CMAKE_HOME_DIRECTORY"], source, directory=True) != source:
        raise ReleaseError("Build cache does not identify the selected source tree")
    config_paths = {}
    configuration_entries = []
    contents = {}
    for name in CONFIGURATION_FILES:
        path = checked_path(build / "generated" / name, source)
        contents[name] = path.read_text(encoding="utf-8")
        target = f"configuration/{name}"
        config_paths[target] = path.relative_to(source).as_posix()
        configuration_entries.append((target, path))
    config = validate_configuration_bundle(contents)
    validate_effective_build(cache, profile, configuration, config)
    entries = build_outputs(build, source, configuration)
    validate_target_outputs(((name, path.read_bytes()[:64]) for name, path in entries), profile)
    compiled = [name for name, path in entries if compiled_file(path)]
    entries.extend(configuration_entries)
    fragment = checked_path(cache.get("NEXUS_CONFIG_FILE") or profile["fragment"], source)
    entries.append(("configuration/input.fragment", fragment))
    config_paths["configuration/input.fragment"] = fragment.relative_to(source).as_posix()
    for name in ("CMakeCache.txt", "compile_commands.json"):
        path = checked_path(build / name, source)
        entries.append((f"build/{name}", path))
    validate_compile_commands((build / "compile_commands.json").read_text(encoding="utf-8"),
                              profile, build / "generated", source)
    if profile["platform"] == "native":
        report = checked_path(build / "ctest-results.xml", source)
        validation = test_report(report.read_bytes())
        entries.append(("validation/ctest-results.xml", report))
        log = checked_path(build / "Testing/Temporary/LastTest.log", source, required=False)
        if log.exists():
            entries.append(("validation/LastTest.log", log))
    else:
        validation = {"kind": "cross-compile-only", "hardware_verified": False}
    for filename in ("CMakePresets.json", "README.md", "LICENSE"):
        entries.append((filename, checked_path(source / filename, source)))
    gitmodules = checked_path(source / ".gitmodules", source, required=False)
    if gitmodules.exists():
        entries.append(("configuration/gitmodules", gitmodules))
    toolchain = cache.get("CMAKE_TOOLCHAIN_FILE")
    if toolchain:
        entries.append(("configuration/toolchain.cmake", checked_path(toolchain, source)))
    validate_clean_source(source, build, output, version)
    provenance = {
        "schema_version": 2, **info, "artifact": artifact, "preset": preset,
        "configuration": configuration, "source_worktree_dirty": False,
        "configuration_paths": config_paths, "target": target_identity(profile),
        "validation": validation,
        "cmake": {key: cache[key] for key in CACHE_KEYS if key in cache},
        "submodules": submodules(source, profile["dependencies"]), "compiled_outputs": compiled,
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
        if provenance.get("schema_version") != 2 or provenance.get("target") != target_identity(profile):
            raise ReleaseError("Archive target identity does not match its reviewed support profile")
        contents = {name: bundle.read(f"{artifact}/configuration/{name}").decode("utf-8")
                    for name in CONFIGURATION_FILES}
        config = validate_configuration_bundle(contents)
        validate_effective_build(provenance.get("cmake", {}), profile,
                                 provenance.get("configuration"), config)
        checksums = parse_checksums(bundle.read(f"{artifact}/SHA256SUMS").decode())
        expected_names = {f"{artifact}/{name}" for name in checksums}
        if expected_names != set(names) - {f"{artifact}/SHA256SUMS"}:
            raise ReleaseError("Archive checksum manifest does not cover its contents")
        for name, digest in checksums.items():
            with bundle.open(f"{artifact}/{name}") as handle:
                if hashlib.file_digest(handle, "sha256").hexdigest() != digest:
                    raise ReleaseError("Archive member checksum mismatch")
        files = provenance.get("files", {})
        if set(files) != set(checksums) - {"provenance.json"}:
            raise ReleaseError("Archive provenance file list does not cover its contents")
        for name, record in files.items():
            if record.get("sha256") != checksums[name] or record.get("size") != bundle.getinfo(f"{artifact}/{name}").file_size:
                raise ReleaseError("Archive provenance file identity disagrees with its contents")
        packaged_cache = cmake_cache_contents(bundle.read(f"{artifact}/build/CMakeCache.txt").decode("utf-8"))
        if {key: packaged_cache[key] for key in CACHE_KEYS if key in packaged_cache} != provenance["cmake"]:
            raise ReleaseError("Archive cache does not match build provenance")
        effective_path = provenance.get("configuration_paths", {}).get("configuration/effective.config", "")
        if not effective_path:
            raise ReleaseError("Archive lacks the effective configuration location")
        validate_archive_path(effective_path)
        generated_directory = Path(packaged_cache["CMAKE_HOME_DIRECTORY"]) / Path(effective_path).parent
        validate_compile_commands(bundle.read(f"{artifact}/build/compile_commands.json").decode("utf-8"),
                                  profile, generated_directory)
        if profile["platform"] == "native":
            validation = test_report(bundle.read(f"{artifact}/validation/ctest-results.xml"))
        else:
            validation = {"kind": "cross-compile-only", "hardware_verified": False}
        if provenance.get("validation") != validation:
            raise ReleaseError("Archive validation identity disagrees with its test evidence")
        for name in provenance["compiled_outputs"]:
            member = bundle.getinfo(f"{artifact}/{name}")
            with bundle.open(member) as handle:
                if not compiled_header(handle.read(4096), member.file_size):
                    raise ReleaseError("Archive compiled output has an unrecognized format")
        validate_target_outputs(((name, bundle.read(f"{artifact}/{name}")[:64])
                                 for name in provenance["compiled_outputs"]), profile)
        return provenance


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
    validations = {}
    for artifact, preset in sorted(expected_map.items()):
        archive = checked_path(assets / f"{artifact}-{version}.zip", source)
        sidecar = checked_path(assets / f"{archive.name}.sha256", source)
        actual = sha256_file(archive)
        if parse_checksums(sidecar.read_text(encoding="utf-8")) != {archive.name: actual}:
            raise ReleaseError("Downloaded archive checksum mismatch")
        provenance = verify_bundle(archive, artifact, preset, version, commit)
        validate_dependency_identity(source, provenance.get("submodules", []), release_profile(preset, artifact))
        validations[artifact] = provenance["validation"]
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
            "Native host test reports and ARM cross-compile identities are included where applicable. "
            "Hardware qualification and firmware signing are not provided by this workflow.\n\n"
            "Archives include build configuration, submodule commits, and unsigned provenance. "
            "SHA256SUMS checks transfer integrity; it is not an authenticity signature or "
            "a reproducible-build attestation.\n\n"
            + "".join(
                f"- `{artifact}`: `{preset}`; " + (
                    f"{validations[artifact]['executed']} host tests passed, {validations[artifact]['skipped']} skipped.\n"
                    if validations[artifact]["kind"] == "native-host-tests" else "cross-compiled; HIL pending.\n"
                ) for artifact, preset in sorted(expected_map.items()))
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
