"""Create independent projects from explicit, immutable external templates."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import shlex
import shutil
import stat
import subprocess
import sys
import tempfile

from tools.evidence.common import (
    fields, file_identity, load_json, parse_json, sha256_value,
)

ROOT = Path(__file__).resolve().parents[2]
MAXIMUM_FILE_BYTES = 4 * 1024 * 1024
MAXIMUM_PROJECT_BYTES = 16 * 1024 * 1024


def _path(value):
    path = Path(value).absolute()
    if any(item.is_symlink() for item in (path, *path.parents)):
        raise ValueError("Project paths must not traverse symlinks")
    return path.resolve()


def _relative(value):
    if (not isinstance(value, str) or not value or "\\" in value
            or any(ord(character) < 32 for character in value)):
        raise ValueError("Template needs a normal relative file path")
    path = PurePosixPath(value)
    if (path.is_absolute() or str(path) != value
            or any(part in (".", "..", ".git") for part in path.parts)):
        raise ValueError("Template needs a contained relative file path")
    return path


def _git(root, *arguments):
    try:
        return subprocess.check_output(
            ["git", "-C", str(root), *arguments], stderr=subprocess.PIPE,
            timeout=30)
    except (OSError, subprocess.SubprocessError) as error:
        raise ValueError("Cannot inspect pinned template Git source") from error


def _pinned_file(root, revision, relative, *, remaining=None):
    _relative(relative)
    path = _path(root / relative)
    if not path.is_file() or path.stat().st_size > MAXIMUM_FILE_BYTES:
        raise ValueError("Template requires a bounded regular file")
    entry = _git(root, "ls-tree", "-z", revision, "--", relative)
    header, separator, name = entry.partition(b"\t")
    if (not separator or name != relative.encode() + b"\0"
            or header.split()[0] not in (b"100644", b"100755")):
        raise ValueError("Template source must be a tracked regular file")
    blob = header.split()[2].decode("ascii")
    size = int(_git(root, "cat-file", "-s", blob))
    if size > MAXIMUM_FILE_BYTES:
        raise ValueError("Template Git blob exceeds file bound")
    if remaining is not None and size > remaining:
        raise ValueError("Template exceeds the project storage bound")
    data = _git(root, "cat-file", "blob", blob)
    if data != path.read_bytes():
        raise ValueError("Selected template source identity changed")
    return data


def _execute(arguments, directory):
    """Run the selected source's own entry point without imported host modules."""
    try:
        result = subprocess.run(arguments, cwd=directory, capture_output=True,
                                text=True, timeout=120, check=False)
    except (OSError, subprocess.SubprocessError) as error:
        raise ValueError("Selected platform command could not complete") from error
    if result.returncode:
        raise ValueError("Selected platform rejected the project: " +
                         (result.stderr.strip() or result.stdout.strip()))
    return result


def _unchanged(records):
    for record in records:
        if file_identity(_path(record["path"])) != record:
            raise ValueError("Selected platform source changed during creation")


