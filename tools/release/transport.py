#!/usr/bin/env python3
"""Preserve strict consumer bytes across the CI artifact identity boundary."""

import argparse
import configparser
import gzip
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import stat
import sys
import tarfile

MAX_FILES = 100000
MAX_BYTES = 1024 * 1024 * 1024
RAW_OWNER = ".nexus-sdk-consumer-check"
OWNER = ".nexus-sdk-consumer-transport"
ARCHIVE = "consumer-raw.tar.gz"
MANIFEST = "consumer-raw-manifest.json"
FIXTURES = {name + " source/.git" for name in (
    "native", "stm32", "gd32", "h743 baremetal", "h743 freertos",
    "u575 baremetal", "u575 freertos", "f103 baremetal", "f103 freertos",
    "gd303 baremetal", "gd303 freertos", "g474 baremetal", "g474 freertos")}


def commit(value):
    if not isinstance(value, str) or not re.fullmatch(r"[0-9a-f]{40}", value):
        raise ValueError("An exact commit is required")
    return value


def path(value):
    result = Path(value).absolute()
    if ".." in result.parts or any(item.is_symlink() for item in
            (result, *result.parents)):
        raise ValueError("Transport paths must be canonical without symlinks")
    return result


def name(value):
    parsed = PurePosixPath(value)
    if (not value or len(value) > 1024 or parsed.is_absolute() or
            parsed.as_posix() != value or ".." in parsed.parts or
            "\\" in value or any(ord(char) < 32 for char in value)):
        raise ValueError("Unsafe transport member path")
    return value


def checksum(stream):
    result = hashlib.sha256()
    for chunk in iter(lambda: stream.read(1024 * 1024), b""):
        result.update(chunk)
    return result.hexdigest()


def identity(value):
    value = path(value)
    facts = value.stat()
    if not stat.S_ISREG(facts.st_mode) or facts.st_nlink != 1:
        raise ValueError("Transport requires regular files without hardlinks")
    if facts.st_size > MAX_BYTES:
        raise ValueError("Transport file exceeds its bound")
    with value.open("rb") as stream:
        digest = checksum(stream)
    return {"size": facts.st_size, "sha256": digest,
            "mode": stat.S_IMODE(facts.st_mode)}


def byte_identity(value):
    return {key: item for key, item in identity(value).items()
            if key in {"size", "sha256"}}


def scan(root):
    root = path(root)
    if not root.is_dir():
        raise ValueError("Transport input must be a directory")
    files, directories, total = {}, [], 0
    for item in sorted(root.rglob("*")):
        relative = name(item.relative_to(root).as_posix())
        path(item)
        if item.is_dir():
            directories.append(relative)
        else:
            files[relative] = identity(item)
            total += files[relative]["size"]
        if len(files) + len(directories) > MAX_FILES or total > MAX_BYTES:
            raise ValueError("Transport inventory exceeds its bound")
    if not files:
        raise ValueError("Transport input is empty")
    return files, directories


def pairs(items):
    result = {}
    for key, value in items:
        if key in result:
            raise ValueError("Duplicate transport JSON key")
        result[key] = value
    return result


def read(value):
    if path(value).stat().st_size > 32 * 1024 * 1024:
        raise ValueError("Transport JSON exceeds its bound")
    return json.loads(Path(value).read_text(), object_pairs_hook=pairs)


def write(value, data):
    Path(value).write_text(json.dumps(data, sort_keys=True, indent=2) + "\n")


def report(data, expected_commit, files):
    if (not isinstance(data, dict) or
            data.get("kind") != "source_sdk_consumer_verification" or
            data.get("source_revision") != commit(expected_commit) or
            data.get("status") != "passed" or
            data.get("physical_status") != "not_executed" or
            not isinstance(data.get("commands"), list) or not data["commands"]):
        raise ValueError("Passed strict consumer report required for source commit")
    for command in data["commands"]:
        if (not isinstance(command, dict) or
                type(command.get("exit_code")) is not int or
                command["exit_code"] != 0 or
                not isinstance(command.get("argv"), list) or
                not command["argv"] or
                name(command.get("log", "")) not in files):
            raise ValueError("Consumer command lacks successful raw evidence")


