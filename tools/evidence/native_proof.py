"""Retain the maintained Native complete input snapshot for read-only replay.

Live admission uses the maintained snapshot verifier. Portable replay checks
its declared bytes and original metadata receipts; it does not rerun Ninja,
claim launch-time tool capture, or establish independent source authenticity.
"""

import json
import re
import shlex
from pathlib import Path

from scripts.ci import io_lifecycle_matrix as matrix
from tools.evidence.common import EvidenceError, fields, stream_digest


def validate_native_snapshot(inputs, source_root, build):
    """Bind the full existing io-property snapshot to retained named inputs."""
    root, build = Path(source_root), Path(build)
    catalogue = root / "tools/testing/io_lifecycle_matrix.json"
    snapshot = build / "tdd/io-property-inputs.json"
    before = inputs.json(snapshot)
    fields(before, {"schema", "kind", "not_before_ns", "catalogue_sha256",
        "plan_sha256", "compile_commands_sha256", "sources", "compiled",
        "target_compiled", "complete_sources", "objects", "target_contexts",
        "binaries", "compiler_inputs"})
    if (type(before["schema"]) is not int or before["schema"] != 1 or
            before["kind"] != "io-property-preexecution-inputs" or
            type(before["not_before_ns"]) is not int or before["not_before_ns"] <= 0):
        raise EvidenceError("complete Native preexecution snapshot required")
    if not inputs.portable:
        verified = matrix.verify_snapshot(root, build, catalogue, snapshot)
        if verified != before:
            raise EvidenceError("Native snapshot changed during admission")

    def observed(name, sha, *, size=None):
        if not isinstance(sha, str) or re.fullmatch(r"[0-9a-f]{64}", sha) is None:
            raise EvidenceError("invalid Native snapshot file identity")
        path = inputs.path(name, stream=size == 0)
        with path.open("rb") as handle:
            actual = stream_digest(handle)
        if actual != sha or (size is not None and path.stat().st_size != size):
            raise EvidenceError("Native snapshot input identity changed: " + str(name))

    def absolute(name, *, within=None):
        if (not isinstance(name, str) or not Path(name).is_absolute() or
                ".." in Path(name).parts or
                (within is not None and not Path(name).is_relative_to(within))):
            raise EvidenceError("Native snapshot context leaves its declared boundary")
        return Path(name)

    for name, key in ((catalogue, "catalogue_sha256"),
                      (build / "google-contracts.json", "plan_sha256"),
                      (build / "compile_commands.json", "compile_commands_sha256")):
        observed(name, before[key])
    for name in ("scripts/ci/native_contracts.py", "scripts/ci/io_lifecycle_matrix.py"):
        inputs.path(root / name)
    for name, sha in before["sources"].items():
        if (not isinstance(name, str) or Path(name).is_absolute() or
                ".." in Path(name).parts):
            raise EvidenceError("Native catalogue source leaves checkout")
        observed(root / name, sha)
    complete_sources = before["complete_sources"]
    objects = before["objects"]
    binaries = before["binaries"]
    if not complete_sources or not objects or not binaries:
        raise EvidenceError("complete Native compiled snapshot required")
    for name, sha in complete_sources.items():
        path = absolute(name)
        if not path.is_relative_to(root) and not path.is_relative_to(build):
            raise EvidenceError("Native compiled source leaves checkout/build")
        observed(name, sha)
    database = json.loads(inputs.path(build / "compile_commands.json").read_text())
    if not isinstance(database, list) or len(database) > matrix.MAX_CONTEXTS:
        raise EvidenceError("Native compile database exceeds its bound")
    contexts = {}
    for entry in database:
        directory = Path(entry["directory"])
        command = entry.get("arguments") or shlex.split(entry["command"])
        if "-o" not in command:
            continue
        output = Path(command[command.index("-o") + 1])
        output = output if output.is_absolute() else directory / output
        if str(output) not in objects:
            continue
        source = Path(entry["file"])
        source = source if source.is_absolute() else directory / source
        if str(output) in contexts:
            raise EvidenceError("duplicate Native materialized object context")
        contexts[str(output)] = {"source": str(source), "directory": str(directory),
                                "command": command,
                                "sha256": objects[str(output)]["sha256"]}
    if contexts != objects:
        raise EvidenceError("Native snapshot differs from materialized compile contexts")
    if set(complete_sources) != {row["source"] for row in objects.values()}:
        raise EvidenceError("Native complete source context differs from objects")
    for name, row in objects.items():
        fields(row, {"source", "directory", "command", "sha256"})
        absolute(name, within=build)
        absolute(row["directory"], within=build)
        command = row["command"]
        if command.count("-c") != 1 or command.index("-c") + 1 >= len(command):
            raise EvidenceError("Native object has no unique compile source")
        selected = Path(command[command.index("-c") + 1])
        selected = selected if selected.is_absolute() else Path(row["directory"]) / selected
        if str(selected) != row["source"]:
            raise EvidenceError("Native object and compile source disagree")
        observed(name, row["sha256"])
    for name, rows in before["compiled"].items():
        if name not in before["sources"] or not rows:
            raise EvidenceError("Native reviewed source has no materialized context")
        for row in rows:
            fields(row, {"directory", "command", "object", "object_sha256"})
            expected = {"source": str(root / name), "directory": row["directory"],
                        "command": row["command"], "sha256": row["object_sha256"]}
            if objects.get(row["object"]) != expected:
                raise EvidenceError("Native reviewed compiled context changed")
    for target, rows in before["target_compiled"].items():
        if target not in binaries or not rows:
            raise EvidenceError("Native reviewed target has no materialized context")
        for row in rows:
            fields(row, {"source", "source_sha256", "directory", "command",
                         "object", "object_sha256"})
            expected = {"source": row["source"], "directory": row["directory"],
                        "command": row["command"], "sha256": row["object_sha256"]}
            if (objects.get(row["object"]) != expected or
                    complete_sources.get(row["source"]) != row["source_sha256"] or
                    "/CMakeFiles/" + target + ".dir/" not in row["object"]):
                raise EvidenceError("Native target compiled context changed")
    plan = inputs.json(build / "google-contracts.json")
    if ({Path(name).name: name for name in plan["executables"]} !=
            {name: row["path"] for name, row in binaries.items()} or
            set(before["target_contexts"]) != set(binaries)):
        raise EvidenceError("Native binary snapshot differs from executable plan")
    for target, row in binaries.items():
        fields(row, {"path", "sha256"})
        absolute(row["path"], within=build)
        observed(row["path"], row["sha256"])
        actual = sorted(name for name in objects if
                        "/CMakeFiles/" + target + ".dir/" in name)
        if not actual or sorted(before["target_contexts"][target]) != actual:
            raise EvidenceError("Native binary lacks its complete object context")
    compiler = before["compiler_inputs"]
    fields(compiler, {"scope", "files", "archives", "headers"})
    if (compiler["scope"] != "current_ninja_materialized_context_superset" or
            not compiler["files"] or len(compiler["files"]) > matrix.MAX_CONTEXTS):
        raise EvidenceError("complete Native compiler input snapshot required")
    for name, row in compiler["files"].items():
        absolute(name)
        fields(row, {"sha256", "size"})
        if type(row["size"]) is not int or row["size"] < 0:
            raise EvidenceError("invalid Native compiler input size")
        observed(name, row["sha256"], size=row["size"])
    for name in [*compiler["archives"], *compiler["headers"]]:
        if name not in compiler["files"]:
            raise EvidenceError("Native compiler dependency is absent from snapshot")
    for name in ("CMakeCache.txt", "build.ninja", "CMakeFiles/rules.ninja", ".ninja_deps"):
        if str(build / name) not in compiler["files"]:
            raise EvidenceError("Native build input missing from compiler snapshot")
    for stem, suffix in (("input-graph", ["graph", "nexus_google_contracts"]),
                         ("input-dependencies", ["deps"])):
        log = build / "tdd" / (stem + ".txt")
        receipt = inputs.command(log, tool_kind="ninja")
        argv = receipt["command"]
        if (receipt.get("cwd") != str(root) or
                argv != [argv[0], "-C", str(build), "-t", *suffix] or
                argv[0] not in compiler["files"]):
            raise EvidenceError("Native metadata command differs from complete snapshot")
        attempt = inputs.json(log.with_suffix(".command.json"))["attempt_receipt"]
        required = [str(log), str(log.with_suffix(".command.json")), attempt,
                    receipt["stdout"]["path"], receipt["stderr"]["path"]]
        if any(name not in compiler["files"] for name in required):
            raise EvidenceError("Native metadata original receipt missing from snapshot")
    return before
