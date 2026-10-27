"""Create independent projects from explicit, immutable external templates."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess

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


def create(templates, revision, template, output, *, source_root=ROOT):
    """Validate first; rollback only the directory exclusively created here."""
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
    compatibility = load_json(
        Path(source_root) / "dependencies/platform-compatibility.json")
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
        "qualification": False,
    }
    # mkdir is exclusive, including when another process creates output late.
    # Validate the actual relocated assembly before returning an origin record.
    output.mkdir()
    try:
        for name, data in payload.items():
            path = output / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        from tools.configure.configure import resolve_ir
        input_paths = {}
        source_root = _path(source_root)
        resolved = resolve_ir(output / "assembly.toml", source_root,
                              input_paths=input_paths)
        for value in input_paths.values():
            input_path = _path(value)
            if not (input_path.is_relative_to(source_root)
                    or input_path.is_relative_to(output)):
                raise ValueError("Template depends on inputs outside the "
                                 "copied project and selected platform")
        result["validation"] = {
            "scope": "current resolver and declared inputs only",
            "source_root": str(source_root),
            "configuration_sha256": resolved.configuration_sha256,
            "inputs": [file_identity(input_paths[item["path"]])
                       for item in resolved.inputs],
            "resolver": file_identity(source_root /
                                      "tools/configure/configure.py"),
        }
        (output / ".nexus-template.json").write_text(
            json.dumps(result, indent=2, sort_keys=True) + "\n")
    except BaseException:
        shutil.rmtree(output)
        raise
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--templates", type=Path, required=True)
    parser.add_argument("--revision", required=True)
    parser.add_argument("--template", required=True)
    parser.add_argument("--output", type=Path, required=True)
    arguments = parser.parse_args(argv)
    result = create(arguments.templates, arguments.revision,
                    arguments.template, arguments.output)
    print("Created " + str(arguments.output) + " from "
          + result["examples_commit"]
          + "; build and run to validate consumption")
    return 0