def git_metadata(files, directories, contents):
    observed = {entry for entry in directories if entry.endswith("/.git")}
    if observed != FIXTURES:
        raise ValueError("Exactly thirteen empty git-init consumer fixtures required")
    allowed_dirs = {"", "branches", "hooks", "info", "objects",
                    "objects/info", "objects/pack", "refs", "refs/heads", "refs/tags"}
    for entry in [*directories, *files]:
        parts = PurePosixPath(entry).parts
        if ".git" not in parts:
            continue
        index = parts.index(".git")
        fixture = "/".join(parts[:index + 1])
        relative = "/".join(parts[index + 1:])
        if fixture not in FIXTURES:
            raise ValueError("Unexpected Git metadata location")
        if entry in directories:
            if relative not in allowed_dirs:
                raise ValueError("Git fixture contains nonempty history directories")
        elif (relative not in {"HEAD", "config", "description", "info/exclude"}
              and not (relative.startswith("hooks/") and
                       relative.count("/") == 1 and relative.endswith(".sample"))):
            raise ValueError("Git fixture contains history, index or active metadata")
        elif files[entry]["size"] > 65536:
            raise ValueError("Git fixture metadata exceeds its bound")
    for fixture in sorted(FIXTURES):
        for required in ("HEAD", "config", "description", "info/exclude"):
            if fixture + "/" + required not in files:
                raise ValueError("Incomplete git-init fixture metadata")
        config = configparser.ConfigParser(interpolation=None, strict=True)
        config.read_string(contents(fixture + "/config").decode())
        if (config.defaults() or config.sections() != ["core"] or
                dict(config["core"]) != {"repositoryformatversion": "0",
                    "filemode": "true", "bare": "false", "logallrefupdates": "true"}):
            raise ValueError("Git fixture config must contain only git-init core defaults")
        head = contents(fixture + "/HEAD").decode()
        if (not re.fullmatch(r"ref: refs/heads/[A-Za-z0-9][A-Za-z0-9._/-]*\n", head)
                or ".." in head):
            raise ValueError("Git fixture HEAD must be unborn")
        if contents(fixture + "/description") != (
                b"Unnamed repository; edit this file 'description' to name the repository.\n"):
            raise ValueError("Git fixture description differs from git-init defaults")
        if any(line.strip() and not line.lstrip().startswith(b"#")
               for line in contents(fixture + "/info/exclude").splitlines()):
            raise ValueError("Git fixture excludes must remain comments")


def verify(output, expected_commit):
    output = path(output)
    files, directories = scan(output)
    if (directories or set(files) != {OWNER, ARCHIVE, MANIFEST, "verification.json"}
            or (output / OWNER).read_text() != "owned SDK consumer transport\n"):
        raise ValueError("Invalid owned transport output closure")
    manifest = read(output / MANIFEST)
    if (manifest.get("kind") != "source_sdk_consumer_transport" or
            manifest.get("source_commit") != commit(expected_commit) or
            manifest.get("archive") != byte_identity(output / ARCHIVE)):
        raise ValueError("Transport archive identity changed")
    if manifest.get("git_metadata") != "thirteen_unborn_init_fixtures":
        raise ValueError("Transport Git fixture inventory claim changed")
    expected, expected_dirs = manifest["files"], manifest["directories"]
    if (not isinstance(expected, dict) or not isinstance(expected_dirs, list) or
            len(expected) + len(expected_dirs) > MAX_FILES or
            len(set(expected_dirs)) != len(expected_dirs) or
            set(expected) & set(expected_dirs)):
        raise ValueError("Invalid transport manifest inventory")
    seen, observed, observed_dirs, metadata, total = set(), {}, [], {}, 0
    with tarfile.open(output / ARCHIVE, "r:gz") as archive:
        for member in archive:
            entry = name(member.name)
            if entry in seen or len(seen) >= MAX_FILES:
                raise ValueError("Duplicate or excessive transport archive entries")
            seen.add(entry)
            if member.isdir() and entry in expected_dirs:
                observed_dirs.append(entry)
            elif member.isfile() and entry in expected:
                total += member.size
                if total > MAX_BYTES or member.size != expected[entry]["size"]:
                    raise ValueError("Transport archive size exceeds its bound")
                with archive.extractfile(member) as stream:
                    observed[entry] = {"sha256": checksum(stream),
                                      "size": member.size, "mode": member.mode}
                if "/.git/" in entry and not entry.endswith(".sample"):
                    if member.size > 65536:
                        raise ValueError("Git fixture metadata exceeds its bound")
                    metadata[entry] = archive.extractfile(member).read()
            else:
                raise ValueError("Unexpected or nonregular transport archive entry")
    if observed != expected or sorted(observed_dirs) != sorted(expected_dirs):
        raise ValueError("Transport archive differs from complete raw inventory")
    git_metadata(expected, expected_dirs, metadata.__getitem__)
    report(read(output / "verification.json"), expected_commit, expected)
    if byte_identity(output / "verification.json") != {
            key: value for key, value in expected.get("verification.json", {}).items()
            if key in {"size", "sha256"}}:
        raise ValueError("Transport report differs from original raw bytes")
    return manifest