def _selection(source_root, sdk_prefix, interpreter):
    if source_root is not None and sdk_prefix is not None:
        raise ValueError("Platform source and SDK prefix are mutually exclusive")
    prefix = _path(sdk_prefix) if sdk_prefix is not None else None
    source = (_path(prefix / "share/nexus/src") if prefix is not None else
              _path(ROOT if source_root is None else source_root))
    if not source.is_dir():
        raise ValueError("Selected platform source directory is missing")
    selected = {"kind": "sdk" if prefix is not None else "source",
                "source_root": str(source)}
    snapshot = [file_identity(source / "dependencies/platform-compatibility.json"),
                file_identity(source / "tools/configure/configure.py")]
    if prefix is not None or (source / ".nexus-source-sdk.json").is_file():
        manifest = source / ".nexus-source-sdk.json"
        verifier = source / "cmake/package/package_source_sdk.py"
        snapshot.extend((file_identity(manifest), file_identity(verifier)))
        if prefix is not None:
            snapshot.append(file_identity(
                prefix / "lib/cmake/Nexus/NexusConfig.cmake"))
            selected["sdk_prefix"] = str(prefix)
        arguments = [str(interpreter), "-I", "-B", str(verifier),
                     "--verify", str(source)]
        completed = _execute(arguments, source)
        summary, identity = parse_json(completed.stdout.encode()), load_json(manifest)
        if (summary.get("publishable") is not True or
                identity.get("publishable") is not True or
                identity.get("source_dirty") is not False):
            raise ValueError("Development SDK fixtures are not publishable; "
                             "select a publishable SDK prefix")
        if summary.get("source_revision") != identity.get("source_revision"):
            raise ValueError("Selected SDK verification identity changed")
        selected.update({"source_revision": identity["source_revision"],
                         "source_tree": identity["source_tree"],
                         "manifest": file_identity(manifest),
                         "verification": {"argv": arguments,
                                          "verifier": file_identity(verifier)}})
    elif (source / ".git").exists():
        selected.update({
            "source_revision": _git(source, "rev-parse", "HEAD").decode().strip(),
            "source_tree": _git(source, "rev-parse", "HEAD^{tree}").decode().strip(),
            "source_dirty": bool(_git(source, "status", "--porcelain")),
        })
    _unchanged(snapshot)
    return source, selected, snapshot


def _commands(output, source, selected, resolved, *, test_support):
    build = output / "build"
    configure = ["cmake", "-S", str(output), "-B", str(build), "-G", "Ninja",
                 "-DCMAKE_BUILD_TYPE=Debug", "-DNEXUS_BUILD_TESTS=OFF",
                 "-DNEXUS_BUILD_WORKLOAD=OFF"]
    configure.append("-DNEXUS_SOURCE=" + str(source) if selected["kind"] == "source"
                     else "-DNexus_DIR=" + str(Path(selected["sdk_prefix"]) /
                                              "lib/cmake/Nexus"))
    native = resolved["cpu"]["arch"] == "native"
    if native and not test_support:
        raise ValueError("Native projects require explicit test support")
    if test_support:
        configure.append("-DNEXUS_TEST_SUPPORT=ON")
    if not native:
        toolchain = _path(source / "cmake/toolchains/arm-gcc.cmake")
        file_identity(toolchain)
        configure.append("-DCMAKE_TOOLCHAIN_FILE=" + str(toolchain))
    commands = {"configure": configure,
                "build": ["cmake", "--build", str(build), "--parallel", "1"]}
    if native:
        commands["test"] = ["ctest", "--test-dir", str(build),
                            "--output-on-failure", "--no-tests=error"]
    return commands


def _verify_copy(output, payload):
    """Publish origin hashes only for the exact copied regular-file tree."""
    directories = {str(parent) for name in payload
                   for parent in PurePosixPath(name).parents if str(parent) != "."}
    found = set()
    pending = [output]
    while pending:
        for path in pending.pop().iterdir():
            name = path.relative_to(output).as_posix()
            if path.is_symlink():
                raise ValueError("Copied project contains a symlink")
            if path.is_dir():
                if name not in directories:
                    raise ValueError("Copied project contains an unknown directory")
                pending.append(path)
            elif path.is_file() and name in payload:
                if (path.stat().st_size != len(payload[name]) or
                        path.read_bytes() != payload[name]):
                    raise ValueError("Copied project payload changed during creation")
                found.add(name)
            else:
                raise ValueError("Copied project contains an unknown or nonregular file")
    if found != payload.keys():
        raise ValueError("Copied project payload is incomplete")


def _owned_output(output, identity):
    """Observe the original directory without following replacement links."""
    try:
        _path(output)
        current = output.lstat()
    except (OSError, ValueError):
        return False
    return (stat.S_ISDIR(current.st_mode) and
            (current.st_dev, current.st_ino) == identity)