def pack(raw, output, expected_commit):
    raw, output = path(raw), path(output)
    if (output.exists() or output.is_relative_to(raw) or raw.is_relative_to(output)):
        raise ValueError("Transport output must be fresh and separate from raw input")
    files, directories = scan(raw)
    if (RAW_OWNER not in files or
            (raw / RAW_OWNER).read_text() != "owned SDK consumer verification\n"):
        raise ValueError("Raw consumer output lacks its ownership marker")
    report(read(raw / "verification.json"), expected_commit, files)
    git_metadata(files, directories, lambda entry: (raw / entry).read_bytes())
    output.mkdir(parents=True)
    (output / OWNER).write_text("owned SDK consumer transport\n")
    with (output / ARCHIVE).open("xb") as target:
        with gzip.GzipFile(fileobj=target, mode="wb", mtime=0) as compressed:
            with tarfile.open(fileobj=compressed, mode="w|") as archive:
                for entry in sorted([*files, *directories]):
                    member = tarfile.TarInfo(entry)
                    if entry in files:
                        member.size, member.mode = files[entry]["size"], files[entry]["mode"]
                        with (raw / entry).open("rb") as stream:
                            archive.addfile(member, stream)
                    else:
                        member.type, member.mode = tarfile.DIRTYPE, 0o755
                        archive.addfile(member)
    (output / "verification.json").write_bytes((raw / "verification.json").read_bytes())
    (output / "verification.json").chmod(files["verification.json"]["mode"])
    write(output / MANIFEST, {"schema_version": 1,
        "kind": "source_sdk_consumer_transport", "source_commit": expected_commit,
        "archive": byte_identity(output / ARCHIVE), "files": files,
        "directories": directories, "git_metadata": "thirteen_unborn_init_fixtures",
        "raw_bytes_pruned": False})
    if scan(raw) != (files, directories):
        raise ValueError("Raw consumer bytes changed during transport")
    return verify(output, expected_commit)


def attach(output, inputs, source_commit, builder_commit, workflow_ref, run_id, attempt,
           *, source=None):
    output, inputs = path(output), path(inputs)
    commit(source_commit)
    commit(builder_commit)
    if (not re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+/\.github/workflows/"
            r"release\.yml@refs/heads/main", workflow_ref) or
            type(run_id) is not int or run_id <= 0 or
            type(attempt) is not int or attempt <= 0):
        raise ValueError("Exact authorized builder workflow and run required")
    # The versioned helper may execute outside the source checkout. Version
    # policy belongs to the explicit SDK source, never the helper's location.
    source = path(Path.cwd() if source is None else source)
    sys.path.insert(0, str(source))
    from tools.configure.metadata import load_policy
    version = load_policy(source)["semantic_version"]
    sdk_source = inputs / "sdk/delivery/source-sdk/share/nexus/src"
    if load_policy(sdk_source)["semantic_version"] != version:
        raise ValueError("SDK version differs from source policy")
    manifest = read(output / "release-manifest.json")
    if (manifest.get("source_commit") != source_commit or
            manifest.get("tag") != "v" + version or
            manifest.get("source_sdk_run_id") != run_id):
        raise ValueError("Release manifest source/run differs from builder receipt")
    assets = manifest["assets"]
    names = [name(item["name"]) for item in assets]
    if (any("/" in item for item in names) or len(set(names)) != len(names) or
            set(names) | {"release-manifest.json"} !=
            {item.name for item in output.iterdir()} or
            "delivery-workflow.json" in names):
        raise ValueError("Release closure changed before builder receipt")
    for item in assets:
        observed = identity(output / item["name"])
        if any(item[key] != observed[key] for key in ("size", "sha256")):
            raise ValueError("Release asset changed before builder receipt")
    transport = verify(inputs / "sdk/delivery/sdk-consumers", source_commit)
    receipt = output / "delivery-workflow.json"
    write(receipt, {"schema_version": 1, "kind": "source_sdk_delivery_workflow",
        "source_commit": source_commit, "builder_commit": builder_commit,
        "builder_workflow_ref": workflow_ref, "release_run_id": run_id,
        "release_run_attempt": attempt,
        "transport_helper_sha256": identity(Path(__file__))["sha256"],
        "consumer_transport": transport,
        "provenance_scope": "Builder workflow and SDK source identities are recorded separately; "
        "transport provenance does not expand source software or physical qualification.",
        "physical_hil": "not_executed"})
    assets.append({"name": receipt.name, **{key: value for key, value in
        identity(receipt).items() if key in {"size", "sha256"}}})
    manifest["builder_commit"] = builder_commit
    manifest["builder_workflow_ref"] = workflow_ref
    write(output / "release-manifest.json", manifest)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    packer = commands.add_parser("pack")
    packer.add_argument("--raw", type=Path, required=True)
    packer.add_argument("--output", type=Path, required=True)
    packer.add_argument("--expected-commit", required=True)
    attachment = commands.add_parser("attach")
    attachment.add_argument("--source", type=Path, default=Path.cwd())
    attachment.add_argument("--output", type=Path, required=True)
    attachment.add_argument("--inputs", type=Path, required=True)
    attachment.add_argument("--source-commit", required=True)
    attachment.add_argument("--builder-commit", required=True)
    attachment.add_argument("--workflow-ref", required=True)
    attachment.add_argument("--run-id", type=int, required=True)
    attachment.add_argument("--attempt", type=int, required=True)
    args = parser.parse_args()
    try:
        if args.command == "pack":
            pack(args.raw, args.output, args.expected_commit)
        else:
            attach(args.output, args.inputs, args.source_commit, args.builder_commit,
                   args.workflow_ref, args.run_id, args.attempt, source=args.source)
    except (ValueError, OSError, KeyError, TypeError, configparser.Error,
            tarfile.TarError) as error:
        print("Consumer transport rejected: " + str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