def create(templates, revision, template, output, *, source_root=None,
           sdk_prefix=None, test_support=False):
    """Validate first; rollback only the directory exclusively created here."""
    if type(test_support) is not bool:
        raise ValueError("Project test support must be an explicit boolean")
    root, output = _path(templates), _path(output)
    if (not isinstance(revision, str)
            or not re.fullmatch(r"[0-9a-f]{40}", revision)):
        raise ValueError("Template revision must be an exact commit identity")
    if output.exists():
        raise ValueError("Project output already exists")
    if (root == output or root.is_relative_to(output)
            or output.is_relative_to(root)):
        raise ValueError("Project output must be outside the template checkout")
    head = _git(root, "rev-parse", "HEAD").decode().strip()
    top = Path(_git(root, "rev-parse", "--show-toplevel").decode().strip())
    if head != revision or top != root:
        raise ValueError("Template checkout identity differs from revision")
    if (not isinstance(template, str)
            or not re.fullmatch(r"[a-z0-9][a-z0-9_-]*", template)):
        raise ValueError("Invalid template identifier")
    raw = _pinned_file(root, revision, "templates/manifest.json")
    manifest = parse_json(raw)
    fields(manifest, {"schema", "version", "templates"})
    if (type(manifest["schema"]) is not int or manifest["schema"] != 1
            or type(manifest["version"]) is not int or manifest["version"] != 1
            or not isinstance(manifest["templates"], dict)):
        raise ValueError("Unsupported template manifest")
    selected = manifest["templates"].get(template)
    fields(selected, {"files", "platform"})
    fields(selected["platform"], {"interface_epoch", "assembly_schema"})
    interpreter = Path(sys.executable).resolve()
    interpreter_identity = file_identity(interpreter)
    source_root, selected_platform, source_snapshot = _selection(
        source_root, sdk_prefix, interpreter)
    compatibility = load_json(
        source_root / "dependencies/platform-compatibility.json")
    expected = {
        "interface_epoch": compatibility["factory_abi_epoch"],
        "assembly_schema": compatibility["schemas"]["assembly_authoring"],
    }
    if (selected["platform"] != expected
            or any(type(value) is not int
                   for value in selected["platform"].values())):
        raise ValueError("Template platform interface/schema is incompatible")
    files = selected["files"]
    if (not isinstance(files, dict) or not 3 <= len(files) <= 128
            or not {"main.c", "CMakeLists.txt", "assembly.toml"} <= files.keys()
            ):
        raise ValueError("Template needs a bounded complete project file set")
    payload, used = {}, 0
    for name, expected_digest in sorted(files.items()):
        relative = _relative(name)
        if relative.parts[0] == ".nexus-template.json":
            raise ValueError("Template cannot supply its generated origin")
        sha256_value(expected_digest)
        data = _pinned_file(
            root, revision, "templates/" + template + "/" + name,
            remaining=MAXIMUM_PROJECT_BYTES - used)
        if hashlib.sha256(data).hexdigest() != expected_digest:
            raise ValueError("Template file digest differs from manifest")
        payload[name] = data
        used += len(data)
    lock_data = _pinned_file(root, revision, "dependencies/nexus.lock.json")
    lock = parse_json(lock_data)
    if any(not isinstance(lock.get(key), str) or
           not re.fullmatch(r"[0-9a-f]{40}", lock[key])
           for key in ("commit", "tree")):
        raise ValueError("Template needs an exact platform source lock")
    result = {
        "schema": 1, "kind": "nexus_project_origin", "template": template,
        "examples_commit": revision,
        "examples_tree": _git(root, "rev-parse",
                              "HEAD^{tree}").decode().strip(),
        "manifest_sha256": hashlib.sha256(raw).hexdigest(),
        "expected_platform": {"commit": lock["commit"], "tree": lock["tree"]},
        "platform_lock_sha256": hashlib.sha256(lock_data).hexdigest(),
        "platform_interface": expected, "files": files,
        "selected_platform": selected_platform,
        "qualification": False,
    }
    # mkdir is exclusive, including when another process creates output late.
    # Validate the actual relocated assembly before returning an origin record.
    output.mkdir()
    created = output.lstat()
    output_identity = created.st_dev, created.st_ino
    try:
        if not _owned_output(output, output_identity):
            raise ValueError("Created project root was replaced or its path changed")
        for name, data in payload.items():
            path = output / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        with tempfile.TemporaryDirectory(prefix="nexus project validation ") as temporary:
            bundle = Path(temporary) / "bundle"
            resolver = source_root / "tools/configure/configure.py"
            arguments = [str(interpreter), "-I", "-B", str(resolver), "generate",
                         "--assembly", str(output / "assembly.toml"),
                         "--output", str(bundle), "--source-root", str(source_root)]
            if test_support:
                arguments.append("--test-support")
            _execute(arguments, source_root)
            resolved = load_json(bundle / "resolved.json")
            input_paths = load_json(bundle / "input_paths.json")
            inputs = []
            for value in input_paths.values():
                input_path = _path(value)
                if not (input_path.is_relative_to(source_root)
                        or input_path.is_relative_to(output)):
                    raise ValueError("Template depends on inputs outside the "
                                     "copied project and selected platform")
            for item in resolved["inputs"]:
                record = file_identity(_path(input_paths[item["path"]]))
                if record["sha256"] != item["sha256"]:
                    raise ValueError("Selected platform input changed during creation")
                inputs.append(record)
            sha256_value(resolved["configuration_sha256"])
            result["validation"] = {
                "scope": "current resolver and declared inputs only",
                "source_root": str(source_root), "argv": arguments,
                "configuration_sha256": resolved["configuration_sha256"],
                "inputs": inputs, "resolver": file_identity(resolver),
                "interpreter": interpreter_identity,
            }
            generator = bundle / "generator-identity.json"
            if generator.is_file():
                result["validation"]["generator"] = load_json(generator)
            result["commands"] = _commands(
                output, source_root, selected_platform, resolved,
                test_support=test_support)
        _unchanged([*source_snapshot, interpreter_identity])
        if (_git(root, "rev-parse", "HEAD").decode().strip() != revision or
                (root / "templates/manifest.json").read_bytes() != raw or
                (root / "dependencies/nexus.lock.json").read_bytes() != lock_data or
                any(_path(root / "templates" / template / name).read_bytes() != data
                    for name, data in payload.items())):
            raise ValueError("Selected template source changed during creation")
        if not _owned_output(output, output_identity):
            raise ValueError("Created project root was replaced or its path changed")
        _verify_copy(output, payload)
        if not _owned_output(output, output_identity):
            raise ValueError("Created project root was replaced or its path changed")
        (output / ".nexus-template.json").write_text(
            json.dumps(result, indent=2, sort_keys=True) + "\n")
    except BaseException:
        if _owned_output(output, output_identity):
            try:
                shutil.rmtree(output)
            except OSError:
                # Cleanup failure must not replace the original rejection.
                pass
        raise
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--templates", type=Path, required=True)
    parser.add_argument("--revision", required=True)
    parser.add_argument("--template", required=True)
    parser.add_argument("--output", type=Path, required=True)
    sources = parser.add_mutually_exclusive_group()
    sources.add_argument("--source", type=Path)
    sources.add_argument("--sdk-prefix", type=Path)
    parser.add_argument("--test-support", action="store_true",
                        help="explicitly enable Native test model projects")
    parser.add_argument("--json", action="store_true")
    arguments = parser.parse_args(argv)
    result = create(arguments.templates, arguments.revision,
                    arguments.template, arguments.output,
                    source_root=arguments.source, sdk_prefix=arguments.sdk_prefix,
                    test_support=arguments.test_support)
    if arguments.json:
        print(json.dumps(result, indent=2, sort_keys=True))
        return 0
    print("Created " + str(arguments.output) + " from "
          + result["examples_commit"]
          + "; build and run to validate consumption")
    for name, command in result["commands"].items():
        print(name.capitalize() + ": " + shlex.join(command))
    return 0
